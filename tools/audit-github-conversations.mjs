// Adapted audit procedure from Pealayer docs/REVIEW-COMMENT-AUDIT.md.
// READ-ONLY GitHub requests. Downloaded is never synonymous with reviewed.
import {spawnSync} from 'node:child_process';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {resolve,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url));
export function connection(fetchPage){
  const nodes=[],seen=new Set();let after=null;
  for(;;){
    const page=fetchPage(after);
    if(!Array.isArray(page?.nodes)||typeof page.pageInfo?.hasNextPage!=='boolean')
      throw Error('Incomplete GitHub connection');
    nodes.push(...page.nodes);
    if(!page.pageInfo.hasNextPage)return nodes;
    const cursor=page.pageInfo.endCursor;
    if(!cursor||seen.has(cursor))throw Error('Missing/repeated GitHub cursor');
    seen.add(cursor);after=cursor;
  }
}
export function inspect(record){
  const messages=[...record.comments,...record.reviews,...record.threads.flatMap(t=>t.comments)];
  const copilot=messages.filter(m=>/^(copilot|copilot-pull-request-reviewer|copilot-swe-agent)(\[bot\])?$/i.test(m.author?.login||''));
  const contentHash=createHash('sha256').update(JSON.stringify(record)).digest('hex');
  return {repository:record.repository,number:record.number,kind:record.kind,
    title:record.title,url:record.url,state:record.state,content_sha256:contentHash,
    comments:record.comments.length,reviews:record.reviews.length,threads:record.threads.length,
    unresolved_threads:record.threads.filter(t=>!t.isResolved).map(t=>t.id),
    copilot_messages:copilot.length,
    copilot_quota_notices:copilot.filter(m=>/quota|review limit|rate limit/i.test(m.body||'')).length,
    read_state:'pending',application:'Not evaluated; fetching does not certify an import or resolution.'};
}
export function reviewed(inventory,decisions){
  return inventory.map(row=>{
    const decision=decisions.find(d=>d.repository===row.repository&&d.number===row.number);
    if(!decision)return row;
    if(decision.content_sha256!==row.content_sha256)return {...row,read_state:'stale-review'};
    if(!['full-record','review-comments'].includes(decision.scope)||!decision.application)
      throw Error('Incomplete manual conversation decision');
    return {...row,read_state:decision.scope==='full-record'?'reviewed-record':'reviewed-comment-scope',
      application:decision.application};
  });
}
function gh(args,input){
  const result=spawnSync('gh',args,{cwd:root,encoding:'utf8',input,maxBuffer:64*1024*1024,timeout:60000});
  if(result.error)throw result.error;
  if(result.status!==0)throw Error('GitHub read failed: '+result.stderr);
  const value=JSON.parse(result.stdout);
  if(value.errors?.length)throw Error('GraphQL partial/error response rejected: '+JSON.stringify(value.errors));
  return value;
}
function rest(path){return gh(['api','--method','GET','--paginate','--slurp',path]).flat();}
function graph(query,variables){return gh(['api','graphql','--input','-'],JSON.stringify({query,variables})).data;}
const info='pageInfo{hasNextPage endCursor}';
const comment='databaseId body url author{login}';
const threadFields=`id isResolved isOutdated path line comments(first:100){${info} nodes{${comment}}}`;
const pullFields=`id number reviews(first:100){${info} nodes{${comment} state}} reviewThreads(first:100){${info} nodes{${threadFields}}}`;
export function collect(repository){
  if(!/^[A-Za-z0-9][A-Za-z0-9_-]*\/[A-Za-z0-9][A-Za-z0-9_.-]*$/.test(repository))throw Error('Invalid repository');
  const [owner,name]=repository.split('/');
  const issues=rest('repos/'+repository+'/issues?state=all&sort=created&direction=asc&per_page=100');
  const prs=connection(after=>graph(`query($owner:String!,$name:String!,$after:String){repository(owner:$owner,name:$name){pullRequests(first:20,after:$after,orderBy:{field:CREATED_AT,direction:ASC}){${info} nodes{${pullFields}}}}}`,{owner,name,after}).repository.pullRequests);
  const issueComments=rest('repos/'+repository+'/issues/comments?per_page=100');
  const grouped=new Map();
  for(const c of issueComments){
    const n=Number(c.issue_url.split('/').at(-1));
    if(!grouped.has(n))grouped.set(n,[]);
    grouped.get(n).push({databaseId:c.id,body:c.body,url:c.html_url,author:{login:c.user?.login}});
  }
  const pullMap=new Map(prs.map(p=>[p.number,p]));
  const more=(id,type,field,selection,initial)=>{
    let first=true;
    return connection(after=>{
      if(first){first=false;return initial;}
      return graph(`query($id:ID!,$after:String){node(id:$id){... on ${type}{${field}(first:100,after:$after){${info} nodes{${selection}}}}}}`,{id,after}).node[field];
    });
  };
  const records=issues.map(i=>{
    const p=pullMap.get(i.number);
    if(!!i.pull_request!==!!p)throw Error('Issue/PR enumeration mismatch; refresh rather than silently omit');
    const reviews=p?more(p.id,'PullRequest','reviews',comment+' state',p.reviews):[];
    const threads=p?more(p.id,'PullRequest','reviewThreads',`id isResolved isOutdated path line comments(first:100){${info} nodes{${comment}}}`,p.reviewThreads).map(t=>({
      ...t,comments:more(t.id,'PullRequestReviewThread','comments',comment,t.comments)
    })):[];
    return {repository,number:i.number,kind:p?'pull_request':'issue',title:i.title,
      url:i.html_url,state:i.state,body:i.body,comments:grouped.get(i.number)||[],reviews,threads};
  });
  if(prs.length!==records.filter(r=>r.kind==='pull_request').length)throw Error('Missing PR bodies');
  return records;
}
function save(path,value){
  const text=JSON.stringify(value,null,2)+'\n';
  let prior;try{prior=readFileSync(path,'utf8');}catch(e){if(e.code!=='ENOENT')throw e;}
  if(prior===text)return;
  mkdirSync(dirname(path),{recursive:true});writeFileSync(path,text);
}
// A fresh worktree may have the committed inventory but no ignored raw cache.
// A targeted fetch must not erase other repositories or their review evidence.
export function mergeInventory(previous,current){
  const key=row=>row.repository+'#'+row.number;
  const replacements=new Map(current.map(row=>[key(row),row]));
  const merged=previous.map(row=>{
    const id=key(row),next=replacements.get(id);
    replacements.delete(id);
    return next||row;
  });
  return [...merged,...replacements.values()];
}
export function audit(repositories,cached=false){
  const inventoryPath=resolve(root,'docs/github-conversation-inventory.json');
  let previous=[];
  try{previous=JSON.parse(readFileSync(inventoryPath,'utf8')).records;}
  catch(e){if(e.code!=='ENOENT')throw e;}
  const records=[];
  let prior=[];
  try{prior=JSON.parse(readFileSync(resolve(root,'.build/github-audit/conversations.json'),'utf8'));}
  catch(e){if(e.code!=='ENOENT'||cached)throw e;}
  if(cached)records.push(...prior);
  else for(const repository of repositories){
    const fetched=collect(repository);records.push(...fetched);
    console.log(repository+': '+fetched.length+' complete records fetched; review still pending.');
  }
  if(!cached)records.push(...prior.filter(r=>!repositories.includes(r.repository)));
  // One stable ignored cache, not timestamped copies or another package/cache.
  if(!cached)save(resolve(root,'.build/github-audit/conversations.json'),records);
  let decisions=[];
  try{decisions=JSON.parse(readFileSync(resolve(root,'docs/github-conversation-decisions.json'),'utf8')).records;}
  catch(e){if(e.code!=='ENOENT')throw e;}
  const inventory=mergeInventory(previous,reviewed(records.map(inspect),decisions));
  const included=[...new Set(inventory.map(r=>r.repository))];
  save(inventoryPath,{scope:'All states; fetched records include full paginated bodies, comments, reviews and threads. Unrefreshed records retain their last committed evidence. Only explicit hash-bound decisions attest manual review; fetching never promotes review state.',repositories:included,records:inventory});
  return inventory;
}
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const repositories=process.argv.slice(2).filter(a=>a!=='--cached');
  audit(repositories.length?repositories:['atomicdeploy/PakarController','ToghrolTP/pealayer','atomicdeploy/PCController'],process.argv.includes('--cached'));
}
