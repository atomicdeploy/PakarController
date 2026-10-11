// Adapted from RayanLamp webui/src/api.ts: APIError, Headers, same-origin
// no-store requests, AbortController deadline and finally cleanup. No 401 replay.
// Board/session admission follows PCController internal/link/session.go; the
// boot identity itself comes from Pakar's server, never a hostname/build stamp.
class PeerAPIError extends Error {
  constructor(message, status = 0, code = '') {
    super(message); this.name = 'PeerAPIError'; this.status = status; if (code) this.code = code;
  }
}
function createPeerHttp({fetcher = (...args) => fetch(...args), now = () => Date.now(),
  setTimer = setTimeout, clearTimer = clearTimeout, changed = () => {}} = {}) {
  let owner = null, generation = 0, observed = 0;
  function identity(s) {
    return s?.http_peer_fencing === true && typeof s.board_id === 'string' && typeof s.boot_id === 'string' && /^[a-z0-9_-]{1,64}$/.test(s.board_id) &&
      /^[a-f0-9]{32}$/.test(s.boot_id ?? '') && !/^0{32}$/.test(s.boot_id)
      ? {board: s.board_id, boot: s.boot_id} : null;
  }
  function observe(s) {
    const next = identity(s);
    if (owner?.board !== next?.board || owner?.boot !== next?.boot) {
      owner = next; generation++; changed();
    }
    observed = now();
  }
  function revoke() { owner = null; observed = 0; generation++; changed(); }
  function pin() {
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
      if (peer) {headers.set('X-Pakar-Board', peer.board); headers.set('X-Pakar-Boot', peer.boot);}
      const response = await fetcher(path, {...init, method, headers, credentials: 'same-origin', cache: 'no-store', redirect: 'error', signal: controller.signal});
      const text = await response.text();
      if (controller.signal.aborted) throw new PeerAPIError('Request cancelled.', 0, 'cancelled');
      if (!localAuth && admission !== generation) throw new PeerAPIError('Late reply from a previous board session was ignored.', 409, 'peer_changed');
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
      if (peer) assertOwner(peer);
      return value;
    } catch (error) {
      if (timedOut) throw new PeerAPIError('Request timed out. A sent write may have applied; it was not retried.', 408, 'timeout');
      if (external?.aborted) throw new PeerAPIError('Request cancelled. A sent write may have applied; it was not retried.', 0, 'cancelled');
      throw error;
    } finally {clearTimer(timer); external?.removeEventListener('abort', abort);}
  }
  return {request, pin, assertOwner, observe, revoke, writable: () => {try {pin(); return true;} catch {return false;}}};
}
