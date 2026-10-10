// Reviewable desired state. Read before write; never delete labels or rules.
// Caller supplies a scoped GH_TOKEN; this script never reads or saves credentials.
import {readFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
const desired=JSON.parse(readFileSync(new URL('../.github/repository-settings.json',import.meta.url)));
const labels=JSON.parse(readFileSync(new URL('../.github/labels.json',import.meta.url)));
const apply=process.argv.includes('--apply');
function api(path,method='GET',payload){
  const args=['api',path,'--method',method];
  if(payload)args.push('--input','-');
  const r=spawnSync('gh',args,{input:payload?JSON.stringify(payload):undefined,encoding:'utf8'});
  if(r.error)throw r.error;
  if(r.status!==0)throw Error(`GitHub ${method} ${path} failed: ${r.stderr}`);
  return r.stdout.trim()?JSON.parse(r.stdout):null;
}
const base=`repos/${desired.repository}`;
const repo=api(base),current=api(`${base}/labels?per_page=100`),topics=api(`${base}/topics`).names;
const workflow=api(`${base}/actions/permissions/workflow`);
const changes=Object.fromEntries(Object.entries(desired.settings).filter(([key,value])=>repo[key]!==value));
const missing=labels.filter(label=>!current.some(item=>item.name===label.name&&item.color.toUpperCase()===label.color&&item.description===label.description));
const topicChanged=!desired.topics.every(topic=>topics.includes(topic));
const workflowChanged=Object.entries(desired.workflow_permissions).some(([key,value])=>workflow[key]!==value);
console.log(JSON.stringify({apply,repository:desired.repository,settings:changes,labels:missing,topics:topicChanged?desired.topics:'unchanged',workflowPermissions:workflowChanged?desired.workflow_permissions:'unchanged',preserved:'existing labels, rulesets, branch protections and secret scanning'},null,2));
if(apply){
  if(!repo.permissions?.admin)throw Error('Repository administration permission required');
  if(Object.keys(changes).length)api(base,'PATCH',changes);
  if(topicChanged)api(`${base}/topics`,'PUT',{names:[...new Set([...topics,...desired.topics])]});
  if(workflowChanged)api(`${base}/actions/permissions/workflow`,'PUT',desired.workflow_permissions);
  for(const label of missing){
    const exists=current.some(item=>item.name===label.name);
    api(exists?`${base}/labels/${encodeURIComponent(label.name)}`:`${base}/labels`,exists?'PATCH':'POST',label);
  }
  // These enable protection, never disable existing policies or secret scanning.
  api(`${base}/vulnerability-alerts`,'PUT');
  api(`${base}/automated-security-fixes`,'PUT');
  api(`${base}/private-vulnerability-reporting`,'PUT');
  const verified=api(base),verifiedLabels=api(`${base}/labels?per_page=100`);
  for(const [key,value] of Object.entries(desired.settings))if(verified[key]!==value)throw Error(`Setting readback mismatch: ${key}`);
  for(const label of labels)if(!verifiedLabels.some(item=>item.name===label.name&&item.color.toUpperCase()===label.color&&item.description===label.description))throw Error(`Label readback mismatch: ${label.name}`);
  const verifiedTopics=api(`${base}/topics`).names;
  if(!desired.topics.every(topic=>verifiedTopics.includes(topic)))throw Error('Topics readback mismatch');
  const verifiedWorkflow=api(`${base}/actions/permissions/workflow`);
  if(Object.entries(desired.workflow_permissions).some(([key,value])=>verifiedWorkflow[key]!==value))throw Error('Workflow permissions readback mismatch');
  if(api(`${base}/private-vulnerability-reporting`).enabled!==true)throw Error('Private disclosure readback mismatch');
  if(verified.security_and_analysis?.dependabot_security_updates?.status!=='enabled')throw Error('Dependabot security fixes readback mismatch');
  console.log('Repository settings, labels, topics, read-only workflow defaults and private reporting verified.');
}
