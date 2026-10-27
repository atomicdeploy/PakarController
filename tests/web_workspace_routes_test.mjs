// Execute the actual canonical fragment adapter and existing navigation/apply
// functions with DOM/history doubles. This is not rendered/browser acceptance.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const source=readFileSync(new URL('../assets/web/workspace-studio.js',import.meta.url),'utf8');
const routeSource=source.slice(source.indexOf('function studioRoute('),source.indexOf('function studioClassify('));
const context=vm.createContext({});vm.runInContext(routeSource,context);
const location={hash:'#/diagnostics',pathname:'/panel',search:'?theme=dark'},visits=[],pushes=[];
const history={state:{keep:'draft'},pushState(state,title,url){assert.equal(state,this.state);pushes.push(url);location.hash=url;}};
const route=context.studioRoute({location,history,onNavigate:id=>visits.push(id)});
assert.equal(route.read(),'diagnostics');route.follow();assert.deepEqual(visits,['diagnostics']);
assert.equal(route.write('diagnostics'),false);assert.equal(pushes.length,0,'Current page does not spam history');
assert.equal(route.write('hardware'),true);assert.equal(location.pathname,'/panel');assert.equal(location.search,'?theme=dark');
assert.equal(pushes[0],'#/hardware');assert.equal(route.write('hardware'),false);
location.hash='#/diagnostics';route.follow();assert.equal(visits.at(-1),'diagnostics','Back follows existing history without a write');
location.hash='#/hardware';route.follow();assert.equal(visits.at(-1),'hardware');assert.equal(pushes.length,1);
for(const bad of ['#/network','#/firmware','#/snake','#/Diagnostics','#/diagnostics?actuate=1','#/../api/reset','#//example.test','#/api/command','#diagnostics','#/diagnostics/','%23/diagnostics']){
 location.hash=bad;assert.equal(route.read(),null,bad);const count=visits.length;route.follow();assert.equal(visits.length,count,'Unknown fragments cannot dispatch');
}
for(const bad of ['network','firmware','snake','../controls','https://example.test',undefined,null,{},'hardware\n'])assert.equal(route.write(bad),false);
location.hash='';route.follow();assert.equal(visits.at(-1),'controls','Back to landing restores Controls');
const blocked=context.studioRoute({location,history:{pushState(){throw Error('sandbox');}},onNavigate(){}});
assert.equal(blocked.write('effects'),false,'Unavailable history does not break navigation');
// Run actual navigation against live capability sets without recreating cards.
const navigate=source.split(/\r?\n/).find(line=>line.startsWith(' function navigate('));
const apply=source.slice(source.indexOf(' function apply(){'),source.indexOf(' function navigate('));
const nodes=[{id:'controls',hidden:false,dataset:{panelId:'controls'},classList:{toggle(){}},style:{}},{id:'diagnostics',hidden:true,dataset:{panelId:'diagnostics'},classList:{toggle(){}},style:{}}];
const pageTitle={dataset:{}},content={dataset:{},scrollTop:72};let writes=0;
Object.assign(context,{cards:()=>nodes,panelPreferences:{},studioClassify:c=>c.id,nav:{querySelectorAll:()=>[]},pageTitle,content,
 pages:[['controls','Controls'],['diagnostics','Diagnostics']],settingsAvailable:()=>false,finish(){},layout:{classList:{remove(){}}},
 customize:{setAttribute(){}},customizeLabel:{},copy:(n,text)=>{n.textContent=text},route:{write:()=>writes++}});
vm.runInContext("let selected='controls',requestedPage='diagnostics',editing=false,order=[];"+apply+'\n'+navigate,context);
vm.runInContext('apply()',context);assert.equal(content.dataset.page,'controls');assert.equal(nodes[1].hidden,true,'URL never enables an unacknowledged panel');
nodes[1].hidden=false;vm.runInContext('apply()',context);assert.equal(content.dataset.page,'diagnostics','Linked page resumes when acknowledged panels arrive');
vm.runInContext("navigate('controls')",context);assert.equal(writes,1);assert.equal(content.dataset.page,'controls');assert.equal(content.scrollTop,0);
vm.runInContext("navigate('diagnostics',false)",context);assert.equal(writes,1,'History restoration never writes itself');assert.equal(content.dataset.page,'diagnostics');
nodes[1].hidden=true;vm.runInContext('apply()',context);assert.equal(content.dataset.page,'controls','Lost capability still controls visibility');
assert(source.includes("window.addEventListener('hashchange',route.follow)"));assert(source.includes("window.addEventListener('popstate',route.follow)"));
console.log('Canonical Web workspace routes: allowlist, path/query retention, equal-history skips, Back/Forward, late capability arrival and no hidden-panel enablement passed. Not browser or hardware acceptance.');
