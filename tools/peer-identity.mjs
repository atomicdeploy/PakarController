// Product build identification is descriptive, not an API generation selector.
// All tools share one process session and the normal unversioned device routes.
import {readFileSync} from 'node:fs';
import {randomUUID} from 'node:crypto';
import os from 'node:os';
export const toolRelease=Object.freeze(JSON.parse(readFileSync(new URL('../interfaces/client-release.json',import.meta.url),'utf8')));
export const toolPolicy=Object.freeze(JSON.parse(readFileSync(new URL('../interfaces/peer-policy.json',import.meta.url),'utf8')));
if(toolRelease.product!=='PakarController'||!/^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/.test(toolRelease.version)||!Number.isSafeInteger(toolRelease.build)||toolRelease.build<1)throw Error('Invalid installed tool release identity');
const session=randomUUID();
export function toolHeaders(additional={}) {
  const reserved=new Set(['user-agent','x-pakar-client','x-pakar-version','x-pakar-client-id','x-pakar-build']);
  const retained=Object.fromEntries(Object.entries(additional).filter(([key])=>!reserved.has(key.toLowerCase())));
  return {...retained,'User-Agent':'PakarController/'+toolRelease.version+' (tool; '+os.platform()+'; '+os.arch()+')',
    'X-Pakar-Client':'pakarcontroller-tool','X-Pakar-Version':toolRelease.version,'X-Pakar-Client-Id':session,'X-Pakar-Build':String(toolRelease.build)};
}
export function toolReport() {
  return {id:session,session_id:session,application:'pakarcontroller-tool',version:toolRelease.version,build:String(toolRelease.build),
    os:os.platform(),architecture:os.arch(),hostname:os.hostname().slice(0,64),hardware:{cpu_count:os.availableParallelism(),memory_mb:Math.floor(os.totalmem()/1048576),os_version:os.release().slice(0,96)},
    capabilities:['http','websocket','firmware-upload','peer-version-policy'],settings:{transport:'http'}};
}
export function toolSemver(value) {
  if(typeof value!=='string'||value.length>63)return null;
  const m=/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$/.exec(value);
  if(!m)return null;const n=m.slice(1,4).map(Number),pre=m[4]?.split('.')??[];
  return n.some(v=>!Number.isSafeInteger(v)||v>0xffffffff)||pre.some(s=>/^\d+$/.test(s)&&s.length>1&&s.startsWith('0'))?null:{n,pre};
}
export function toolVersionCompare(left,right) {
  const a=toolSemver(left),b=toolSemver(right);if(!a||!b)return null;
  for(let i=0;i<3;i++)if(a.n[i]!==b.n[i])return a.n[i]<b.n[i]?-1:1;
  if(!a.pre.length||!b.pre.length)return a.pre.length===b.pre.length?0:a.pre.length?-1:1;
  for(let i=0;i<Math.max(a.pre.length,b.pre.length);i++){
    if(a.pre[i]===undefined)return -1;if(b.pre[i]===undefined)return 1;
    const x=a.pre[i],y=b.pre[i];if(x===y)continue;const nx=/^\d+$/.test(x),ny=/^\d+$/.test(y);
    if(nx&&ny)return x.length!==y.length?(x.length<y.length?-1:1):(x<y?-1:1);
    if(nx!==ny)return nx?-1:1;return x<y?-1:1;
  }
  return 0;
}
export function toolVersionEnforced(environment=process.env) {return toolPolicy.enforce&&!['0','false'].includes(String(environment.PAKAR_ENFORCE_PEER_VERSIONS??'').toLowerCase());}
export function peerVersionAllowed(version,minimum,maximum) {
  const lo=toolVersionCompare(version,minimum),hi=maximum==null?-1:toolVersionCompare(version,maximum);
  return lo!==null&&lo>=0&&hi!==null&&hi<0;
}
export function assertToolPeerCompatible(peer,{enforce=toolVersionEnforced()}={}) {
  if(!enforce)return peer;
  if(!peerVersionAllowed(peer?.version,toolPolicy.minimum_board,toolPolicy.maximum_board)||
    (peer?.compatibility?.enforced!==false&&peer?.compatibility?.minimum_app!==undefined&&!peerVersionAllowed(toolRelease.version,peer.compatibility.minimum_app,peer.compatibility.maximum_app)))
    throw Error('Unsupported board/application version; update the incompatible peer before connecting. No command sent.');
  return peer;
}
