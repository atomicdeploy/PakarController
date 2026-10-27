// RayanLamp webui/src/theme.ts and public/theme-init.js (MIT, YektaYar).
// Adapted controller ownership, pre-paint reconciliation and browser metadata.
// Pealayer's actual helpers and shared palettes remain the sole color engine.
// This module has no device requests, peer identity writes or dialog references.
const appearanceDefaults={theme:'dark',color_palette:'studio',accent_color:'pealayer_green',custom_accent_color:null};
function normalizeBrowserAppearance(value){
  const v=value&&typeof value==='object'?value:{};
  return {
    theme:['system','light','dark'].includes(v.theme)?v.theme:appearanceDefaults.theme,
    color_palette:['studio','native'].includes(v.color_palette)?v.color_palette:appearanceDefaults.color_palette,
    accent_color:['system','pealayer_green','windows_blue','macos_blue','custom'].includes(v.accent_color)?v.accent_color:appearanceDefaults.accent_color,
    custom_accent_color:/^#[0-9a-f]{6}$/i.test(v.custom_accent_color??'')?v.custom_accent_color:null
  };
}
let browserAppearance=appearanceDefaults;
try{browserAppearance=normalizeBrowserAppearance(JSON.parse(localStorage.getItem('pakar-appearance')||'null'))}catch{}
let appearanceMedia=null,stopAppearanceController=null;
function browserPrefersLight(){
  try{return globalThis.matchMedia?.('(prefers-color-scheme: light)').matches===true}catch{return false}
}
function applyBrowserAppearance(){
  const root=document.documentElement;
  const scheme=resolvedAppearanceTheme(browserAppearance.theme,appearanceMedia?.matches??browserPrefersLight());
  const name=paletteName(browserAppearance),colors=PEALAYER_PALETTES[name][scheme],accent=resolvedAccent(null,browserAppearance);
  root.dataset.theme=scheme; // Existing resolved-theme CSS contract, not a preference.
  root.dataset.themePreference=browserAppearance.theme;
  root.dataset.resolvedTheme=scheme;
  root.dataset.palette=name;
  root.dataset.accentPreference=browserAppearance.accent_color;
  root.style.colorScheme=scheme;
  for(const[role,value]of Object.entries(colors))root.style.setProperty('--'+role,value);
  root.style.setProperty('--accent',accent);
  root.style.setProperty('--accent-text',accentForeground(accent));
  root.style.setProperty('--accent-soft',`color-mix(in srgb, ${accent} 16%, transparent)`);
  const colorScheme=document.querySelector?.('meta[name="color-scheme"]');
  const themeColor=document.querySelector?.('meta[name="theme-color"]');
  if(colorScheme)colorScheme.content=scheme;
  if(themeColor)themeColor.content=colors.canvas;
  return scheme;
}
function startBrowserAppearanceController(){
  stopAppearanceController?.();
  try{appearanceMedia=globalThis.matchMedia?.('(prefers-color-scheme: light)')??null}catch{appearanceMedia=null}
  const media=appearanceMedia;
  let stopped=false;
  const onChange=()=>{if(!stopped&&browserAppearance.theme==='system')applyBrowserAppearance()};
  const onStorage=event=>{
    if(stopped||!['pakar-appearance',null].includes(event.key))return;
    try{
      const encoded=event.key===null?localStorage.getItem('pakar-appearance'):event.newValue;
      browserAppearance=normalizeBrowserAppearance(JSON.parse(encoded||'null'));
      applyBrowserAppearance();
    }catch{} // Malformed/unavailable storage never breaks the running session.
  };
  media?.addEventListener?.('change',onChange);
  globalThis.addEventListener?.('storage',onStorage);
  const stop=()=>{
    if(stopped)return;
    stopped=true;
    media?.removeEventListener?.('change',onChange);
    globalThis.removeEventListener?.('storage',onStorage);
    if(stopAppearanceController===stop){stopAppearanceController=null;appearanceMedia=null}
  };
  stopAppearanceController=stop;
  applyBrowserAppearance();
  return stop;
}
// Adapt theme-init's locale pre-paint without translating peer names or title.
// Pakar's existing primary-browser-language rule and explicit direction win.
function applyPrepaintLocale(){
  let language='system',direction='auto';
  try{language=localStorage.getItem('pakar-locale')||language;direction=localStorage.getItem('pakar-direction')||direction}catch{}
  if(!['system','en','fa'].includes(language))language='system';
  if(!['auto','ltr','rtl'].includes(direction))direction='auto';
  const resolved=language==='system'
    ? String(globalThis.navigator?.languages?.[0]||globalThis.navigator?.language||'en').toLowerCase().startsWith('fa')?'fa':'en'
    : language;
  const root=document.documentElement;
  root.lang=resolved;
  root.dir=direction==='auto'?(resolved==='fa'?'rtl':'ltr'):direction;
  root.dataset.localePreference=language;
  root.dataset.resolvedLocale=resolved;
}
startBrowserAppearanceController();
applyPrepaintLocale();
