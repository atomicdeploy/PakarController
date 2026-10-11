// Adapted actual PCController web/src/status-frame-batcher.ts generation,
// one scheduled callback, cancelPending/dispose and injected clock contracts.
// LED frames are latest-only: never retain a history/catch-up burst of actuations.
function streamPolicy(fps, queueFrames) {
  if (!Number.isInteger(fps) || fps < 0 || fps > 240 || !Number.isInteger(queueFrames) || queueFrames < 1 || queueFrames > 16)
    throw Error('Choose 0–240 FPS and 1–16 buffered frames. 0 means uncapped.');
  return {interval: fps === 0 ? 0 : 1000 / fps, maxBytes: queueFrames * 1460};
}
function createFrameStream({policy, valid, frame, status = () => {},
  clock = {now: () => performance.now(), request: (fn, delay) => setTimeout(fn, delay), cancel: id => clearTimeout(id)}}) {
  let generation = 0, timer = null, active = false, disposed = false, transport = null;
  let started = 0, ended = 0, reported = 0, sentFrames = 0, sentChunks = 0, skipped = 0, reason = '';
  function snapshot() {
    const elapsed = Math.max(0, (active ? clock.now() : ended) - started);
    return {active, sentFrames, sentChunks, skipped, reason,
      sentFps: elapsed > 0 ? sentFrames * 1000 / elapsed : null,
      bufferedBytes: transport && Number.isFinite(transport.bufferedAmount) ? transport.bufferedAmount : null};
  }
  function publish() {reported = clock.now(); status(snapshot());}
  function stop(message = 'Stopped') {
    const wasActive = active; generation++; active = false; if (wasActive) ended = clock.now();
    if (timer !== null) {clock.cancel(timer); timer = null;}
    reason = message; if (wasActive) publish();
  }
  function allowed() {try {return transport?.readyState === 1 && valid(transport) === true;} catch {return false;}}
  function schedule(delay, owner) {
    timer = clock.request(() => {
      // Check BEFORE clearing the handle: a late cancelled callback must not
      // disarm a newer generation's scheduled frame (stronger than donor).
      if (!active || owner !== generation) return;
      timer = null;
      const frameStarted = clock.now();
      try {
        if (!allowed()) return stop('Stopped: peer, capability or connection changed');
        const p = policy(), limits = streamPolicy(p.fps, p.queueFrames);
        const queued = transport.bufferedAmount;
        if (!Number.isSafeInteger(queued) || queued < 0) return stop('Stopped: socket buffering is unknown');
        if (queued + 1460 > limits.maxBytes) skipped++;
        else {
          const chunks = frame(Math.max(0, clock.now() - started));
          if (!Array.isArray(chunks) || chunks.length !== 2 || !chunks.every(ArrayBuffer.isView) || chunks.reduce((n, b) => n + b.byteLength, 0) !== 1460)
            throw Error('Invalid LED frame');
          for (const chunk of chunks) {
            if (!active || owner !== generation) return;
            if (!allowed()) return stop('Stopped: partial frame outcome unknown; not replayed');
            transport.send(chunk); sentChunks++;
          }
          sentFrames++;
        }
        if (clock.now() - reported >= 1000) publish();
        // No catch-up queue. 0 yields to the browser event loop, not rAF/vsync.
        if (active && owner === generation) schedule(Math.max(0, limits.interval - (clock.now() - frameStarted)), owner);
      } catch {stop('Stopped: send failed or policy invalid; outcome unknown, not replayed');}
    }, delay);
  }
  function start(socket) {
    if (disposed) throw Error('Stream owner disposed');
    stop(); const p = policy(); streamPolicy(p.fps, p.queueFrames);
    transport = socket;
    if (!allowed()) throw Error('Connect to an acknowledged streaming board first');
    started = ended = reported = clock.now(); sentFrames = sentChunks = skipped = 0;
    reason = ''; active = true; const owner = generation; publish();
    if (active && owner === generation) schedule(0, owner);
  }
  function dispose() {if (!disposed) {disposed = true; stop('Stopped: page owner disposed');}}
  return {start, stop, dispose, snapshot, running: () => active};
}
