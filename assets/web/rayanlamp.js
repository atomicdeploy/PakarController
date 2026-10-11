// Actual source adaptations from RayanLamp (MIT, YektaYar):
// webui/src/haptics.ts, connection-preferences.ts, internal/i18n/digits.go.
// License: desktop/licenses/RayanLamp-MIT.txt. No lamp frames enter this module.
function latinDigits(value) {
  return String(value).replace(/[۰-۹٠-٩]/g, digit =>
    String(digit.charCodeAt(0) - (digit >= '۰' ? 0x06f0 : 0x0660)));
}
const fixedHapticPatterns = {
  'command.start':12, 'command.success':[15,35,24],
  'command.error':[28,42,28], 'message.received':[18,55,18]
};
function safeHapticCondition(read, fallback) {
  try { return read(); } catch { return fallback; }
}
class WebHaptics {
  constructor(runtime=browserHapticsRuntime) { this.runtime=runtime; }
  get supported() { return typeof this.runtime.vibrate==='function'; }
  trigger(cue) {
    return Object.hasOwn(fixedHapticPatterns,cue) && this.play(fixedHapticPatterns[cue]);
  }
  audioCue(count) {
    if(!Number.isFinite(count)||count<1)return false;
    const pattern=[];
    for(let index=0;index<Math.min(3,Math.floor(count));index++){
      if(index>0)pattern.push(65);
      pattern.push(20);
    }
    return this.play(pattern);
  }
  play(pattern) {
    const vibrate=this.runtime.vibrate;
    if(typeof vibrate!=='function'||
       !safeHapticCondition(this.runtime.isDocumentVisible,false)||
       safeHapticCondition(this.runtime.prefersReducedMotion,true))return false;
    try { return Boolean(vibrate(typeof pattern==='number'?pattern:Array.from(pattern))); }
    catch { return false; }
  }
}
const browserHapticsRuntime = {
  vibrate:typeof navigator!=='undefined'&&typeof navigator.vibrate==='function'
    ? pattern=>navigator.vibrate(pattern):undefined,
  isDocumentVisible:()=>typeof document!=='undefined'&&document.visibilityState==='visible',
  prefersReducedMotion:()=>typeof window!=='undefined'&&typeof window.matchMedia==='function'&&
    window.matchMedia('(prefers-reduced-motion: reduce)').matches
};
const webHaptics=new WebHaptics();
let webHapticsEnabled=false;
try { webHapticsEnabled=localStorage.getItem('pakar-haptics')==='true'; } catch {}
function saveHaptics() {
  webHapticsEnabled=document.getElementById('uiHaptics').checked;
  try { const value=String(webHapticsEnabled);if(localStorage.getItem('pakar-haptics')!==value)localStorage.setItem('pakar-haptics',value); } catch {}
}
function commandHaptic(cue) { return webHapticsEnabled && webHaptics.trigger(cue); }

// Adapt the donor's validated local event + BroadcastChannel with no echo.
// Deliberately only client presentation preferences, never board drafts/secrets.
function validLocalePreferences(value) {
  return !!value && ['system','en','fa'].includes(value.language) &&
    ['auto','ltr','rtl'].includes(value.direction);
}
function persistLocalePreferences(value) {
  try {
    for(const [key,item] of [['pakar-locale',value.language],['pakar-direction',value.direction]])
      if(localStorage.getItem(key)!==item)localStorage.setItem(key,item);
  } catch {} // In-memory preferences remain usable in storage-restricted browsers.
}
function announceLocalePreferences(value) {
  if(!validLocalePreferences(value))return false;
  persistLocalePreferences(value);
  if(typeof window!=='undefined'&&typeof CustomEvent==='function')
    window.dispatchEvent(new CustomEvent('pakar:locale-preferences',{detail:value}));
  if(typeof BroadcastChannel==='function') {
    let channel;
    try { channel=new BroadcastChannel('pakar-locale');channel.postMessage(value); }
    catch {} finally { try { channel?.close(); } catch {} }
  }
  return true;
}
function subscribeLocalePreferences(callback) {
  if(typeof window==='undefined')return ()=>{};
  const receive=value=>{if(validLocalePreferences(value)){persistLocalePreferences(value);callback(value);}};
  const onLocal=event=>receive(event.detail);
  const onBroadcast=event=>receive(event.data);
  const onStorage=event=>{
    if(!['pakar-locale','pakar-direction',null].includes(event.key))return;
    try { receive({language:localStorage.getItem('pakar-locale')||'system',
      direction:localStorage.getItem('pakar-direction')||'auto'}); } catch {}
  };
  window.addEventListener('pakar:locale-preferences',onLocal);
  window.addEventListener('storage',onStorage);
  let channel;
  try { if(typeof BroadcastChannel==='function'){
    channel=new BroadcastChannel('pakar-locale');channel.addEventListener('message',onBroadcast);
  }} catch {}
  return ()=>{
    window.removeEventListener('pakar:locale-preferences',onLocal);
    window.removeEventListener('storage',onStorage);
    channel?.removeEventListener('message',onBroadcast);channel?.close();
  };
}
