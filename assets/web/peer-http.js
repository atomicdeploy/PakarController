// Adapted from RayanLamp webui/src/api.ts: APIError, Headers, same-origin
// no-store requests, AbortController deadline and finally cleanup. No 401 replay.
// Board/session admission follows PCController internal/link/session.go; the
// boot identity itself comes from Pakar's server, never a hostname/build stamp.
class PeerAPIError extends Error {
  constructor(message, status = 0, code = '') {
    super(message); this.name = 'PeerAPIError'; this.status = status; if (code) this.code = code;
  }
}
// Build identity belongs to this shipped client, never to the board it reads.
// Generators replace this one literal from interfaces/client-release.json.
const PAKAR_WEB_BUILD = {"application":"pakarcontroller-web","version":"0.2.8","build":15,"minimum_board":"0.2.8","maximum_board":null,"enforcePeerVersions":true};
function peerSemver(value) {
  if(typeof value!=='string'||value.length>63)return null;
  const m=/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$/.exec(value);
  if(!m)return null;
  const numbers=m.slice(1,4).map(Number),pre=m[4]?.split('.')??[];
  if(numbers.some(n=>!Number.isSafeInteger(n)||n>0xffffffff)||pre.some(s=>/^\d+$/.test(s)&&s.length>1&&s.startsWith('0')))return null;
  return {numbers,pre};
}
function peerVersionCompare(left,right) {
  const a=peerSemver(left),b=peerSemver(right);if(!a||!b)return null;
  for(let i=0;i<3;i++)if(a.numbers[i]!==b.numbers[i])return a.numbers[i]<b.numbers[i]?-1:1;
  if(!a.pre.length||!b.pre.length)return a.pre.length===b.pre.length?0:a.pre.length?-1:1;
  for(let i=0;i<Math.max(a.pre.length,b.pre.length);i++){
    if(a.pre[i]===undefined)return -1;if(b.pre[i]===undefined)return 1;
    const x=a.pre[i],y=b.pre[i];if(x===y)continue;
    const nx=/^\d+$/.test(x),ny=/^\d+$/.test(y);
    if(nx&&ny)return x.length!==y.length?(x.length<y.length?-1:1):(x<y?-1:1);
    if(nx!==ny)return nx?-1:1;return x<y?-1:1;
  }
  return 0;
}
function peerVersionAllowed(value,minimum,maximum) {
  const lower=peerVersionCompare(value,minimum);
  return lower!==null&&lower>=0&&(maximum==null||(peerVersionCompare(value,maximum)!==null&&peerVersionCompare(value,maximum)<0));
}
let pakarBrowserIdentity;
function browserPeerIdentity() {
  if(pakarBrowserIdentity)return pakarBrowserIdentity;
  let id;try{id=globalThis.sessionStorage?.getItem('pakar-peer-session')}catch{}
  if(!/^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89ab][a-f0-9]{3}-[a-f0-9]{12}$/.test(id??'')){
    const bytes=new Uint8Array(16);
    if(!globalThis.crypto?.getRandomValues)throw new PeerAPIError('Secure session identity is unavailable.',0,'client_identity_unavailable');
    globalThis.crypto.getRandomValues(bytes);bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;
    const hex=Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('');id=[hex.slice(0,8),hex.slice(8,12),hex.slice(12,16),hex.slice(16,20),hex.slice(20)].join('-');
    try{globalThis.sessionStorage?.setItem('pakar-peer-session',id)}catch{}
  }
  return pakarBrowserIdentity=Object.freeze({...PAKAR_WEB_BUILD,id});
}
function createPeerHttp({fetcher = (...args) => fetch(...args), now = () => Date.now(),
  setTimer = setTimeout, clearTimer = clearTimeout, changed = () => {},
  clientIdentity = browserPeerIdentity,
  enforceVersions = globalThis.PAKAR_ENFORCE_PEER_VERSIONS ?? PAKAR_WEB_BUILD.enforcePeerVersions} = {}) {
  let owner = null, generation = 0, observed = 0, versionError = null, serverIdentity = null;
  function acceptVersion(s) {
    serverIdentity={...serverIdentity,application:typeof s?.application==='string'?s.application:typeof s?.application?.id==='string'?s.application.id:null,
      name:typeof s?.application?.name==='string'?s.application.name:null,version:typeof s?.version==='string'?s.version:null,
      board_id:typeof s?.board_id==='string'?s.board_id:null,boot_id:typeof s?.boot_id==='string'?s.boot_id:null};
    if(enforceVersions===false){versionError=null;return;}
    const policy=s?.compatibility,client=clientIdentity();
    if(!peerVersionAllowed(s?.version,PAKAR_WEB_BUILD.minimum_board,PAKAR_WEB_BUILD.maximum_board)||
      (policy?.enforced!==false&&policy?.minimum_app!==undefined&&!peerVersionAllowed(client.version,policy.minimum_app,policy.maximum_app))) {
      versionError=new PeerAPIError('Unsupported board or application version. Update the incompatible peer before connecting.',426,'unsupported_peer_version');
      revoke();throw versionError;
    }
    versionError=null;
  }
  function identity(s) {
    return s?.http_peer_fencing === true && typeof s.board_id === 'string' && typeof s.boot_id === 'string' && /^[a-z0-9_-]{1,64}$/.test(s.board_id) &&
      /^[a-f0-9]{32}$/.test(s.boot_id ?? '') && !/^0{32}$/.test(s.boot_id)
      ? {board: s.board_id, boot: s.boot_id} : null;
  }
  function observe(s) {
    acceptVersion(s);
    const next = identity(s);
    if (owner?.board !== next?.board || owner?.boot !== next?.boot) {
      owner = next; generation++; changed();
    }
    observed = now();
  }
  function revoke() { owner = null; observed = 0; generation++; changed(); }
  function pin() {
    if(versionError)throw versionError;
    const age = now() - observed;
    if (!owner || age < 0 || age >= 8000)
      throw new PeerAPIError('Refresh this board before writing; its boot identity is unavailable or stale.', 409, 'peer_unavailable');
    return Object.freeze({...owner, generation});
  }
  function assertOwner(peer) {
    if (!peer || peer.generation !== generation || peer.board !== owner?.board || peer.boot !== owner?.boot)
      throw new PeerAPIError('Board or boot changed. Old edits were not transferred; refresh before a new action.', 409, 'peer_changed');
  }
  async function request(path, options = {}) {
    if (typeof path !== 'string' || !/^\/(?!\/)/.test(path) || /[\x00-\x20\\]/.test(path))
      throw new PeerAPIError('Only same-origin device paths are allowed.', 400, 'invalid_endpoint');
    const {peer: expected, timeoutMs = 12000, signal: external, ...init} = options;
    const method = String(init.method || 'GET').toUpperCase(), endpoint = path.split('?')[0];
    const mutation = !['GET', 'HEAD'].includes(method), localAuth = ['/login', '/logout'].includes(endpoint);
    const peer = expected || (mutation && !localAuth ? pin() : null);
    if (peer) assertOwner(peer);
    if (mutation && !localAuth) {
      // One read preflight, never a mutation retry. The server closes the race
      // between this observation and admission of the pinned POST/upload.
      await request('/api/status', {signal: external, peer, timeoutMs});
      assertOwner(peer);
    }
    const admission = generation, controller = new AbortController();
    let timedOut = false, timer;
    const abort = () => {controller.abort(); if (timer !== undefined) clearTimer(timer);};
    if (external?.aborted) abort(); else external?.addEventListener('abort', abort, {once: true});
    timer = setTimer(() => {timedOut = true; abort();}, Math.max(1, Math.min(60000, timeoutMs)));
    if (controller.signal.aborted) clearTimer(timer);
    try {
      if (controller.signal.aborted) throw new PeerAPIError('Request cancelled.', 0, 'cancelled');
      const headers = new Headers(init.headers); headers.set('Accept', 'application/json');
      headers.delete('User-Agent');
      const client=clientIdentity();
      // Browsers own User-Agent; do not forge it or copy a board's version.
      headers.set('X-Pakar-Client',client.application);headers.set('X-Pakar-Version',client.version);
      headers.set('X-Pakar-Client-Id',client.id);headers.set('X-Pakar-Build',String(client.build));
      if (peer) {headers.set('X-Pakar-Board', peer.board); headers.set('X-Pakar-Boot', peer.boot);}
      const response = await fetcher(path, {...init, method, headers, credentials: 'same-origin', cache: 'no-store', redirect: 'error', signal: controller.signal});
      const responseHeaders={};for(const key of ['Server','X-Pakar-Application','X-Pakar-Version','X-Pakar-Build','X-Pakar-Board','X-Pakar-Boot','X-Pakar-Board-Version','X-Pakar-Min-App-Version','X-Pakar-Version-Check']){
        const value=response.headers?.get(key);if(value)responseHeaders[key]=value.slice(0,160);
      }
      const text = await response.text();
      if (controller.signal.aborted) throw new PeerAPIError('Request cancelled.', 0, 'cancelled');
      if (!localAuth && admission !== generation) throw new PeerAPIError('Late reply from a previous board session was ignored.', 409, 'peer_changed');
      serverIdentity={...serverIdentity,headers:responseHeaders};
      let value;
      try {value = JSON.parse(text);} catch {
        if (response.headers?.get('Content-Type')?.includes('application/json'))
          throw new PeerAPIError('Device returned malformed JSON.', 502, 'invalid_response');
        value = text; // Existing plain-text firmware receipts remain supported.
      }
      if (!response.ok) {
        if (response.status === 409 && value?.error === 'peer_changed') revoke();
        const reported = typeof value?.error === 'string' ? value.error : value?.error?.code;
        const code = typeof reported === 'string' && /^[a-z][a-z0-9_.-]{0,63}$/.test(reported) ? reported : '';
        throw new PeerAPIError(response.status === 401 ? (endpoint === '/login' ? 'Incorrect username or password' : 'Please sign in again') : 'Device request failed (' + response.status + ').', response.status, code);
      }
      if (endpoint === '/api/status' && method === 'GET') observe(value);
      if (endpoint === '/api/peer/info' && method === 'GET') acceptVersion(value);
      if (peer) assertOwner(peer);
      return value;
    } catch (error) {
      if (timedOut) throw new PeerAPIError(mutation ? 'Request timed out. A sent write may have applied; it was not retried.' : 'Board read timed out. No command was sent.', 408, 'timeout');
      if (external?.aborted) throw new PeerAPIError(mutation ? 'Request cancelled. A sent write may have applied; it was not retried.' : 'Board read cancelled. No command was sent.', 0, 'cancelled');
      throw error;
    } finally {clearTimer(timer); external?.removeEventListener('abort', abort);}
  }
  function webSocketUrl(value) {
    const url=new URL(value),client=clientIdentity();
    url.searchParams.set('client',client.application);url.searchParams.set('version',client.version);
    url.searchParams.set('client_id',client.id);url.searchParams.set('build',String(client.build));
    return url.href;
  }
  function report({settings={}}={}) {
    const client=clientIdentity(),nav=globalThis.navigator??{},display=globalThis.screen??{};
    const text=(v,n)=>typeof v==='string'?v.slice(0,n):undefined;
    const visible={};for(const key of ['language','theme','direction','palette','sound','transport'])if(['string','boolean','number'].includes(typeof settings[key]))visible[key]=typeof settings[key]==='string'?settings[key].slice(0,48):settings[key];
    const value={id:client.id,session_id:client.id,application:client.application,version:client.version,build:String(client.build),
      os:text(nav.userAgentData?.platform??nav.platform,32),user_agent:text(nav.userAgent,256),transport:'http',
      hardware:{cpu_count:Number.isInteger(nav.hardwareConcurrency)&&nav.hardwareConcurrency>0&&nav.hardwareConcurrency<=65535?nav.hardwareConcurrency:undefined,
        memory_mb:Number.isFinite(nav.deviceMemory)&&nav.deviceMemory>0&&nav.deviceMemory<4096?Math.floor(nav.deviceMemory*1024):undefined},
      capabilities:['http','websocket','cell-batch','peer-version-policy'],settings:visible};
    for(const [key,n]of [['screen_width',display.width],['screen_height',display.height]])if(Number.isInteger(n)&&n>0&&n<=65535)value[key]=n;
    return value;
  }
  return {request, pin, assertOwner, observe, revoke, webSocketUrl, report, clientIdentity,
    peerIdentity:()=>serverIdentity?{...serverIdentity}:null,
    writable: () => {try {pin(); return true;} catch {return false;}}};
}
