#pragma once

#include <Arduino.h>

const char kWebUi[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="dark"><meta name="theme-color" content="#080a0e">
<script data-pakar-presentation>
// BEGIN SHARED APPEARANCE
// Actual Pealayer web_ui/src/appearance.ts functions, with TypeScript-only
// annotations removed. Shared preference/resolution semantics are retained.
// Source SHA-256 is recorded in docs/file-comparison-evidence.json.
function mergeAppearance(values, appearance) {
  return {
    ...values,
    theme: appearance.theme,
    color_palette: appearance.color_palette,
    accent_color: appearance.accent_color,
    custom_accent_color: appearance.custom_accent_color ?? null,
  };
}

function resolvedAccent(runtime, config, appearance) {
  switch (config?.accent_color) {
    case 'pealayer_green': return '#38d27a';
    case 'windows_blue': return '#0078d4';
    case 'macos_blue': return '#0a84ff';
    case 'custom': return /^#[0-9a-f]{6}$/i.test(config?.custom_accent_color ?? '')
      ? config.custom_accent_color
      : (runtime?.accentColor ?? '#0078d4');
    default: return appearance?.resolved_accent ?? runtime?.accentColor ?? '#0078d4';
  }
}

function resolvedAppearanceTheme(preference, browserIsLight, appearance) {
  if (preference !== 'system') return preference;
  // Connected clients follow the host. Browser OS preference is offline only.
  return appearance?.resolved_theme ?? (browserIsLight ? 'light' : 'dark');
}

function accentForeground(accent) {
  const match = /^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i.exec(accent);
  if (!match) return '#ffffff';
  const [red, green, blue] = match.slice(1).map((value) => Number.parseInt(value, 16));
  return (red * 299 + green * 587 + blue * 114) > 150_000 ? '#141414' : '#ffffff';
}

function paletteName(config) {
  return config?.color_palette === 'studio' ? 'studio' : 'native';
}
const PEALAYER_PALETTES={"studio":{"dark":{"canvas":"#080a0e","surface-0":"#0d1016","surface-1":"#11161e","surface-2":"#171d27","surface-3":"#1d2531","line":"#29313d","line-strong":"#3a4656","text":"#eef2f7","muted":"#909baa","subtle":"#687382","green":"#38d27a","blue":"#68a7ff","red":"#ff5c68","amber":"#f3b954"},"light":{"canvas":"#edf1f5","surface-0":"#f8fafc","surface-1":"#ffffff","surface-2":"#f2f5f8","surface-3":"#e8edf2","line":"#d5dde6","line-strong":"#b9c5d2","text":"#17202b","muted":"#5e6a78","subtle":"#7a8795","green":"#168a4e","blue":"#286fc5","red":"#d83b48","amber":"#a66a0a"}},"native":{"dark":{"canvas":"#181818","surface-0":"#202020","surface-1":"#242424","surface-2":"#2d2d2d","surface-3":"#373737","line":"#3c3c3c","line-strong":"#606060","text":"#eeeeee","muted":"#aaaaaa","subtle":"#888888","green":"#38d27a","blue":"#68a7ff","red":"#ff5c68","amber":"#f3b954"},"light":{"canvas":"#eeeeee","surface-0":"#f3f3f3","surface-1":"#fafafa","surface-2":"#eeeeee","surface-3":"#e4e4e4","line":"#d5d5d5","line-strong":"#b9b9b9","text":"#232a34","muted":"#5e6a78","subtle":"#7a8795","green":"#168a4e","blue":"#286fc5","red":"#d83b48","amber":"#a66a0a"}}};
// END SHARED APPEARANCE
// BEGIN BROWSER CONTROLLER
// RayanLamp webui/src/theme.ts and public/theme-init.js (MIT, YektaYar).
// Adapted controller ownership, pre-paint reconciliation and browser metadata.
// Pealayer's actual helpers and shared palettes remain the sole color engine.
// This module has no device requests, peer identity writes or dialog references.
const appearanceDefaults={theme:'dark',color_palette:'studio',accent_color:'system',custom_accent_color:null};
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
// END BROWSER CONTROLLER
</script>
<title>PakarController</title><!-- BEGIN SHARED BRAND FAVICON --><link rel="icon" type="image/png" href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAACq0lEQVR42u2av2sUQRTHT2MsJASJdqZKUP8ABRuxEBJ/nFoF0thdzn9ARC18xfG6i6+yCdopEUwE7Y2lBJMQqxSiaGwURBDBGFGjOzAHw2N2du5wYN/e+8K3mZ2Znfe5mdmZm6nVVCqVSqVSqVSqoADJeBiQEJCWAGm1RF4BpAVAmrLtTALABP8akP6W3K1UAFBA8Ma/AWksBYAl5yXfC7qk26DPgXwbLO/7HocAr2c6BQA3sNXOWMux25i5QL5jLO+Vgnqj61EACkABiATQ7jcAY57P2NV+AmB83wPhRj8B2GOXsRzCA0Bas2uOt4C0CEjHqwggBIH7VwbkmjQAdyO7cSyEHUA6LQnAFiBNREI46Qn2OSBtsvRXkgB0IExGALjFyjVt+gAgLbMNzz5JAGIhECsz6jxrsWdHpAEw/gFIZwLlZlj+WUDaa0G8cdK3AWlQCoAdD4SzOeWO2pmeb6d52jNJc8DDrOu/8EA4l1O26I+XLQtK1DpguAsIpsvf8fQc40+AdF7qQsgHYTsnIONTgHTbdvdHgHQzA3NQ+kowD0K9x12gyKVwHoQL/QIgBOFiFQDsZ40x6/tdkRB+9gihNAAagPTFM3u/BKTDXUC4JBHATM6nq+MP2c7wUBcQ6pIAjADS14it7L2cAHwQNiUBaLIGPLFdvm6Xsu4vOxiAsM7qEQNgljXAHe/twC6Oe04qgOusAe74fcqejVQRwKRnB9e2wf9x0t8VBCIWwIBnEvO5UVUANTu2PwaCn48IRDQA4wP2oOMb6/aNyEDEA+h4dwZivGDCqzSAXq0AygxgxXnBhn3p//ZjFkhsucv8PCEFgAUht8SMJ1IAmBISvNlEDaW6Kdqyx1JlDv5EqouS7q2OaXufpyxu2gPYoWRXZVUqlUqlUqlUFdM/lkF4sYxV/wAAAAAASUVORK5CYII="><link rel="apple-touch-icon" href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAACq0lEQVR42u2av2sUQRTHT2MsJASJdqZKUP8ABRuxEBJ/nFoF0thdzn9ARC18xfG6i6+yCdopEUwE7Y2lBJMQqxSiaGwURBDBGFGjOzAHw2N2du5wYN/e+8K3mZ2Znfe5mdmZm6nVVCqVSqVSqVSqoADJeBiQEJCWAGm1RF4BpAVAmrLtTALABP8akP6W3K1UAFBA8Ma/AWksBYAl5yXfC7qk26DPgXwbLO/7HocAr2c6BQA3sNXOWMux25i5QL5jLO+Vgnqj61EACkABiATQ7jcAY57P2NV+AmB83wPhRj8B2GOXsRzCA0Bas2uOt4C0CEjHqwggBIH7VwbkmjQAdyO7cSyEHUA6LQnAFiBNREI46Qn2OSBtsvRXkgB0IExGALjFyjVt+gAgLbMNzz5JAGIhECsz6jxrsWdHpAEw/gFIZwLlZlj+WUDaa0G8cdK3AWlQCoAdD4SzOeWO2pmeb6d52jNJc8DDrOu/8EA4l1O26I+XLQtK1DpguAsIpsvf8fQc40+AdF7qQsgHYTsnIONTgHTbdvdHgHQzA3NQ+kowD0K9x12gyKVwHoQL/QIgBOFiFQDsZ40x6/tdkRB+9gihNAAagPTFM3u/BKTDXUC4JBHATM6nq+MP2c7wUBcQ6pIAjADS14it7L2cAHwQNiUBaLIGPLFdvm6Xsu4vOxiAsM7qEQNgljXAHe/twC6Oe04qgOusAe74fcqejVQRwKRnB9e2wf9x0t8VBCIWwIBnEvO5UVUANTu2PwaCn48IRDQA4wP2oOMb6/aNyEDEA+h4dwZivGDCqzSAXq0AygxgxXnBhn3p//ZjFkhsucv8PCEFgAUht8SMJ1IAmBISvNlEDaW6Kdqyx1JlDv5EqouS7q2OaXufpyxu2gPYoWRXZVUqlUqlUqlUFdM/lkF4sYxV/wAAAAAASUVORK5CYII="><!-- END SHARED BRAND FAVICON --><style>
/* BEGIN PALETTE FALLBACK */
:root{color-scheme:dark;--canvas:#080a0e;--surface-0:#0d1016;--surface-1:#11161e;--surface-2:#171d27;--surface-3:#1d2531;--line:#29313d;--line-strong:#3a4656;--text:#eef2f7;--muted:#909baa;--subtle:#687382;--green:#38d27a;--blue:#68a7ff;--red:#ff5c68;--amber:#f3b954;--accent:#0078d4;--accent-text:#ffffff;--accent-soft:color-mix(in srgb,var(--accent) 16%,transparent);--cyan:var(--blue);--gold:var(--amber)}
/* END PALETTE FALLBACK */
.appearance-toolbar{display:flex;justify-content:flex-end;margin:12px 0}.appearance-fields{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px;margin:20px 0}.appearance-fields label{display:flex;flex-direction:column;gap:7px;font-weight:600}.appearance-fields input,.appearance-fields select{width:100%}.appearance-fields input[type=color]{height:42px;padding:5px}.appearance-fields .hint{font-weight:400;margin:0}.appearance-dialog{width:min(640px,calc(100vw - 32px))}.appearance-dialog footer{border-top:1px solid var(--line);padding-top:16px}@media(max-width:480px){.appearance-fields{grid-template-columns:1fr}}
@font-face{font-family:Vazirmatn;src:url('/assets/fonts/Vazirmatn-Regular.ttf') format('truetype');font-style:normal;font-weight:400;font-display:swap}
*{box-sizing:border-box}body{margin:0;font:14px Inter,"Segoe UI",Vazirmatn,system-ui;background:var(--canvas);color:var(--text)}
html[lang=fa] body{font-family:Vazirmatn,"Segoe UI",system-ui}input,textarea{unicode-bidi:plaintext}html[dir=rtl] .settings-rail button,html[dir=rtl] .cell-menu button{text-align:start}.grid,.segment-display{direction:ltr}
main{max-width:1440px;margin:auto;padding:20px}.top{display:flex;justify-content:space-between;gap:16px;align-items:center;padding:4px 2px 16px;border-bottom:1px solid var(--line)}.brand{display:flex;gap:12px;align-items:center}.brandmark{width:42px;height:42px;display:grid;place-items:center;border-radius:12px;background:linear-gradient(145deg,var(--surface-3),var(--surface-1));border:1px solid var(--line-strong);color:var(--green);font-size:22px}.eyebrow{color:var(--green);letter-spacing:.12em;text-transform:uppercase;font-size:11px}h1{margin:.15rem 0;font-size:24px;letter-spacing:-.03em}h2{font-size:14px;margin:0 0 12px}.status{color:var(--muted)}.layout{display:grid;grid-template-columns:minmax(600px,1fr) 390px;gap:16px;margin-top:16px}.card{background:var(--surface-1);border:1px solid var(--line);border-radius:12px;padding:15px;box-shadow:0 12px 32px #0003}.stack{display:grid;gap:12px}.grid{display:grid;grid-template-columns:repeat(10,minmax(42px,1fr));grid-auto-rows:minmax(42px,1fr);gap:7px;padding:6px 0 28px}.cell:nth-child(10n+1),.cell:nth-child(10n+3),.cell:nth-child(10n+5),.cell:nth-child(10n+7),.cell:nth-child(10n+9){transform:translateY(50%);--cell-surface:var(--surface-0)}.cell{aspect-ratio:1;border:2px solid var(--line-strong);border-radius:10px;background:var(--cell-surface,var(--surface-2));color:var(--text);font-weight:750;cursor:pointer;position:relative;transition:background-color .14s,color .14s,border-color .14s,box-shadow .14s;outline:2px solid transparent;outline-offset:-5px;user-select:none}.cell::before{content:'';position:absolute;left:6px;top:6px;width:5px;height:5px;border-radius:50%;background:var(--text);transition:background-color .14s}.cell.red::before{background:var(--red)}.cell.off::before{background:transparent;border:1px solid var(--subtle)}.cell:focus-visible{outline-color:var(--blue)}.cell:hover{background:var(--surface-3);border-color:var(--blue)}.cell:active{background:var(--surface-3)}
button,select,input{font:inherit;border-radius:8px;border:1px solid var(--line-strong);background:var(--surface-2);color:var(--text);padding:9px 10px}button{cursor:pointer}button:hover{background:var(--surface-3)}button.primary,.active{background:var(--accent);color:var(--accent-text);border-color:transparent}.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}.row>*{flex:1}.metrics{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}.metric{background:var(--surface-0);border:1px solid var(--line);border-radius:9px;padding:9px}.metric span{display:block;color:var(--muted);font-size:11px;text-transform:uppercase;letter-spacing:.05em}.metric b{display:block;margin-top:3px;overflow-wrap:anywhere}.good{color:var(--green)}.bad{color:var(--red)}.warn{color:var(--gold)}.winner{font-size:54px;font-weight:900;color:var(--blue);text-align:center}.switch{display:flex;align-items:center;gap:7px}.switch input{width:auto}.outputs{display:grid;grid-template-columns:repeat(4,1fr);gap:7px}.outputs label{background:var(--surface-0);padding:9px;border-radius:9px;text-align:center}.outputs input{width:auto}.file{width:100%}a{color:var(--blue)}#notice:empty{display:none}#notice{border-left:3px solid var(--amber);padding:8px 10px;background:color-mix(in srgb,var(--surface-1),var(--amber) 12%);margin-top:10px}.section-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:12px;margin-top:12px}.form-grid{display:grid;grid-template-columns:1fr 100px;gap:8px}.form-grid .wide{grid-column:1/-1}.hint{color:var(--muted);font-size:12px;margin:8px 0}.segment-display{display:flex;gap:4px;background:#130c0d;border:1px solid #3c2528;border-radius:9px;padding:8px;margin:8px 0}.segment-display svg{width:36px;height:60px}.segment-display line{stroke:#4d2023;stroke-width:4;stroke-linecap:round}.segment-display line.lit,.segment-display circle.lit{stroke:#ff4a3a;fill:#ff4a3a;filter:drop-shadow(0 0 4px #ff4a3a)}.segment-display circle{fill:#4d2023}
@media(max-width:900px){.layout{grid-template-columns:1fr}.grid{gap:4px}.cell{border-radius:6px;padding:0}.top{align-items:start;flex-direction:column}.section-grid{grid-template-columns:1fr}}
.grid{position:relative;touch-action:pan-y}.table-tools{margin-bottom:8px}.table-tools label{display:flex;align-items:center;gap:6px;font-size:12px}.table-tools input[type=checkbox]{flex:0}.table-tools span{white-space:nowrap}.cell-menu{position:fixed;z-index:30;padding:5px;min-width:190px;max-width:calc(100vw - 16px);background:var(--surface-2);border:1px solid var(--line-strong);border-radius:9px;box-shadow:0 12px 32px #0008}.cell-menu button{display:flex;align-items:center;gap:7px;width:100%;padding:4px 8px;border:0;border-radius:5px;text-align:left;font-size:13px}.cell-menu svg{width:16px;height:16px;fill:none;stroke:currentColor;stroke-width:2}.cell-menu hr{border:0;border-top:1px solid var(--line);margin:4px}.marquee{position:absolute;border:1px solid var(--blue);background:var(--accent-soft);pointer-events:none;border-radius:3px}.manager{color:var(--text);background:var(--surface-1);border:1px solid var(--line-strong);border-radius:14px;padding:20px;width:min(720px,calc(100vw - 32px));max-height:calc(100dvh - 40px);overflow:auto;box-shadow:0 24px 80px #0009}.manager::backdrop{background:#0009;backdrop-filter:blur(3px)}.manager header{display:flex;justify-content:space-between;align-items:center;gap:12px}.manager header h2{margin:0;font-size:20px}.manager-rows{display:grid;gap:4px;margin:14px 0}.manager-row{display:flex;gap:4px;flex-wrap:wrap}.manager-row button{padding:4px 6px;min-width:30px;font-size:12px}.manager-row button[aria-pressed=true]{background:color-mix(in srgb,var(--surface-2),var(--accent) 14%);border-color:var(--accent)}.manager footer{display:flex;justify-content:flex-end;gap:8px;margin-top:16px;flex-wrap:wrap}.no-cell-animation .cell,.no-cell-animation .cell::before{transition:none}@media(prefers-reduced-motion:reduce){.cell,.cell::before{transition:none}}button:focus-visible,input:focus-visible,select:focus-visible{outline:2px solid var(--blue);outline-offset:2px}button:disabled{cursor:not-allowed;opacity:.5}
/* BEGIN CONTROLLER SURFACES */
/* Adapted actual Pealayer web_ui/src/styles.css: semantic shadow, button glyph
   centering, shrinkable content, responsive form stacking and reachable dialogs.
   Controller-specific 80-cell layout replaces the excluded media/timeline layout. */
:root{--shadow:0 24px 70px rgba(0,0,0,.38)}
:root[data-theme=light]{--shadow:0 22px 54px rgba(31,45,61,.14)}
button{display:inline-flex;align-items:center;justify-content:center;vertical-align:middle}
.cell-menu button{justify-content:flex-start}
.card,.manager{box-shadow:var(--shadow)}
.layout>*,.stack,.card,.brand,.brand>div{min-width:0}
.status{overflow-wrap:anywhere}
.row>*{min-width:0;flex:1 1 90px}
.row input,.row select{max-width:100%}
.form-grid{grid-template-columns:minmax(0,1fr) 100px}
.form-grid>input{min-width:0;max-width:100%}
.metrics{grid-template-columns:repeat(2,minmax(0,1fr))}
.table-tools>input{flex-basis:180px}
.manager{overscroll-behavior:contain;scrollbar-gutter:stable}
@media(max-width:560px){
  main{padding:12px}
  .grid{grid-template-columns:repeat(10,minmax(0,1fr));grid-auto-rows:auto}
  .cell{min-width:0;font-size:12px}
  .form-grid{grid-template-columns:minmax(0,1fr) 90px}
  .manager header{align-items:flex-start}
  .manager footer{flex-wrap:wrap;gap:8px}
}
/* END CONTROLLER SURFACES */
/* Fixed manager slots: border/focus changes are paint-only, never layout. */
.editing-order .cell,[data-cell][draggable=true]{cursor:grab;touch-action:none}
.order-source{opacity:.55}.order-before{box-shadow:inset 3px 0 var(--amber)!important}.order-after{box-shadow:inset -3px 0 var(--amber)!important}
.manager-row button{box-sizing:border-box;flex:0 0 32px;width:32px;min-width:32px;height:28px;padding:0;border:2px solid var(--line-strong);transition:background-color .14s,border-color .14s,outline-color .14s}
.manager-row button[data-row]{flex-basis:96px;width:96px;min-width:96px}
.manager-row button[aria-pressed=true],.manager-row button[aria-pressed=true]:hover,.manager-row button[aria-pressed=true]:active{background:var(--surface-2);border-color:var(--amber)}
.manager-row button:focus-visible{outline:1px solid var(--blue);outline-offset:-5px}
.manager-row button[aria-pressed=true]:focus-visible{outline-color:transparent;border-color:var(--amber);background:var(--surface-2)}
.no-cell-animation .manager-row button{transition:none}@media(prefers-reduced-motion:reduce){.manager-row button{transition:none}}
.app-logo{display:block;object-fit:contain;flex-shrink:0}.brandmark{background:none;border:0;padding:0}.brand-login{margin-bottom:12px}
.grid:not(.show-cell-indicators) .cell::before{display:none}.cell.selected,.cell.selected:hover,.cell.selected:active,.cell.selected:focus-visible{border-color:var(--amber);background:var(--cell-surface,var(--surface-2));box-shadow:none;outline-color:transparent}
.pixel-cells .cell,.pixel-cells .cell:hover,.pixel-cells .cell:active,.pixel-cells .cell.selected,.pixel-cells .cell.selected:hover,.pixel-cells .cell.selected:active,.pixel-cells .cell:focus-visible,.pixel-cells .cell.selected:focus-visible{background:transparent;border-color:transparent;outline-color:transparent;box-shadow:none;padding:0}
.table-tools .pixel-help{flex-basis:100%;white-space:normal}
[hidden]{display:none!important}.layout.controller-only{grid-template-columns:1fr}.layout.controller-only>.stack{display:contents}
.pixel-cells .cell::before{display:none}.pixel-face{width:100%;height:100%;display:block;pointer-events:none;overflow:hidden}.pixel-sector{stroke:var(--line-strong);stroke-width:.6}.pixel-outline{fill:none;stroke:var(--line-strong);stroke-width:1;transition:stroke .14s,stroke-width .14s}.cell:hover .pixel-outline,.cell:focus-visible .pixel-outline{stroke:var(--blue);stroke-width:2}.cell.selected .pixel-outline{stroke:var(--amber);stroke-width:2}.pixel-index-badge{fill:#000d}.pixel-index{fill:white;font:9px monospace;text-anchor:middle}.pixel-label{fill:white;font:bold 13px system-ui;text-anchor:middle;paint-order:stroke;stroke:#10151ef0;stroke-width:5;stroke-linejoin:round}.pixel-unknown{fill:var(--muted);font:7px system-ui;text-anchor:middle;paint-order:stroke;stroke:var(--surface-1);stroke-width:3}.no-cell-animation .pixel-outline{transition:none}@media(prefers-reduced-motion:reduce){.pixel-outline{transition:none}}
#nameForm label{display:grid;grid-template-columns:150px minmax(0,1fr);gap:12px;align-items:center}#nameForm input{width:100%;min-width:0;box-sizing:border-box}@media(max-width:600px){#nameForm label{grid-template-columns:1fr;gap:6px}}
.settings-shell{width:min(960px,calc(100vw - 32px));max-height:85vh}.settings-layout{display:grid;grid-template-columns:200px minmax(0,1fr);gap:20px}.settings-rail{display:flex;flex-direction:column;gap:6px;position:sticky;top:0;align-self:start}.settings-rail nav{display:flex;flex-direction:column;gap:4px}.settings-field{display:grid;grid-template-columns:170px minmax(0,1fr);align-items:center;gap:12px;padding:8px 0}.settings-field>span{font-size:13px;color:var(--muted)}.settings-field input,.settings-field select{width:100%;min-width:0;box-sizing:border-box}.settings-field input[type=checkbox]{width:auto;justify-self:start}.settings-fields{margin-bottom:14px}.settings-rail input{width:100%;box-sizing:border-box}.settings-rail button{justify-content:flex-start;text-align:left;border:2px solid transparent}.settings-rail button[aria-current=page]{border-color:var(--accent);background:var(--surface-2)}.settings-body{min-width:0}.settings-body .card{margin-bottom:12px;box-shadow:none}.settings-shell footer{position:sticky;bottom:-1px;background:var(--surface-1);z-index:2}.settings-shell header,.settings-shell footer{flex-wrap:wrap;gap:12px}.settings-shell pre{white-space:pre-wrap;overflow-wrap:anywhere}.settings-shell details{margin-top:16px;padding:12px;border:1px solid var(--line);border-radius:10px}.connection-menu{position:relative}.connection-menu summary{cursor:pointer;list-style:none;padding:8px 12px;border:1px solid var(--line-strong);border-radius:8px}.connection-menu .menu-items{position:absolute;right:0;top:100%;z-index:10;min-width:230px;padding:8px;background:var(--surface-1);border:1px solid var(--line-strong);border-radius:10px;box-shadow:var(--shadow);display:grid;gap:4px}.connection-menu .menu-items button{justify-content:flex-start}.panel-heading{display:flex;align-items:center;justify-content:space-between;gap:8px}.panel-heading h2{margin:0}.panel-heading button{padding:4px 8px}.workspace-panel.collapsed>:not(.panel-heading){display:none}.workspace-panel.collapsed{align-self:start}@media(max-width:640px){.settings-layout{grid-template-columns:1fr}.settings-rail{position:static}.settings-rail nav{display:flex;flex-direction:row;gap:4px;overflow:auto;padding-bottom:8px}.settings-rail button{white-space:nowrap}.settings-shell{padding:14px}.settings-field{grid-template-columns:1fr;gap:4px}.settings-shell footer{position:static}}
.settings-rail,.settings-rail nav,.settings-fields,.settings-field{min-width:0}.settings-field[hidden]{display:none}@media(max-width:640px){.settings-layout{grid-template-columns:minmax(0,1fr)}}
/* BEGIN RAYANLAMP TOAST CSS */
/* RayanLamp app.css toast/viewport/motion/fallback slices; shared Pealayer roles.
   Full donor CSS remains pending: these exact rules are not a whole-file import. */
#notice.toast-region{position:fixed;z-index:100;inset-block-end:20px;inset-inline-end:20px;display:grid;width:min(390px,calc(100vw - 40px));gap:10px;pointer-events:none;border:0;padding:0;margin:0;background:none}
#notice.toast-region:empty{display:none}
.app-toast{display:flex;align-items:center;gap:12px;padding:13px;border:1px solid var(--line-strong);border-radius:15px;background:color-mix(in srgb,var(--surface-2) 92%,transparent);box-shadow:var(--shadow);backdrop-filter:blur(24px) saturate(1.25);animation:toast-enter .24s cubic-bezier(.2,0,0,1) both;pointer-events:auto;min-width:0}
.toast-icon{display:grid;width:32px;height:32px;flex:0 0 auto;place-items:center;border-radius:10px;color:var(--accent);background:color-mix(in srgb,var(--accent) 13%,transparent)}
.toast-success .toast-icon{color:var(--green);background:color-mix(in srgb,var(--green) 13%,transparent)}
.toast-error .toast-icon{color:var(--red);background:color-mix(in srgb,var(--red) 13%,transparent)}
.toast-copy{display:grid;min-width:0;flex:1;align-content:center;gap:2px;overflow-wrap:anywhere}
.toast-copy strong,.toast-copy span{line-height:1.35;white-space:pre-wrap;unicode-bidi:plaintext}
.toast-copy span{color:var(--muted);font-size:13px}
.toast-dismiss{display:grid;place-items:center;flex:0 0 auto;width:30px;height:30px;padding:0;border:1px solid transparent;background:transparent;color:var(--muted);border-radius:7px;transition:color .14s,background .14s,border-color .14s}
.toast-dismiss:hover,.toast-dismiss:active{color:var(--text);background:var(--surface-1);border-color:var(--line-strong)}
.toast-dismiss:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
@media(max-width:680px){#notice.toast-region{inset:auto 10px calc(12px + env(safe-area-inset-bottom)) 10px;width:auto}}
@supports not (backdrop-filter:blur(1px)){.app-toast{background:var(--surface-2)}}
@media(prefers-reduced-motion:reduce){.app-toast,.toast-dismiss{animation:none;transition:none}}
html[data-motion=off] .app-toast,html[data-motion=off] .toast-dismiss{animation:none;transition:none}
@media(forced-colors:active){.app-toast{color:CanvasText;background:Canvas;box-shadow:none;border:1px solid CanvasText;forced-color-adjust:auto}.toast-icon,.toast-copy span,.toast-dismiss{color:CanvasText;background:Canvas}.toast-dismiss:focus-visible{outline-color:Highlight}}
@keyframes toast-enter{from{opacity:0;transform:translateX(20px) scale(.98)}}
[dir=rtl] .app-toast{animation-name:toast-enter-rtl}
@keyframes toast-enter-rtl{from{opacity:0;transform:translateX(-20px) scale(.98)}}

/* END RAYANLAMP TOAST CSS */</style></head><body><main>
<section class="card" id="loginPanel" style="max-width:480px;margin:8vh auto"><div class="brand-login"><!-- BEGIN SHARED BRAND LOGIN --><img class="app-logo" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAACq0lEQVR42u2av2sUQRTHT2MsJASJdqZKUP8ABRuxEBJ/nFoF0thdzn9ARC18xfG6i6+yCdopEUwE7Y2lBJMQqxSiaGwURBDBGFGjOzAHw2N2du5wYN/e+8K3mZ2Znfe5mdmZm6nVVCqVSqVSqVSqoADJeBiQEJCWAGm1RF4BpAVAmrLtTALABP8akP6W3K1UAFBA8Ma/AWksBYAl5yXfC7qk26DPgXwbLO/7HocAr2c6BQA3sNXOWMux25i5QL5jLO+Vgnqj61EACkABiATQ7jcAY57P2NV+AmB83wPhRj8B2GOXsRzCA0Bas2uOt4C0CEjHqwggBIH7VwbkmjQAdyO7cSyEHUA6LQnAFiBNREI46Qn2OSBtsvRXkgB0IExGALjFyjVt+gAgLbMNzz5JAGIhECsz6jxrsWdHpAEw/gFIZwLlZlj+WUDaa0G8cdK3AWlQCoAdD4SzOeWO2pmeb6d52jNJc8DDrOu/8EA4l1O26I+XLQtK1DpguAsIpsvf8fQc40+AdF7qQsgHYTsnIONTgHTbdvdHgHQzA3NQ+kowD0K9x12gyKVwHoQL/QIgBOFiFQDsZ40x6/tdkRB+9gihNAAagPTFM3u/BKTDXUC4JBHATM6nq+MP2c7wUBcQ6pIAjADS14it7L2cAHwQNiUBaLIGPLFdvm6Xsu4vOxiAsM7qEQNgljXAHe/twC6Oe04qgOusAe74fcqejVQRwKRnB9e2wf9x0t8VBCIWwIBnEvO5UVUANTu2PwaCn48IRDQA4wP2oOMb6/aNyEDEA+h4dwZivGDCqzSAXq0AygxgxXnBhn3p//ZjFkhsucv8PCEFgAUht8SMJ1IAmBISvNlEDaW6Kdqyx1JlDv5EqouS7q2OaXufpyxu2gPYoWRXZVUqlUqlUqlUFdM/lkF4sYxV/wAAAAAASUVORK5CYII=" width="42" height="42" alt="PakarController application logo"><!-- END SHARED BRAND LOGIN --></div><div class="eyebrow">PakarController</div><h1>Sign in</h1><p class="hint" id="loginHint">Loading device access settings…</p><form id="loginForm"><div class="stack"><label>Username <input id="loginUser" autocomplete="username" required maxlength="32"></label><label>Password <input id="loginPassword" type="password" autocomplete="current-password" required maxlength="64"></label><button class="primary">Sign in</button></div></form><p class="bad" id="loginError" role="alert"></p><p class="hint">Local HTTP is not encrypted. Use a trusted LAN; do not expose the device directly to the Internet.</p></section>
<div class="appearance-toolbar"><button id="appearanceOpen" type="button" aria-haspopup="dialog">Appearance…</button></div>
<div class="appearance-toolbar row"><label>Language <select id="uiLanguage" onchange="saveLocalization()"><option value="system">System language</option><option value="en">English</option><option value="fa">فارسی</option></select></label><label>Direction <select id="uiDirection" onchange="saveLocalization()"><option value="auto">Automatic</option><option value="ltr">Left to right</option><option value="rtl">Right to left</option></select></label><label><input id="uiHaptics" type="checkbox" onchange="saveHaptics()"> Touch feedback</label></div>
<div id="workspace" hidden>
<div class="top"><div class="brand"><div class="brandmark"><!-- BEGIN SHARED BRAND HEADER --><img class="app-logo" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAACq0lEQVR42u2av2sUQRTHT2MsJASJdqZKUP8ABRuxEBJ/nFoF0thdzn9ARC18xfG6i6+yCdopEUwE7Y2lBJMQqxSiaGwURBDBGFGjOzAHw2N2du5wYN/e+8K3mZ2Znfe5mdmZm6nVVCqVSqVSqVSqoADJeBiQEJCWAGm1RF4BpAVAmrLtTALABP8akP6W3K1UAFBA8Ma/AWksBYAl5yXfC7qk26DPgXwbLO/7HocAr2c6BQA3sNXOWMux25i5QL5jLO+Vgnqj61EACkABiATQ7jcAY57P2NV+AmB83wPhRj8B2GOXsRzCA0Bas2uOt4C0CEjHqwggBIH7VwbkmjQAdyO7cSyEHUA6LQnAFiBNREI46Qn2OSBtsvRXkgB0IExGALjFyjVt+gAgLbMNzz5JAGIhECsz6jxrsWdHpAEw/gFIZwLlZlj+WUDaa0G8cdK3AWlQCoAdD4SzOeWO2pmeb6d52jNJc8DDrOu/8EA4l1O26I+XLQtK1DpguAsIpsvf8fQc40+AdF7qQsgHYTsnIONTgHTbdvdHgHQzA3NQ+kowD0K9x12gyKVwHoQL/QIgBOFiFQDsZ40x6/tdkRB+9gihNAAagPTFM3u/BKTDXUC4JBHATM6nq+MP2c7wUBcQ6pIAjADS14it7L2cAHwQNiUBaLIGPLFdvm6Xsu4vOxiAsM7qEQNgljXAHe/twC6Oe04qgOusAe74fcqejVQRwKRnB9e2wf9x0t8VBCIWwIBnEvO5UVUANTu2PwaCn48IRDQA4wP2oOMb6/aNyEDEA+h4dwZivGDCqzSAXq0AygxgxXnBhn3p//ZjFkhsucv8PCEFgAUht8SMJ1IAmBISvNlEDaW6Kdqyx1JlDv5EqouS7q2OaXufpyxu2gPYoWRXZVUqlUqlUqlUFdM/lkF4sYxV/wAAAAAASUVORK5CYII=" width="42" height="42" alt="PakarController application logo"><!-- END SHARED BRAND HEADER --></div><div><div class="eyebrow">AtomicDeploy control surface</div><h1>PakarController</h1><div class="status" id="summary">Connecting…</div></div></div><div class="row"><button onclick="refresh()">Refresh</button><button type="button" onclick="openDeviceSettings()">Device settings…</button><details class="connection-menu"><summary><span id="wsState">WebSocket —</span> ▾</summary><div class="menu-items"><button type="button" onclick="reconnectWebSocket()">Reconnect WebSocket</button><button type="button" onclick="pauseWebSocket()">Disconnect · use HTTP</button><p class="hint">HTTP status refresh continues. USB transport is managed by the desktop app.</p></div></details><details class="connection-menu"><summary>Panels ▾</summary><div class="menu-items" id="webPanelsMenu"></div></details></div></div>
<div class="row"><button onclick="signOut()">Sign out</button><span id="accessWarning" class="warn"></span></div>
<p id="contextMessage" class="hint" role="status">Waiting for board capabilities…</p><div class="layout"><div class="stack"><section class="card" data-capability="table" hidden><h2>Table</h2><div class="row table-tools"><input id="cellSearch" placeholder="Find cells: 12, 3–8" maxlength="256" aria-label="Find logical cells"><button onclick="selectAllCells()">Select all</button><button onclick="openCellManager()">Manage cells…</button></div><div class="row table-tools"><label><input id="additiveSelection" type="checkbox" onchange="saveTablePreferences()">Toggle without Ctrl</label><label><input id="animateCells" type="checkbox" checked onchange="saveTablePreferences()">Animate cells</label><label><input id="showCellIndicators" type="checkbox" onchange="saveTablePreferences()">LED-state indicators</label><label><input id="editCellOrder" type="checkbox" onchange="setEditingOrder(this.checked)">Edit order</label><button id="resetCellOrderButton" onclick="resetCellOrder()" disabled>Reset order</button><span id="selectionSummary" aria-live="polite">0 selected</span></div><div class="row table-tools"><label>Cell shape <select id="cellShape" onchange="saveTablePreferences()"><option value="rounded">Rounded</option><option value="circle" selected>Circle · six pixels</option><option value="hexagon">Hexagon · six pixels</option></select></label><label><input id="pixelDiagnostics" type="checkbox" onchange="saveTablePreferences()">Physical pixel indices</label><span class="hint pixel-help">Top first · CCW · indices 0–479 · sampled RGB before brightness</span></div><div class="grid" id="grid" role="group" aria-label="80 logical cells"></div><p class="hint" id="bulkStatus" role="status">Click selects · Ctrl toggles · Shift extends · drag selects · Enter manages</p></section>
<div class="section-grid"><section class="card" data-capability="table" hidden><h2>Board modes</h2><div id="boardModes"></div><div class="row"><button onclick="resetGame()">Reset</button><input id="brightness" type="range" min="1" max="255" value="255" oninput="brightnessValue.textContent=value" onchange="setBrightness(value)"><b id="brightnessValue">255</b></div><div class="row" id="snakeControls" hidden><button onclick="snakeDirection('up')">↑</button><button onclick="snakeDirection('left')">←</button><button onclick="snakeDirection('down')">↓</button><button onclick="snakeDirection('right')">→</button></div><div class="outputs" id="outputs" data-capability="outputs" hidden></div></section>
<section class="card" data-capability="display" hidden><h2>Live display</h2><div class="segment-display" id="segmentDisplay" role="img" aria-label="Live TM1637 preview"></div><div class="row"><input id="displayValue" type="number" min="-999" max="9999" value="1"><button onclick="showDisplay()">Show number</button></div></section><section class="card" data-capability="text" hidden><h2>Text</h2>
<input id="displayText" maxlength="40" value="PAKAR CONTROLLER" aria-label="Display text">
<div class="row"><label>Step <input id="displaySpeed" type="number" min="80" max="5000" value="260"> ms</label><label class="switch"><input id="displayScroll" type="checkbox">Scroll short text</label></div>
<div class="row"><label>Repeat <select id="displayRepeat" onchange="displayControls()"><option value="once">Once</option><option value="loop">Loop</option><option value="interval">Interval</option></select></label><label id="displayPauseLabel" hidden>Pause <input id="displayPause" type="number" min="1" max="255" value="5"> s</label><label id="displayHoldLabel">Hold <input id="displayHold" type="number" min="0" max="60000" value="1500"> ms</label></div>
<div class="row"><button onclick="showMarquee()">Present text</button><button id="displayStop" onclick="post('/api/v1/display',{release:true})" hidden>Stop text</button></div>
</section><section class="card" data-capability="buzzer" hidden><h2>Buzzer</h2><div class="row"><input id="frequency" type="number" min="20" max="12000" value="1032"><input id="duration" type="number" min="1" max="5000" value="100"><button onclick="playTone()">Play</button></div><div class="row"><select id="audioCue" disabled aria-label="Available melody"></select><button id="audioPlay" disabled onclick="playCue()">Play cue</button></div></section></div>
</div><aside class="stack"><section class="card" data-capability="telemetry" hidden><h2>Live device</h2><div class="metrics"><div class="metric"><span>AVR</span><b id="avr">—</b></div><div class="metric"><span>TM1637</span><b id="tm">—</b></div><div class="metric"><span>Clients</span><b id="clients">—</b></div><div class="metric"><span>Station</span><b id="station">—</b></div><div class="metric"><span>Recovery AP</span><b id="ap">—</b></div><div class="metric"><span>mDNS / SSDP</span><b id="discovery">—</b></div><div class="metric"><span>NTP</span><b id="ntp">—</b></div><div class="metric"><span>UART errors</span><b id="uart">—</b></div><div class="metric"><span>MQTT</span><b id="mqttState">—</b></div><div class="metric"><span>Cloud WebSocket</span><b id="cloudWsState">—</b></div></div><div class="winner" id="winner">—</div></section>
<section class="card" data-capability="table" hidden><h2>Cell identity</h2><div class="row"><input id="mapLogical" type="number" min="1" max="80" value="1"><input id="mapPhysical" type="number" min="1" max="80" value="73"><input id="mapName" maxlength="15" value="1"></div><button onclick="saveMapping()">Save logical / physical mapping</button></section>
<section class="card" data-capability="table" hidden><h2>Cell action</h2><div class="row"><input id="cellId" type="number" min="1" max="80" value="1"><select id="effect"><option>fade</option><option>immediate</option><option>pulse</option></select></div><div class="row"><button onclick="setCell('white')">White</button><button onclick="setCell('red')">Red</button><button onclick="setCell('off')">Off</button></div></section>
<section class="card" data-capability="leds" hidden><h2>Addressable LEDs</h2><div class="row"><select id="pattern"><option>none</option><option>rainbow</option><option>fire</option></select><button onclick="setPattern()">Apply</button></div><div class="row"><button id="streamStart" onclick="startStream()">Stream rainbow</button><button id="streamStop" onclick="stopStream()" disabled>Stop stream</button></div></section>
<section class="card" data-capability="identity" hidden><h2>Board identity</h2><p id="applicationIdentity" class="hint"></p><form id="nameForm" class="stack"><label>Board display name <input id="boardDisplayName" required maxlength="63" placeholder="Living room table"></label><label>Hostname <input id="deviceName" required maxlength="69" placeholder="ledboard.local"></label><button id="nameSave" disabled>Save board identity</button></form><p class="hint">Application branding comes from the running firmware. Board names are saved across OTA. Only hostname changes restart Wi-Fi/AP; passwords are unchanged.</p><a id="nameResult" hidden></a></section>
<section class="card" data-capability="network" hidden><h2>Wi-Fi</h2><div class="row"><select id="ssid"><option value="">Scan for networks</option></select><button onclick="scanWifi()">Scan</button></div><label>Station SSID <input id="wifiSsid" maxlength="32" placeholder="Enter SSID or choose a scanned network"></label><input id="wifiPassword" type="password" placeholder="Station password (re-enter on save)"><label><input id="wifiOpen" type="checkbox">Open station network · no password</label><div class="row"><label class="switch"><input id="stationEnabled" type="checkbox" checked>Station</label><label class="switch"><input id="apEnabled" type="checkbox" checked>Recovery AP</label><button id="networkSave" onclick="applyNetwork()" disabled>Save &amp; verify Wi-Fi</button></div></section>
<section class="card" data-capability="network" hidden><h2>Cloud links</h2><div class="row"><label class="switch"><input id="mqttEnabled" type="checkbox">MQTT</label><label class="switch"><input id="mqttTls" type="checkbox" checked>TLS</label></div><div class="form-grid"><input id="mqttHost" placeholder="MQTT host"><input id="mqttPort" type="number" min="1" max="65535" value="8883"><input class="wide" id="mqttTopic" placeholder="Base topic"><input id="mqttUser" placeholder="MQTT user"><select id="mqttSecretMode" aria-label="MQTT password handling"><option value="keep">Keep stored password</option><option value="replace">Replace password</option><option value="clear">Clear stored password</option></select><input id="mqttPassword" type="password" placeholder="Replacement password" disabled></div><div class="row"><label class="switch"><input id="cloudWsEnabled" type="checkbox">Outbound WebSocket</label><label class="switch"><input id="cloudWsTls" type="checkbox" checked>TLS</label></div><div class="form-grid"><input id="cloudWsHost" placeholder="WebSocket host"><input id="cloudWsPort" type="number" min="1" max="65535" value="443"><input class="wide" id="cloudWsPath" placeholder="/path"><select id="websocketSecretMode" aria-label="WebSocket token handling"><option value="keep">Keep stored token</option><option value="replace">Replace token</option><option value="clear">Clear stored token</option></select><input class="wide" id="cloudWsToken" type="password" placeholder="Replacement token" disabled><input id="telemetrySeconds" type="number" min="1" max="3600" value="5"><button id="cloudSave" onclick="applyCloud()" disabled>Save &amp; verify cloud</button><button onclick="post('/api/v1/cloud/reconnect')">Reconnect cloud links</button></div><div class="hint">TLS validates endpoints against ISRG Root X1. MQTT commands use &lt;base&gt;/&lt;device&gt;/command; status and availability are retained.</div></section>
<section class="card" data-capability="network" hidden><h2>Device access</h2><form id="accessForm" class="stack"><label>Current password <input id="accessCurrent" type="password" autocomplete="current-password" required></label><label>New username <input id="accessUser" autocomplete="username" required maxlength="32" pattern="[A-Za-z0-9_.-]+"></label><label>New password <input id="accessPassword" type="password" autocomplete="new-password" required minlength="8" maxlength="64"></label><label>Confirm password <input id="accessConfirm" type="password" autocomplete="new-password" required minlength="8" maxlength="64"></label><button>Save credentials</button></form><p class="hint">Saved on the ESP and retained across firmware OTA. Changing credentials signs out all clients and applies to API, WebSocket and Telnet. It does not change the Wi-Fi password.</p></section>
<section class="card" data-capability="ota" hidden><h2>Verified remote update</h2><form id="remoteForm" class="stack"><label>Manifest HTTPS URL <input id="remoteUrl" type="url" maxlength="383" required placeholder="https://firmware.example/PakarController/manifest.json"></label><label>Manifest SHA-256 <input id="remoteHash" pattern="[0-9a-f]{64}" minlength="64" maxlength="64" required></label><label>Endpoint bearer (optional) <input id="remoteBearer" type="password" maxlength="256" autocomplete="off"></label><select id="remoteTargets"><option value="3">Both boards</option><option value="1">ESP (same-version repair)</option><option value="2">AVR (same-version repair)</option></select><button id="remoteStage" disabled>Download &amp; verify only</button></form><div class="row"><button onclick="remoteRefresh()">Refresh job</button><button id="remoteApply" onclick="remoteAction('apply')" disabled>Apply verified job…</button><button id="remoteCancel" onclick="remoteAction('cancel')" disabled>Cancel job</button></div><p class="hint">Exact SHA-256, profile and version checks. Applying preserves and verifies AVR EEPROM. Two-board updates are not atomic across power loss; uncertain outcomes keep Prog armed. No automatic retry.</p><pre id="remoteState" style="white-space:pre-wrap;overflow-wrap:anywhere">No job observed</pre><details><summary>Device &amp; client telemetry</summary><button onclick="loadTelemetry()">Read complete snapshot</button><pre id="telemetryView" style="white-space:pre-wrap;overflow-wrap:anywhere"></pre></details></section>
<section class="card" data-capability="isp" hidden><h2>Firmware vault</h2><input class="file" id="avrFile" type="file" accept=".bin"><div class="row"><button onclick="uploadAvr()">Store AVR image</button><button onclick="api('/api/v1/avr/backup',{method:'POST'})">Back up AVR</button></div><div class="row"><button onclick="programAvr()">Program + verify</button><a href="/api/v1/avr/backup/flash">Flash backup</a><a href="/api/v1/avr/backup/eeprom">EEPROM</a></div><a href="/update">ESP self-OTA</a></section></aside></div></div></main>
<dialog id="appearanceDialog" class="manager appearance-dialog" aria-labelledby="appearanceTitle"><header><div><div class="eyebrow">Preferences · This browser</div><h2 id="appearanceTitle">Appearance</h2></div><button id="appearanceClose" type="button" aria-label="Close appearance">×</button></header><form id="appearanceForm"><div class="appearance-fields"><label>Theme<select id="appearanceTheme"><option value="system">System</option><option value="dark">Dark</option><option value="light">Light</option></select><span class="hint">System follows this browser’s light/dark setting.</span></label><label>Color palette<select id="appearancePalette"><option value="studio">Studio</option><option value="native">Neutral</option></select><span class="hint">Actual shared Pealayer palette definitions.</span></label><label>Accent<select id="appearanceAccent"><option value="system">System / default blue</option><option value="pealayer_green">Pealayer green</option><option value="windows_blue">Windows blue</option><option value="macos_blue">macOS blue</option><option value="custom">Custom</option></select><span class="hint">System uses blue in this standalone device UI.</span></label><label>Custom accent<input id="appearanceCustom" type="color" value="#0078d4"><span class="hint">Button text contrast is selected automatically.</span></label></div><p class="hint">Saved only in this browser. Device configuration and LED colors are unchanged.</p><p id="appearanceError" role="status" class="warn"></p><footer><button id="appearanceCancel" type="button">Cancel</button><button class="primary" type="submit">Apply appearance</button></footer></form></dialog>
<dialog id="deviceSettingsDialog" class="manager settings-shell" aria-labelledby="settingsTitle"><header><div><div class="eyebrow">Device configuration</div><h2 id="settingsTitle">Device settings</h2></div><button type="button" onclick="closeDeviceSettings()" aria-label="Close device settings">×</button></header><div class="settings-layout"><aside class="settings-rail"><input id="settingsSearch" type="search" placeholder="Search settings…" aria-label="Search device settings"><nav id="settingsNavigation" aria-label="Device settings pages"></nav></aside><div class="settings-body"><h3 id="settingsPageTitle">Overview</h3><section id="settingsOverview"><p class="hint">Only fresh, acknowledged device capabilities are shown. Application preferences are separate from board configuration.</p><details><summary>Firmware build information · read-only</summary><p class="hint">UART flow-control mode, pin ownership and hardware layout are build flags, not runtime switches. Rebuild and safely update firmware to change them.</p><pre id="buildInformation">No observed build information</pre></details></section><section id="settingsWorkspaceLinks" hidden><p class="hint">Use the same live workspace controls; settings does not duplicate device editors.</p><div class="row"><button data-capability="table" hidden onclick="settingsReveal('table')">Table &amp; mapping</button><button data-capability="display" hidden onclick="settingsReveal('display')">Live display</button><button data-capability="text" hidden onclick="settingsReveal('text')">Text presentation</button><button data-capability="buzzer" hidden onclick="settingsReveal('buzzer')">Buzzer &amp; melodies</button><button data-capability="telemetry" hidden onclick="settingsReveal('telemetry')">Hardware monitor</button></div></section><div id="settingsPages"></div></div></div><footer><span id="settingsMessage" class="hint" role="status">Read the device to load settings</span><button id="settingsReload" type="button" onclick="settingsReload()">Discard &amp; reload…</button><button type="button" onclick="closeDeviceSettings()">Close</button></footer></dialog>
<div id="cellMenu" class="cell-menu" role="menu" hidden></div>
<div id="notice" class="toast-region" aria-live="polite" aria-atomic="false"></div>
<dialog id="cellManager" class="manager" aria-labelledby="cellManagerTitle"><header><h2 id="cellManagerTitle">Manage cells</h2><button aria-label="Close cell manager" onclick="cellManager.close()">×</button></header><p class="hint">Choose rows or cells, then apply a state and transition to the selection. Ctrl toggles, Shift extends. Selection never changes LEDs.</p><div class="row"><button onclick="selectAllCells()">Select all</button><button onclick="clearCellSelection()">Clear selection</button><b id="managerCount">0 selected</b></div><label class="switch"><input id="managerEditCellOrder" type="checkbox" onchange="setEditingOrder(this.checked)">Edit order · drag before/after a cell; IDs and wiring stay unchanged</label><button id="managerResetCellOrderButton" onclick="resetCellOrder()" disabled>Reset order</button><div id="managerRows" class="manager-rows"></div><div class="row"><label>State <select id="bulkState"><option value="red">Red</option><option value="white">White</option><option value="off">Off</option></select></label><label>Transition <select id="bulkEffect"><option value="fade">Fade</option><option value="immediate">Immediate</option><option value="pulse">Pulse</option></select></label></div><p class="hint">Edits are paced and checked against device readback. A batch is not atomic.</p><footer><button id="bulkCancel" onclick="cancelCellBatch()" disabled>Cancel unsent</button><button id="bulkApply" class="primary" onclick="applyCellBatch(bulkState.value)">Apply to selection</button><button onclick="cellManager.close()">Close</button></footer></dialog>
<script>
// BEGIN RAYANLAMP TOASTS
// Actual RayanLamp store::showToast/dismissToast and ToastRegion/Icon contracts
// (MIT, YektaYar). DOM replaces Preact; no second framework or persistent store.
// Preserve four-item queue/serial IDs/4.5s expiry; add bounded timer ownership,
// focus/hover lifetime, safe text nodes and idempotent teardown.
function createToastController({root, document:doc=globalThis.document,
  translate=(text)=>text, setTimer=globalThis.setTimeout,
  clearTimer=(id)=>globalThis.clearTimeout?.(id)}={}){
  if(!root||!doc)throw Error('Notification region unavailable');
  let toastSerial=0,items=[],stopped=false;
  const timers=new Map(),nodes=new Map(),paused=new Map();
  root.setAttribute('aria-live','polite');root.setAttribute('aria-atomic','false');
  root.setAttribute('tabindex','-1');
  function icon(name,size=20){
    // Exact donor SVG geometry/attributes for the four notification consumers.
    const geometry={check:['m4 12 5 5L20 6'],warning:['M10.3 3.7 2.4 18a2 2 0 0 0 1.75 3h15.7a2 2 0 0 0 1.75-3L13.7 3.7a2 2 0 0 0-3.4 0Z','M12 9v4M12 17h.01'],close:['m5 5 14 14M19 5 5 19'],info:['M12 11v6M12 7h.01']};
    const svg=doc.createElementNS('http://www.w3.org/2000/svg','svg');
    for(const[key,value]of Object.entries({width:size,height:size,viewBox:'0 0 24 24',fill:'none',stroke:'currentColor','stroke-width':'1.75','stroke-linecap':'round','stroke-linejoin':'round','aria-hidden':'true'}))svg.setAttribute(key,String(value));
    if(name==='info'){const circle=doc.createElementNS(svg.namespaceURI,'circle');for(const[key,value]of Object.entries({cx:12,cy:12,r:9}))circle.setAttribute(key,String(value));svg.appendChild(circle)}
    for(const d of geometry[name]){const path=doc.createElementNS(svg.namespaceURI,'path');path.setAttribute('d',d);svg.appendChild(path)}
    return svg;
  }
  function cancel(id){if(timers.has(id)){clearTimer(timers.get(id));timers.delete(id)}}
  function arm(item){cancel(item.id);if(!stopped&&!paused.has(item.id)&&items.includes(item))timers.set(item.id,setTimer(()=>dismissToast(item.id),item.duration))}
  function pause(id,reason){const reasons=paused.get(id)||new Set();reasons.add(reason);paused.set(id,reasons);cancel(id)}
  function resume(item,reason){const reasons=paused.get(item.id);reasons?.delete(reason);if(!reasons?.size)paused.delete(item.id);arm(item)}
  function render(){
    if(stopped)return;
    const focused=items.find(item=>nodes.get(item.id)?.row.contains?.(doc.activeElement));
    const hadFocus=root.contains?.(doc.activeElement);
    const ordered=[];
    for(const item of items){
      let node=nodes.get(item.id);
      if(!node){
        const row=doc.createElement('div'),symbol=doc.createElement('span'),copy=doc.createElement('span'),title=doc.createElement('strong'),message=doc.createElement('span'),dismiss=doc.createElement('button');
        row.className='app-toast toast-'+item.tone;row.setAttribute('role','status');row.dataset.toastId=String(item.id);
        symbol.className='toast-icon';symbol.appendChild(icon(item.tone==='error'?'warning':item.tone==='success'?'check':'info'));
        copy.className='toast-copy';title.textContent=item.title;copy.appendChild(title);
        if(item.message){message.textContent=item.message;copy.appendChild(message)}
        dismiss.type='button';dismiss.className='toast-dismiss';dismiss.appendChild(icon('close',16));
        dismiss.addEventListener('click',()=>dismissToast(item.id));
        row.addEventListener('pointerenter',()=>pause(item.id,'hover'));
        row.addEventListener('pointerleave',()=>resume(item,'hover'));
        row.addEventListener('focusin',()=>pause(item.id,'focus'));
        row.addEventListener('focusout',event=>{if(!row.contains?.(event.relatedTarget))resume(item,'focus')});
        row.addEventListener('keydown',event=>{if(event.key==='Escape'){event.preventDefault();dismissToast(item.id)}});
        row.append(symbol,copy,dismiss);node={row,dismiss};nodes.set(item.id,node);
      }
      node.dismiss.setAttribute('aria-label',translate('Dismiss {title}',{title:item.title}));
      ordered.push(node.row);
    }
    root.replaceChildren(...ordered);
    // Do not steal focus when a non-focused toast expires. Keep keyboard
    // dismissal reachable after removing the focused item or queue eviction.
    if(hadFocus){const destination=(focused&&nodes.get(focused.id)?.dismiss)||nodes.get(items[0]?.id)?.dismiss||root;destination.focus?.({preventScroll:true})}
    for(const id of nodes.keys())if(!items.some(item=>item.id===id))nodes.delete(id);
  }
  function dismissToast(id){
    if(stopped)return;cancel(id);paused.delete(id);
    const next=items.filter(item=>item.id!==id);if(next.length===items.length)return;
    items=next;render();
  }
  function showToast(tone,title,message,duration=4500){
    if(stopped)return null;
    const item={id:++toastSerial,tone:['info','success','error'].includes(tone)?tone:'info',title:String(title??'').slice(0,256),message:String(message??'').slice(0,4096),duration:Number.isFinite(duration)?Math.max(100,Math.min(duration,60000)):4500};
    const evicted=items.slice(0,Math.max(0,items.length-3));
    for(const old of evicted){cancel(old.id);paused.delete(old.id)}
    items=[...items.slice(-3),item];render();arm(item);return item.id;
  }
  function destroy(){if(stopped)return;stopped=true;for(const id of timers.keys())cancel(id);items=[];nodes.clear();paused.clear();root.replaceChildren()}
  return {showToast,dismissToast,refresh:render,destroy,snapshot:()=>items.map(({duration,...item})=>({...item}))};
}
let webToasts=null;

// END RAYANLAMP TOASTS
// BEGIN BROWSER DIAGNOSTICS
// Actual RayanLamp webui/src/browser-console.ts (MIT, YektaYar), adapted.
// Allowlisted operational events only: no global console interception, raw
// responses, frames, request bodies, credentials or persistent log storage.
const browserEventPresentation={
  'app.start':'🚀','transport.ready':'🔌','transport.error':'⚠️',
  'device.command':'⚡','websocket.connected':'📡','websocket.disconnected':'📡',
  'message.received':'💬','audio.cue':'🔊','audio.error':'🔇','device.event':'📟',
  'diagnostics.loaded':'🩺','diagnostics.error':'⚠️','settings.saved':'✅','settings.error':'⚠️'
};
const browserMetadataAllowlist=new Set(['source','action','state','attempt','elapsed_ms','count','endpoint','bytes','error_code']);
// Include camelCase keys missed by the donor's token-boundary-only expression.
const browserSensitiveKey=/(authorization|cookie|credential|csrf|password|passwd|passphrase|secret|session|token|api[_-]?key)/i;
function redactBrowserText(value) {
  const clean=String(value).replace(/[\r\n\t]+/g,' ')
    .replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f-\u009f\u202a-\u202e\u2066-\u2069]/g,'')
    .replace(/(\b[a-z][a-z0-9+.-]*:\/\/)[^/\s:@]+:[^/\s@]+@/gi,'$1[REDACTED]@')
    .replace(/\bBearer\s+[A-Za-z0-9._~+/-]+=*/gi,'Bearer [REDACTED]')
    .replace(/\beyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\b/g,'[REDACTED JWT]')
    .replace(/([?&](?:authorization|csrf|password|secret|session|token|api[_-]?key)=)[^&#\s]*/gi,'$1[REDACTED]');
  return clean.length<=500?clean:clean.slice(0,500)+'…';
}
function browserErrorMetadata(error) {
  if(!(error instanceof Error))return {error_code:'unknown_error'};
  const value=error.code??(Number.isFinite(error.status)?'http_'+error.status:error.name||'Error');
  return {error_code:typeof value==='number'&&Number.isFinite(value)?value:
    typeof value==='string'?redactBrowserText(value):'unknown_error'};
}
function sanitizeBrowserValue(value,seen=new WeakSet(),depth=0) {
  if(value===null||typeof value==='boolean')return value;
  if(typeof value==='number')return Number.isFinite(value)?value:'[Nonfinite number]';
  if(typeof value==='string')return redactBrowserText(value);
  if(typeof value==='bigint')return redactBrowserText(value+'n');
  if(typeof value==='undefined')return undefined;
  if(typeof value==='symbol')return redactBrowserText(value.description??'symbol');
  if(typeof value==='function')return '[Function]';
  if(value instanceof Error)return browserErrorMetadata(value);
  if(depth>=4)return '[Maximum depth]';
  if(seen.has(value))return '[Circular]';
  seen.add(value);
  try {
    if(Array.isArray(value)){
      const result=value.slice(0,20).map(v=>sanitizeBrowserValue(v,seen,depth+1));
      if(value.length>20)result.push('['+(value.length-20)+' more items]');
      return result;
    }
    if(ArrayBuffer.isView(value))return '[Typed buffer '+value.byteLength+' bytes]';
    if(value instanceof ArrayBuffer)return '[ArrayBuffer '+value.byteLength+' bytes]';
    if(value instanceof Date)return Number.isFinite(value.getTime())?value.toISOString():'[Invalid date]';
    if(typeof Element!=='undefined'&&value instanceof Element)return '<'+redactBrowserText(value.tagName.toLowerCase())+'>';
    const result=Object.create(null),entries=Object.entries(value);
    for(const[key,item]of entries.slice(0,30)){
      if(key==='__proto__'||key==='constructor'||key==='prototype')continue;
      result[redactBrowserText(key)]=browserSensitiveKey.test(key)?'[REDACTED]':sanitizeBrowserValue(item,seen,depth+1);
    }
    if(entries.length>30)result.omitted=entries.length-30;
    return result;
  } finally {seen.delete(value)}
}
function sanitizeBrowserMetadata(metadata) {
  const result=Object.create(null);
  if(metadata&&typeof metadata==='object')for(const key of browserMetadataAllowlist){
    if(Object.hasOwn(metadata,key))result[key]=sanitizeBrowserValue(metadata[key]);
  }
  return result;
}
function browserConsoleStyle(level,detail=false) {
  // The already-shared Pealayer catalog is the only color engine. DevTools
  // needs literal CSS colors, so resolve the current page palette at emission.
  const root=typeof document==='undefined'?null:document.documentElement;
  const colors=typeof PEALAYER_PALETTES==='undefined'?null:
    PEALAYER_PALETTES[root?.dataset?.palette==='native'?'native':'studio'][root?.dataset?.theme==='light'?'light':'dark'];
  const role={debug:'muted',info:'blue',success:'green',warn:'amber',error:'red'}[level];
  const foreground=detail?(colors?.text??'inherit'):(colors?.canvas??'inherit');
  const background=detail?(colors?.['surface-1']??'transparent'):(colors?.[role]??'transparent');
  return `color:${foreground};background:${background};font:${detail?'500':'600'} 11px/1.8 system-ui,Segoe UI,sans-serif;padding:2px 8px;border-radius:${detail?'0 999px 999px 0':'999px 0 0 999px'}`;
}
class StyledBrowserConsole {
  constructor(sink=globalThis.console,now=()=>Date.now()){this.sink=sink;this.now=now;this.recent=new Map();this.started=false}
  start(metadata){
    if(this.started)return false;
    this.started=true;
    try {
      this.sink?.groupCollapsed?.('%c 🚀 PAKARCONTROLLER %c Browser operational trace%c',browserConsoleStyle('info'),browserConsoleStyle('info',true),'color:inherit;background:transparent');
      this.event('info','app.start','Web controller initialized',metadata);
      this.sink?.debug?.('Privacy: allowlisted metadata only; credentials are redacted.');
      this.sink?.log?.('Filter by event, level or source. Heartbeats and unchanged polls stay quiet.');
      this.sink?.groupEnd?.();
      return true;
    } catch {return false}
  }
  event(level,event,message,metadata,options={}) {
    try {
      if(!['debug','info','success','warn','error'].includes(level)||!Object.hasOwn(browserEventPresentation,event))return false;
      const now=this.now();if(!Number.isFinite(now)||!Number.isFinite(new Date(now).getTime()))return false;
      if(options.dedupeKey){
        const key=redactBrowserText(options.dedupeKey),windowMs=Number.isFinite(options.dedupeWindowMs)?Math.max(0,Math.min(60000,options.dedupeWindowMs)):1000;
        const previous=this.recent.get(key);
        if(previous!==undefined&&now>=previous&&now-previous<windowMs)return false;
        if(this.recent.size>=256&&!this.recent.has(key))this.recent.delete(this.recent.keys().next().value);
        this.recent.set(key,now);
      }
      const method=level==='success'?'info':level,call=this.sink?.[method];
      if(typeof call!=='function')return false;
      const detail=redactBrowserText(message),safe=sanitizeBrowserMetadata(metadata);
      call.call(this.sink,`%c ${browserEventPresentation[event]} ${level.toUpperCase()} · ${event} %c ${detail.replaceAll('%','%%')}%c`,
        browserConsoleStyle(level),browserConsoleStyle(level,true),'color:inherit;background:transparent',
        {...safe,timestamp:new Date(now).toISOString(),level,event,detail});
      return true;
    } catch {return false} // Diagnostics must never break an actual device action.
  }
}
const browserConsole=new StyledBrowserConsole();
function traceBrowserEvent(level,event,message,metadata,options){return browserConsole.event(level,event,message,metadata,options)}
browserConsole.start({source:'web'});

// END BROWSER DIAGNOSTICS
// BEGIN RAYANLAMP ADAPTATIONS
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
    try { channel=new BroadcastChannel('pakar-locale-v1');channel.postMessage(value); }
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
    channel=new BroadcastChannel('pakar-locale-v1');channel.addEventListener('message',onBroadcast);
  }} catch {}
  return ()=>{
    window.removeEventListener('pakar:locale-preferences',onLocal);
    window.removeEventListener('storage',onStorage);
    channel?.removeEventListener('message',onBroadcast);channel?.close();
  };
}

// END RAYANLAMP ADAPTATIONS
// BEGIN SHARED LOCALIZATION
// Pealayer web_ui/src/i18n.ts::tr semantics: translate application copy only.
// Generated shared dictionary follows; peer values never pass through tr.
// BEGIN SHARED PERSIAN DICTIONARY
const PAKAR_FA={"LEDs & cells":"LEDها و خانه‌ها","AVR & inputs":"AVR و ورودی‌ها","Firmware & storage":"میان‌افزار و ذخیره‌سازی","Search settings…":"جست‌وجوی تنظیمات…","Close device settings":"بستن تنظیمات دستگاه","Device settings pages":"صفحه‌های تنظیمات دستگاه","Discard & reload…":"لغو تغییرات و خواندن دوباره…","Read the device to load settings":"برای بارگذاری تنظیمات، دستگاه را بخوانید","Only fresh, acknowledged device capabilities are shown. Application preferences are separate from board configuration.":"فقط قابلیت‌های تازه و تأییدشدهٔ دستگاه نمایش داده می‌شوند. تنظیمات برنامه از پیکربندی برد جدا هستند.","Firmware build information · read-only":"اطلاعات ساخت میان‌افزار · فقط خواندنی","UART flow-control mode, pin ownership and hardware layout are build flags, not runtime switches. Rebuild and safely update firmware to change them.":"حالت کنترل جریان UART، تخصیص پایه‌ها و چیدمان سخت‌افزار گزینه‌های زمان ساخت هستند. برای تغییر آن‌ها، میان‌افزار را دوباره بسازید و به‌صورت ایمن به‌روز کنید.","No observed build information":"اطلاعات ساخت دریافت نشده است","Build information not reported by this peer":"این دستگاه اطلاعات ساخت را گزارش نکرده است","Use the same live workspace controls; settings does not duplicate device editors.":"از کنترل‌های زندهٔ فضای کاری استفاده کنید؛ ویرایشگرهای دستگاه در تنظیمات تکرار نمی‌شوند.","Table & mapping":"میز و نگاشت","Text presentation":"نمایش متن","Buzzer & melodies":"بیزر و ملودی‌ها","Waiting for acknowledgment and saved-settings readback…":"در انتظار تأیید و بازخوانی تنظیمات ذخیره‌شده…","Settings changed on another client. Your edits are preserved; discard and reload before saving.":"تنظیمات در کلاینت دیگری تغییر کرده‌اند. ویرایش‌های شما حفظ شده‌اند؛ پیش از ذخیره، تغییرات را لغو کنید و دوباره بخوانید.","Unsaved device edits":"ویرایش‌های ذخیره‌نشدهٔ دستگاه","Observed device settings; no pending edits":"تنظیمات دستگاه دریافت شده‌اند؛ ویرایش ذخیره‌نشده‌ای نیست","Reconnect to read device settings":"برای خواندن تنظیمات دستگاه دوباره متصل شوید","Station SSID":"نام شبکهٔ ایستگاه","Station password · re-enter on save":"گذرواژهٔ ایستگاه · هنگام ذخیره دوباره وارد کنید","Open station network · no password":"شبکهٔ باز ایستگاه · بدون گذرواژه","Station enabled":"ایستگاه فعال","Recovery AP enabled":"نقطهٔ دسترسی بازیابی فعال","Scanned networks":"شبکه‌های یافت‌شده","The station password is write-only. Re-enter it to save Wi-Fi changes, or explicitly select an open network. Keep at least one network path enabled.":"گذرواژهٔ ایستگاه فقط نوشتنی است. برای ذخیرهٔ تغییرات وای‌فای، آن را دوباره وارد کنید یا شبکهٔ باز را انتخاب کنید. دست‌کم یک مسیر شبکه را فعال نگه دارید.","Enable MQTT":"فعال‌کردن MQTT","Broker hostname":"نام میزبان بروکر","Base topic":"موضوع پایه","Password handling":"مدیریت گذرواژه","Replacement password":"گذرواژهٔ جایگزین","Enable outbound WebSocket":"فعال‌کردن وب‌سوکت خروجی","Server hostname":"نام میزبان سرور","Path":"مسیر","Token handling":"مدیریت توکن","Replacement token":"توکن جایگزین","Telemetry interval · seconds":"فاصلهٔ دورسنجی · ثانیه","Keep stored password":"حفظ گذرواژهٔ ذخیره‌شده","Keep stored token":"حفظ توکن ذخیره‌شده","Wait for the current operation to finish":"منتظر پایان عملیات جاری بمانید","Saved public settings verified by device readback. Secret values remain write-only.":"تنظیمات عمومی با بازخوانی دستگاه تأیید شدند. مقادیر محرمانه فقط نوشتنی می‌مانند.","No acknowledged workspace panels":"پنل تأییدشده‌ای برای فضای کاری نیست","Reset panel layout":"بازنشانی چیدمان پنل‌ها","Show":"نمایش","System":"سیستم","Dark":"تیره","Light":"روشن","Appearance…":"ظاهر…","Apply appearance":"اعمال ظاهر","Apply":"اعمال","Sign out":"خروج از حساب","Select all":"انتخاب همه","Manage cells…":"مدیریت خانه‌ها…","Toggle without Ctrl":"انتخاب چندتایی بدون Ctrl","Animate cells":"پویانمایی خانه‌ها","LED-state indicators":"نشانگرهای وضعیت LED","Edit order":"ویرایش ترتیب","Reset order":"بازنشانی ترتیب","Cell shape":"شکل خانه","Rounded":"گوشه‌های گرد","Circle · six pixels":"دایره · شش پیکسل","Hexagon · six pixels":"شش‌ضلعی · شش پیکسل","Physical pixel indices":"شمارهٔ پیکسل‌های فیزیکی","Board modes":"حالت‌های برد","Live device":"دستگاه زنده","Clients":"کلاینت‌ها","Station":"ایستگاه","Recovery AP":"نقطهٔ دسترسی بازیابی","Cloud WebSocket":"وب‌سوکت ابری","Cell identity":"هویت خانه","Cell action":"عملیات خانه","Addressable LEDs":"LEDهای آدرس‌پذیر","Save logical / physical mapping":"ذخیرهٔ نگاشت منطقی و فیزیکی","Stream rainbow":"ارسال جریان رنگین‌کمان","Transition":"گذار","Live display":"نمایشگر زنده","Show number":"نمایش عدد","Text":"متن","Scroll short text":"پیمایش متن کوتاه","Repeat":"تکرار","Present":"نمایش متن","Present text":"نمایش متن","Stop text":"توقف متن","Board identity":"هویت برد","Save board identity":"ذخیرهٔ هویت برد","Device access":"دسترسی به دستگاه","Device configuration":"پیکربندی دستگاه","Error":"خطا","Wi-Fi":"وای‌فای","Port":"درگاه","Mapping":"نگاشت","Logical":"خانهٔ منطقی","Logical cell":"خانهٔ منطقی","Physical":"خانهٔ فیزیکی","Find cells: 12, 3–8":"یافتن خانه‌ها: 12, 3–8","Search device settings":"جست‌وجوی تنظیمات دستگاه","Application":"برنامه","Device":"دستگاه","Device settings…":"تنظیمات دستگاه","Device settings":"تنظیمات دستگاه","Table":"میز","Hardware monitor":"پایش سخت‌افزار","Theme":"پوسته","Palette":"پالت رنگ","Search settings":"جست‌وجوی تنظیمات","Username":"نام کاربری","Password":"گذرواژه","Refresh":"تازه‌سازی","Find cell":"یافتن خانه","Manage cells":"مدیریت خانه‌ها","Current":"کنونی","File":"پرونده","Edit":"ویرایش","Audio":"صدا","Workspace":"فضای کاری","Window":"پنجره","Panels":"پنل‌ها","Language":"زبان","Touch feedback":"بازخورد لمسی","System language":"زبان سیستم","English":"انگلیسی","Persian":"فارسی","Direction":"جهت","Automatic":"خودکار","Left to right":"چپ به راست","Right to left":"راست به چپ","Left":"چپ","Right":"راست","Replacement":"جایگزین","Connect":"اتصال","Connecting…":"در حال اتصال…","Disconnect":"قطع اتصال","Identity":"هویت","Order":"ترتیب","Presentation":"نحوهٔ نمایش","Manage":"مدیریت","Type":"نوع","Control":"کنترل","Board":"برد","Color palette":"پالت رنگ","Studio":"استودیو","Neutral":"خنثی","New":"جدید","Overview":"نمای کلی","Capabilities":"قابلیت‌ها","Front panel":"پنل جلویی","Red":"قرمز","White":"سفید","Board name":"نام برد","Mode":"حالت","pixels":"پیکسل","Color":"رنگ","Pixel":"پیکسل","Stop stream":"توقف جریان","Controls":"کنترل‌ها","Page":"صفحه","Select":"انتخاب","Brightness":"روشنایی","No":"خیر","Reconnect":"اتصال دوباره","Connected":"متصل","Hold":"نگه‌داشتن","Toggle":"تغییر حالت","Segments":"نمایشگر هفت‌بخشی","Hide":"پنهان‌کردن","hidden":"پنهان","Visibility":"نمایانی","Telemetry":"دورسنجی","Search":"جست‌وجو","Connected board":"برد متصل","ON":"روشن","Discard":"دور انداختن","Close":"بستن","Save":"ذخیره","Preferences":"تنظیمات","Target":"مقصد","Status":"وضعیت","Server":"سرور","Open":"باز کردن","Cancel":"انصراف","Action":"عملکرد","Enabled":"فعال","Appearance":"ظاهر","Pealayer green":"سبز پی‌لیر","Windows blue":"آبی ویندوز","macOS blue":"آبی مک‌اواس","Custom":"سفارشی","Custom accent":"رنگ تأکیدی سفارشی","Reset":"بازنشانی","Icon":"نماد","Name":"نام","On":"روشن","Off":"خاموش","cues":"نشانه","selected":"انتخاب‌شده","Source":"منبع","Output":"خروجی","Effect":"جلوه","Play":"پخش","Pause":"مکث","Stop":"توقف","Unknown":"ناشناخته","End":"پایان","Step":"پله‌ای","Time":"زمان","Value":"مقدار","Dismiss {title}":"بستن {title}","{count} selected":"{count} انتخاب‌شده","Sent {sent} of {total} cell edits; awaiting device readback":"{sent} از {total} ویرایش خانه ارسال شد؛ در انتظار بازخوانی دستگاه","Cancelled unsent edits. {sent} previously sent edits may have applied.":"ویرایش‌های ارسال‌نشده لغو شدند. ممکن است {sent} ویرایش ارسال‌شده اعمال شده باشند.","Verified {sent} cell states on the device":"وضعیت {sent} خانه در دستگاه تأیید شد","Readback did not confirm every edit. Refresh and inspect; batch is not atomic.":"بازخوانی همهٔ ویرایش‌ها را تأیید نکرد. تازه‌سازی و بررسی کنید؛ عملیات گروهی اتمی نیست.","Stopped after {sent} edits. Uncertain writes are not retried.":"پس از {sent} ویرایش متوقف شد. نوشته‌های نامطمئن دوباره ارسال نمی‌شوند."};
// END SHARED PERSIAN DICTIONARY
// Actual RayanLamp i18n.tsx normalize/detect/t/digits/controller contracts (MIT).
// Keep Pakar's primary-language policy, explicit direction and peer identity.
function normalizeUiLanguagePreference(value){return ['system','en','fa'].includes(value)?value:'system'}
function resolveUiLanguage(preference, languages=globalThis.navigator?.languages||[globalThis.navigator?.language||'en']){
  const selected=normalizeUiLanguagePreference(preference);
  if(selected!=='system')return selected;
  return String(languages[0]||globalThis.navigator?.language||'en').toLowerCase().startsWith('fa')?'fa':'en';
}
function resolveUiDirection(preference,language){return preference==='ltr'||preference==='rtl'?preference:language==='fa'?'rtl':'ltr'}
let uiLanguagePreference='system',uiDirectionPreference='auto';
try{uiLanguagePreference=localStorage.getItem('pakar-locale')||'system';uiDirectionPreference=localStorage.getItem('pakar-direction')||'auto'}catch{}
if(!validLocalePreferences({language:uiLanguagePreference,direction:uiDirectionPreference})){uiLanguagePreference='system';uiDirectionPreference='auto'}
let uiLanguage=resolveUiLanguage(uiLanguagePreference);
function uiTr(text,values){
  const translated=uiLanguage==='fa'?(PAKAR_FA[text]??text):text;
  if(!values)return translated;
  return translated.replace(/\{(\w+)\}/g,(match,key)=>Object.hasOwn(values,key)&&values[key]!==undefined?String(values[key]):match);
}
function uiDigits(value){const text=String(value);return uiLanguage==='fa'?text.replace(/\d/g,digit=>'۰۱۲۳۴۵۶۷۸۹'[Number(digit)]):text}
function uiNumber(value,options){return new Intl.NumberFormat(uiLanguage==='fa'?'fa-IR':'en-US',{numberingSystem:uiLanguage==='fa'?'arabext':'latn',...options}).format(value)}
const ownedCopy=[];
const ownedAttributes=[];
const ownedDynamicCopy=new Map();
function uiCopyValues(values){return values?Object.fromEntries(Object.entries(values).map(([key,value])=>[key,typeof value==='number'?uiNumber(value):value])):values}
function setUiText(node,text,values){if(!node)return;ownedDynamicCopy.set(node,{text,values});node.textContent=uiTr(text,uiCopyValues(values))}
let localizationInitialized=false,stopUiLocaleController=null;
function startUiLocaleController(){
  stopUiLocaleController?.();
  let stopped=false;
  const onLanguageChange=()=>{if(!stopped&&uiLanguagePreference==='system')applyLocalization()};
  const unsubscribe=subscribeLocalePreferences(value=>{
    if(stopped)return;
    uiLanguagePreference=value.language;uiDirectionPreference=value.direction;
    const language=document.getElementById('uiLanguage'),direction=document.getElementById('uiDirection');
    if(language)language.value=value.language;if(direction)direction.value=value.direction;
    applyLocalization();
  });
  globalThis.window?.addEventListener?.('languagechange',onLanguageChange);
  const stop=()=>{
    if(stopped)return;stopped=true;unsubscribe();
    globalThis.window?.removeEventListener?.('languagechange',onLanguageChange);
    if(stopUiLocaleController===stop)stopUiLocaleController=null;
  };
  stopUiLocaleController=stop;
  return stop;
}
function initializeLocalization(){
  if(!document.createTreeWalker)return;
  if(localizationInitialized){startUiLocaleController();applyLocalization();return}
  localizationInitialized=true;
  // Snapshot ONLY initial application-owned template nodes, before bootstrap.
  // No mutation observer: a peer name equal to a dictionary key is still a name.
  const walker=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT);
  while(walker.nextNode()){const node=walker.currentNode;if(node.parentElement.closest('script,style,[data-peer-value]'))continue;
    const text=node.nodeValue.trim();if(PAKAR_FA[text])ownedCopy.push({node,text,prefix:node.nodeValue.match(/^\s*/)[0],suffix:node.nodeValue.match(/\s*$/)[0]});}
  for(const node of document.querySelectorAll('[placeholder],[aria-label],[title]')){
    if(node.closest('[data-peer-value]'))continue;
    for(const attribute of ['placeholder','aria-label','title']){const text=node.getAttribute(attribute);if(PAKAR_FA[text])ownedAttributes.push({node,attribute,text});}
  }
  document.getElementById('uiLanguage').value=uiLanguagePreference;
  document.getElementById('uiDirection').value=uiDirectionPreference;
  const hapticsControl=document.getElementById('uiHaptics');
  if(hapticsControl){hapticsControl.checked=webHapticsEnabled;hapticsControl.disabled=!webHaptics.supported;}
  startUiLocaleController();
  applyLocalization();
}
function applyLocalization(){
  uiLanguage=resolveUiLanguage(uiLanguagePreference);
  document.documentElement.lang=uiLanguage;
  document.documentElement.dir=resolveUiDirection(uiDirectionPreference,uiLanguage);
  if(document.documentElement.dataset){document.documentElement.dataset.localePreference=uiLanguagePreference;document.documentElement.dataset.resolvedLocale=uiLanguage}
  for(const entry of ownedCopy)if(entry.node.isConnected)entry.node.nodeValue=entry.prefix+uiTr(entry.text)+entry.suffix;
  for(const entry of ownedAttributes)if(entry.node.isConnected)entry.node.setAttribute(entry.attribute,uiTr(entry.text));
  for(const[node,entry]of ownedDynamicCopy){if(node.isConnected===false){ownedDynamicCopy.delete(node);continue}node.textContent=uiTr(entry.text,uiCopyValues(entry.values))}
  // Safe, named device-settings labels are application catalog copy.
  if(typeof webSettings!=='undefined'){webSettings.navSignature='';settingsNavigation();settingsNavigate(webSettings.page);}
  if(typeof renderPanelMenu==='function')renderPanelMenu();
  if(typeof renderSelectionSummary==='function')renderSelectionSummary();
  if(typeof webToasts!=='undefined')webToasts?.refresh();
}
function saveLocalization(){
  uiLanguagePreference=document.getElementById('uiLanguage').value;
  uiDirectionPreference=document.getElementById('uiDirection').value;
  applyLocalization();
  announceLocalePreferences({language:uiLanguagePreference,direction:uiDirectionPreference});
}

// END SHARED LOCALIZATION
// BEGIN BROWSER APPEARANCE
function appearanceCustomControls(){appearanceCustom.disabled=appearanceAccent.value!=='custom'}
appearanceOpen.addEventListener('click',()=>{appearanceTheme.value=browserAppearance.theme;appearancePalette.value=browserAppearance.color_palette;appearanceAccent.value=browserAppearance.accent_color;appearanceCustom.value=browserAppearance.custom_accent_color||'#0078d4';appearanceCustomControls();appearanceError.textContent='';appearanceDialog.showModal()});
appearanceAccent.addEventListener('change',appearanceCustomControls);
appearanceClose.addEventListener('click',()=>appearanceDialog.close());appearanceCancel.addEventListener('click',()=>appearanceDialog.close());
appearanceForm.addEventListener('submit',event=>{event.preventDefault();browserAppearance=normalizeBrowserAppearance({theme:appearanceTheme.value,color_palette:appearancePalette.value,accent_color:appearanceAccent.value,custom_accent_color:appearanceCustom.value});applyBrowserAppearance();try{const encoded=JSON.stringify(browserAppearance);if(localStorage.getItem('pakar-appearance')!==encoded)localStorage.setItem('pakar-appearance',encoded);appearanceDialog.close()}catch{appearanceError.textContent='Applied for this session only; browser storage is unavailable.'}});
applyBrowserAppearance();
// END BROWSER APPEARANCE
const grid=document.querySelector('#grid');let state={cells:[]},mapping={physical:[],names:[]},socket=null,streamTimer=0,streamHue=0;
// BEGIN PEER CONTEXT
// Adapted PCController peripheral-availability.ts: ready AND advertised.
// Each feature comes from this peer's ACK contract, never from cached preferences.
let contextReceivedAt=0,contextModeSignature='';
function peerAvailability(s,ready=true){
  const c=s?.capabilities,features=new Set(ready&&c?.schema===1&&Array.isArray(c.acknowledged_features)?c.acknowledged_features.filter(v=>typeof v==='string'):[]);
  const table=features.has('table')&&c.board_kind==='table'&&c.table?.columns===10&&c.table?.rows===8&&c.table?.pixels_per_cell===6;
  const modes=(table&&Array.isArray(c.modes)?c.modes:[]).filter(m=>m&&typeof m.label==='string'&&((m.kind==='presentation'&&[['idle',0],['cell',1],['snake',3]].some(([id,code])=>m.id===id&&m.code===code))||(m.kind==='game'&&m.id==='first-wins'&&m.code===2&&features.has('inputs')&&features.has('outputs'))));
  return {has:feature=>feature==='table'?table:features.has(feature),modes,name:ready&&c?.schema===1&&typeof c.name==='string'?c.name:'',known:ready&&c?.schema===1};
}
function currentPeer(){return peerAvailability(state,contextReceivedAt>0&&Date.now()-contextReceivedAt<8000)}
function contextAllows(path,data={}){
  const c=currentPeer(),required={'/api/v1/cell':'table','/api/v1/mapping':'table','/api/v1/game/reset':'table','/api/v1/snake/direction':'table','/api/v1/buzzer':'buzzer','/api/v1/audio':'buzzer','/api/v1/outputs':'outputs','/api/v1/effect':'leds','/api/v1/brightness':'leds'};
  if(path==='/api/v1/mode')return c.modes.some(m=>m.id===data.value);
  if(path==='/api/v1/display')return c.has(Object.hasOwn(data,'text')?'text':'display');
  return required[path]?c.has(required[path]):c.known;
}
function renderContext(){
  const c=currentPeer();
  document.querySelectorAll('[data-capability]').forEach(node=>node.hidden=!c.has(node.dataset.capability));
  document.querySelector('.layout')?.classList.toggle('controller-only',!c.has('table'));
  renderApplicationIdentity(c.known?state.capabilities:null);
  contextMessage.textContent=c.known?(c.name||'Connected board')+' · '+(c.has('table')?'Table board':'Controller'):contextReceivedAt?'Capabilities unavailable or stale. Connect to a compatible board.':'Waiting for board capabilities…';
  const signature=JSON.stringify(c.modes);
  if(signature!==contextModeSignature){contextModeSignature=signature;boardModes.replaceChildren();for(const kind of ['presentation','game']){const modes=c.modes.filter(m=>m.kind===kind);if(!modes.length)continue;const label=document.createElement('p');label.className='hint';label.textContent=kind==='presentation'?'Table presentation':'Game';boardModes.appendChild(label);const row=document.createElement('div');row.className='row';for(const mode of modes){const button=document.createElement('button');button.dataset.mode=mode.id;button.textContent=mode.label;button.onclick=()=>setMode(mode.id);row.appendChild(button)}boardModes.appendChild(row)}}
  document.querySelectorAll('[data-mode]').forEach(b=>b.classList.toggle('active',b.dataset.mode===state.mode));
  snakeControls.hidden=!c.modes.some(m=>m.id==='snake')||state.mode!=='snake';
  if(!c.has('table')){cellBatchCancelled=true;if(cellManager.open)cellManager.close();closeCellMenu();setEditingOrder(false);}
  if(!c.has('streaming')||!c.has('table'))stopStream();
  for(const [id,feature]of [['tm','display'],['avr','inputs'],['uart','inputs'],['winner','table']]){const node=document.querySelector('#'+id);if(node)(node.closest?.('.metric')||node).hidden=!c.has(feature);}
  if(typeof settingsBuildInfo==='function')settingsBuildInfo();
  if(typeof applyPanelVisibility==='function')applyPanelVisibility();
}
// END PEER CONTEXT
// BEGIN APPLICATION IDENTITY
// Adapted Pealayer HeaderBar applicationName/applicationIcon ownership.
const fallbackArtwork=document.querySelector('.app-logo')?.getAttribute('src');
let applicationSignature='',savedIdentityHost='';
function validBoardName(s){return typeof s==='string'&&s.length>0&&new TextEncoder().encode(s).length<=63&&!s.startsWith(' ')&&!s.endsWith(' ')&&!/[\u0000-\u001f\u007f-\u009f]/.test(s)}
function validApplication(a){return a&&[['id',96],['name',96],['title',160]].every(([key,max])=>typeof a[key]==='string'&&a[key].trim()&&new TextEncoder().encode(a[key]).length<=max&&!/[\u0000-\u001f\u007f-\u009f]/.test(a[key]))}
function applicationArtwork(a){
  try{const s=a?.icon_png_base64;if(typeof s!=='string'||s.length>4096||s.length%4||!/^[-A-Za-z0-9+/]*={0,2}$/.test(s))return fallbackArtwork;
    const b=Uint8Array.from(atob(s),c=>c.charCodeAt(0));if(b.length<33||[137,80,78,71,13,10,26,10].some((v,i)=>b[i]!==v)||String.fromCharCode(...b.slice(12,16))!=='IHDR')return fallbackArtwork;
    const v=new DataView(b.buffer);if([v.getUint32(16),v.getUint32(20)].some(n=>n<8||n>256))return fallbackArtwork;
    return 'data:image/png;base64,'+s;
  }catch{return fallbackArtwork}
}
function renderApplicationIdentity(c){
  const a=c?.schema===1&&validApplication(c.application)?c.application:null,name=a?.name||'PakarController',title=a?.title||name;
  document.title=title+(c?.name?' · '+c.name:' · No acknowledged board')+(c?.hostname?' · '+c.hostname+'.local':'');
  const heading=document.querySelector('.top h1');if(heading)heading.textContent=name;
  const info=document.querySelector('#applicationIdentity');if(info)info.textContent=a?a.title+' · '+a.id:'Application identity unavailable';
  const signature=JSON.stringify(a);if(signature===applicationSignature)return;applicationSignature=signature;
  const artwork=applicationArtwork(a);
  document.querySelectorAll('.app-logo').forEach(img=>{img.src=artwork;img.alt=name+' application logo';img.onerror=()=>{img.onerror=null;img.src=fallbackArtwork}});
  document.querySelectorAll('link[rel="icon"],link[rel="apple-touch-icon"]').forEach(link=>link.href=artwork);
}
// END APPLICATION IDENTITY
function renderAudio(s){const names=['welcome','update-start','update-success','error','winner','finish','lost','incorrect','rapid-error','fault','increase','decrease','welcome-puzzleboard','welcome-40led'];const selected=audioCue.value;audioCue.replaceChildren();for(const cue of new Set(Array.isArray(s.audio_cues)?s.audio_cues:[])){if(typeof cue!=='string'||!names.includes(cue))continue;const option=document.createElement('option');option.value=cue;option.textContent=cue;audioCue.appendChild(option)}if([...audioCue.options].some(o=>o.value===selected))audioCue.value=selected;audioCue.disabled=audioPlay.disabled=s.avr_online!==true||audioCue.options.length===0;}
// Cell interaction is initialized below; clicks select rather than silently changing LEDs.
let selectedCells=new Set(),selectionAnchor=1,cellFocus=1,cellDrag=null,suppressCellClick=false,cellBatchBusy=false,cellBatchCancelled=false;
let cellOrder=Array.from({length:80},(_,i)=>i+1),editingOrder=false,orderDrag=null;
let pixelSample=null,pixelSampleAt=0,lastPixelStatus=null;
function renderSelectionSummary(){selectionSummary.textContent=managerCount.textContent=uiTr('{count} selected',{count:uiNumber(selectedCells.size)})}
function validPixelMapping(p){return Array.isArray(p)&&p.length===80&&new Set(p).size===80&&p.every(n=>Number.isInteger(n)&&n>=1&&n<=80)}
function validPixelFrame(f){return f&&Number.isInteger(f.revision)&&f.revision>=0&&Number.isInteger(f.brightness)&&f.brightness>=0&&f.brightness<=255&&typeof f.rgb==='string'&&/^[0-9a-f]{2880}$/i.test(f.rgb)&&validPixelMapping(f.physical)}
function pixelPoint(sector,radius=45){const a=-Math.PI/2-sector*Math.PI/3;return [50+radius*Math.cos(a),50+radius*Math.sin(a)]}
function pixelSvg(tag,attrs={}){const e=document.createElementNS('http://www.w3.org/2000/svg',tag);for(const [k,v] of Object.entries(attrs))e.setAttribute(k,String(v));return e}
function renderCellPixels(){
 if(lastPixelStatus!==state){lastPixelStatus=state;if(state.pixel_preview_requested===true){pixelSample=validPixelFrame(state.pixel_frame)?state.pixel_frame:null;pixelSampleAt=Date.now()}}
 const shape=cellShape.value||'circle',frame=pixelSample&&Date.now()-pixelSampleAt<5000?pixelSample:null;
 grid.classList.toggle('pixel-cells',shape!=='rounded');pixelDiagnostics.disabled=shape==='rounded';
 for(const b of grid.querySelectorAll('.cell')){
  const id=Number(b.dataset.id),label=String(mapping.names[id-1]||id);
  const physical=frame?frame.physical:validPixelMapping(mapping.physical)?mapping.physical:null;
  const first=physical?(physical[id-1]-1)*6:null;
  const colors=frame?Array.from({length:6},(_,s)=>'#'+frame.rgb.slice((first+s)*6,(first+s+1)*6)):null;
  const signature=JSON.stringify([shape,pixelDiagnostics.checked,label,first,colors,frame?.revision,frame?.brightness]);
  if(b.dataset.pixelSignature===signature)continue;b.dataset.pixelSignature=signature;
  b.replaceChildren();
  if(shape==='rounded'){b.textContent=label;continue}
  const svg=pixelSvg('svg',{viewBox:'0 0 100 100','aria-hidden':true,class:'pixel-face'});
  for(let s=0;s<6;s++){
   const steps=shape==='circle'?8:1,points=[[50,50]];
   for(let step=0;step<=steps;step++)points.push(pixelPoint(s-.5+step/steps));
   const sector=pixelSvg('polygon',{points:points.map(p=>p.join(',')).join(' '),fill:colors?colors[s]:'var(--cell-surface,var(--surface-2))',class:'pixel-sector'});
   const title=pixelSvg('title');title.textContent=(first===null?'Unknown pixel':'Pixel '+(first+s))+' · '+(colors?colors[s]:'RGB unavailable');sector.appendChild(title);svg.appendChild(sector);
   if(pixelDiagnostics.checked){const [x,y]=pixelPoint(s,33);svg.appendChild(pixelSvg('rect',{x:x-12,y:y-6,width:24,height:12,rx:3,class:'pixel-index-badge'}));const text=pixelSvg('text',{x,y:y+3,class:'pixel-index'});text.textContent=first===null?'?':first+s;svg.appendChild(text)}
  }
  const outline=shape==='circle'?pixelSvg('circle',{cx:50,cy:50,r:45,class:'pixel-outline'}):pixelSvg('polygon',{points:Array.from({length:6},(_,i)=>pixelPoint(i-.5).join(',')).join(' '),class:'pixel-outline'});svg.appendChild(outline);
  const text=pixelSvg('text',{x:50,y:54,class:'pixel-label'});text.textContent=[...label].length>6?[...label].slice(0,5).join('')+'…':label;svg.appendChild(text);
  if(!colors){const hint=pixelSvg('text',{x:50,y:67,class:'pixel-unknown'});hint.textContent='RGB unavailable';svg.appendChild(hint)}
  b.appendChild(svg);b.title='Logical '+id+' · '+label+' · '+(first===null?'mapping unavailable':'pixels '+first+'–'+(first+5))+' · top first, CCW · '+(frame?'sampled device frame '+frame.revision+', pre-brightness RGB; brightness '+frame.brightness:'RGB unavailable')+(colors?'\n'+colors.map((c,s)=>'Pixel '+(first+s)+': '+c).join('\n'):'');
 }
}
try{const p=JSON.parse(localStorage.getItem('pakar-table')||'{}');additiveSelection.checked=p.additive===true;animateCells.checked=p.animate!==false;showCellIndicators.checked=p.indicators===true;cellOrder=normalizedCellOrder(p.order);cellShape.value=['rounded','circle','hexagon'].includes(p.shape)?p.shape:'circle';pixelDiagnostics.checked=p.pixelDiagnostics===true}catch{}
function saveTablePreferences(){try{const value=JSON.stringify({additive:additiveSelection.checked,animate:animateCells.checked,indicators:showCellIndicators.checked,order:cellOrder,shape:cellShape.value,pixelDiagnostics:pixelDiagnostics.checked});if(localStorage.getItem('pakar-table')!==value)localStorage.setItem('pakar-table',value)}catch(error){report(error)}grid.classList.toggle('no-cell-animation',!animateCells.checked);grid.classList.toggle('show-cell-indicators',showCellIndicators.checked);renderCellPixels()}
saveTablePreferences();
function normalizedCellOrder(saved){const seen=new Set();return [...(Array.isArray(saved)?saved:[]),...Array.from({length:80},(_,i)=>i+1)].filter(id=>Number.isInteger(id)&&id>=1&&id<=80&&!seen.has(id)&&seen.add(id))}
function cancelOrderDrag(){orderDrag=null;for(const b of [...grid.querySelectorAll('.cell'),...managerRows.querySelectorAll('[data-cell]')]){b.classList.toggle('order-source',false);b.classList.toggle('order-before',false);b.classList.toggle('order-after',false)}}
function setEditingOrder(value){editingOrder=value===true;resetCellOrderButton.disabled=managerResetCellOrderButton.disabled=!editingOrder;editCellOrder.checked=managerEditCellOrder.checked=editingOrder;if(cellDrag)selectedCells=cellDrag.base;cellDrag=null;grid.querySelector('.marquee')?.remove();cancelOrderDrag();grid.classList.toggle('editing-order',editingOrder);for(const b of [...grid.querySelectorAll('.cell'),...managerRows.querySelectorAll('[data-cell]')])b.draggable=editingOrder;paintSelection()}
function refreshCellOrder(){const buttons=new Map([...grid.querySelectorAll('.cell')].map(b=>[Number(b.dataset.id),b])),managerButtons=new Map([...managerRows.querySelectorAll('[data-cell]')].map(b=>[Number(b.dataset.cell),b]));cellOrder.forEach((id,index)=>{const b=buttons.get(id);if(b)grid.appendChild(b);const m=managerButtons.get(id);if(m)managerRows.children[Math.floor(index/10)].appendChild(m)});paintSelection(true)}
// Adapt Pealayer remove-source/remaining-target insertion, with an explicit mode guard.
function reorderCell(source,target,before=true){if(!editingOrder||source===target||cellOrder.length!==80||new Set(cellOrder).size!==80||!cellOrder.includes(source)||!cellOrder.includes(target))return false;const next=cellOrder.filter(id=>id!==source),at=next.indexOf(target)+(before?0:1);next.splice(at,0,source);if(next.every((id,i)=>id===cellOrder[i]))return false;cellOrder=next;refreshCellOrder();saveTablePreferences();return true}
function resetCellOrder(){if(!editingOrder)return;const defaults=Array.from({length:80},(_,i)=>i+1);if(defaults.every((id,i)=>id===cellOrder[i]))return;cancelOrderDrag();cellOrder=defaults;refreshCellOrder();saveTablePreferences()}
// Actual Pealayer HardwareTab drag-image/effectAllowed/local source lifecycle, adapted to cells.
function bindCellOrderDrag(b,id){b.draggable=editingOrder;b.ondragstart=e=>{if(!editingOrder){e.preventDefault();return}cancelOrderDrag();orderDrag=id;e.dataTransfer.effectAllowed='move';e.dataTransfer.setData('application/x-pakar-cell',String(id));const r=b.getBoundingClientRect();e.dataTransfer.setDragImage?.(b,e.clientX-r.left,e.clientY-r.top);b.classList.toggle('order-source',true)};b.ondragover=e=>{if(!editingOrder||orderDrag===null||orderDrag===id)return;e.preventDefault();e.dataTransfer.dropEffect='move';const before=e.clientX<b.getBoundingClientRect().left+b.getBoundingClientRect().width/2;b.classList.toggle('order-before',before);b.classList.toggle('order-after',!before)};b.ondragleave=()=>{b.classList.toggle('order-before',false);b.classList.toggle('order-after',false)};b.ondrop=e=>{if(!editingOrder||orderDrag===null)return;e.preventDefault();const before=e.clientX<b.getBoundingClientRect().left+b.getBoundingClientRect().width/2;reorderCell(orderDrag,id,before);cancelOrderDrag()};b.ondragend=cancelOrderDrag}
function parseSelectionQuery(query){if(query.length>256)throw Error('Search is too long');const ids=new Set();for(const token of latinDigits(query).replace(/[–—]/g,'-').replace(/،/g,',').split(/[,;\s]+/).filter(Boolean)){const parts=token.split('-').map(v=>v.replace(/^#/,''));if(parts.length>2||parts.some(v=>!/^\d+$/.test(v)))throw Error('Use cell IDs 1–80 or ranges such as 3–8');const a=Number(parts[0]),b=Number(parts[1]||parts[0]);if(a<1||b>80||a>b)throw Error('Cell ranges must stay within 1–80');for(let id=a;id<=b;id++)ids.add(id)}if(query.trim()&&!ids.size)throw Error('Enter a cell ID or range');return [...ids].sort((a,b)=>a-b)}
function selectionQuery(){const ids=[...selectedCells].sort((a,b)=>a-b),parts=[];for(let i=0;i<ids.length;i++){const start=ids[i];let end=start;while(ids[i+1]===end+1)end=ids[++i];parts.push(start===end?String(start):start+'–'+end)}return parts.join(', ')}
function paintSelection(fromSearch=false){renderCellPixels();if(!fromSearch){cellSearch.value=selectionQuery();cellSearch.setCustomValidity?.('')}if(selectedCells.size&&!selectedCells.has(Number(cellId.value))){cellId.value=[...selectedCells].sort((a,b)=>a-b)[0];loadMappingEditor(Number(cellId.value))}for(const b of grid.querySelectorAll('.cell')){const id=Number(b.dataset.id),selected=selectedCells.has(id);b.classList.toggle('selected',selected);b.setAttribute('aria-pressed',String(selected));b.tabIndex=id===cellFocus?0:-1}renderSelectionSummary();for(const b of managerRows.querySelectorAll('[data-cell]'))b.setAttribute('aria-pressed',String(selectedCells.has(Number(b.dataset.cell))));for(const b of managerRows.querySelectorAll('[data-row]')){const start=Number(b.dataset.row)*10;b.setAttribute('aria-pressed',String(cellOrder.slice(start,start+10).every(id=>selectedCells.has(id))))}}
function selectCell(id,event={}){if(event.shiftKey){selectedCells=new Set(Array.from({length:Math.abs(id-selectionAnchor)+1},(_,i)=>Math.min(id,selectionAnchor)+i))}else if(event.ctrlKey||event.metaKey||additiveSelection.checked){selectedCells.has(id)?selectedCells.delete(id):selectedCells.add(id);selectionAnchor=id}else{selectedCells=new Set([id]);selectionAnchor=id}cellFocus=id;cellId.value=id;loadMappingEditor(id);paintSelection()}
function selectRow(row,event={}){const ids=cellOrder.slice(row*10,row*10+10);if(event.ctrlKey||event.metaKey||additiveSelection.checked){const remove=ids.every(id=>selectedCells.has(id));ids.forEach(id=>remove?selectedCells.delete(id):selectedCells.add(id))}else selectedCells=new Set(ids);paintSelection()}
function selectAllCells(){selectedCells=new Set(Array.from({length:80},(_,i)=>i+1));paintSelection()}
function clearCellSelection(){selectedCells.clear();paintSelection()}
function updateSelectionFromQuery(){try{selectedCells=new Set(parseSelectionQuery(cellSearch.value));if(selectedCells.size){selectionAnchor=cellFocus=[...selectedCells][0];cellId.value=cellFocus;loadMappingEditor(cellFocus)}cellSearch.setCustomValidity?.('');paintSelection(true)}catch(error){cellSearch.setCustomValidity?.(error.message);report(error)}}
cellSearch.addEventListener('input',updateSelectionFromQuery);
function normalizeSelectionQuery(){try{parseSelectionQuery(cellSearch.value);cellSearch.value=selectionQuery();cellSearch.setCustomValidity?.('')}catch(error){cellSearch.setCustomValidity?.(error.message)}}
function navigateSelection(backwards=false){const ids=[...selectedCells].sort((a,b)=>a-b);if(!ids.length)return;const at=ids.indexOf(cellFocus);cellFocus=ids[at<0?(backwards?ids.length-1:0):(at+(backwards?-1:1)+ids.length)%ids.length];cellId.value=cellFocus;loadMappingEditor(cellFocus);paintSelection(true)}
cellSearch.addEventListener('blur',normalizeSelectionQuery);
cellSearch.addEventListener('keydown',e=>{if(e.key==='F3'||e.key==='Enter'){e.preventDefault();if(e.key==='Enter')normalizeSelectionQuery();navigateSelection(e.shiftKey)}else if(e.key==='Escape'){e.preventDefault();clearCellSelection()}});
function cellNavigate(id,key,ctrl=false){const slot=cellOrder.indexOf(id),col=slot%10;let next=slot;if(key==='ArrowLeft'&&col>0)next--;if(key==='ArrowRight'&&col<9)next++;if(key==='ArrowUp'&&slot>=10)next-=10;if(key==='ArrowDown'&&slot<70)next+=10;if(key==='Home')next=ctrl?0:Math.floor(slot/10)*10;if(key==='End')next=ctrl?79:Math.floor(slot/10)*10+9;return cellOrder[next]??id}
for(let id=1;id<=80;id++){const b=document.createElement('button');b.className='cell';b.textContent=id;b.dataset.id=id;b.tabIndex=id===1?0:-1;b.setAttribute('aria-label','Logical cell '+id);bindCellOrderDrag(b,id);b.onclick=e=>{if(editingOrder)return;if(suppressCellClick){suppressCellClick=false;return}selectCell(id,e)};b.ondblclick=()=>{if(!editingOrder)openCellManager()};b.onkeydown=e=>{const ctrl=e.ctrlKey||e.metaKey;if(['ArrowLeft','ArrowRight','ArrowUp','ArrowDown','Home','End'].includes(e.key)){e.preventDefault();const next=cellNavigate(id,e.key,ctrl);cellFocus=next;if(!editingOrder&&(!ctrl||e.shiftKey))selectCell(next,e);paintSelection();grid.querySelector('[data-id="'+next+'"]').focus()}else if(ctrl&&e.key.toLowerCase()==='a'){e.preventDefault();selectAllCells()}else if(e.key==='Escape'){e.preventDefault();cancelOrderDrag();if(!editingOrder)clearCellSelection()}else if(e.key==='Enter'){e.preventDefault();if(!selectedCells.size)selectCell(id);openCellManager()}else if(e.key===' '){e.preventDefault();if(!editingOrder)selectCell(id,e)}else if(e.key==='ContextMenu'||(e.shiftKey&&e.key==='F10')){e.preventDefault();const r=b.getBoundingClientRect();openCellMenu(id,r.left,r.bottom)}};b.oncontextmenu=e=>{e.preventDefault();openCellMenu(id,e.clientX,e.clientY)};grid.appendChild(b)}
for(let row=0;row<8;row++){const line=document.createElement('div');line.className='manager-row';const heading=document.createElement('button');heading.textContent='Row '+(row+1);heading.dataset.row=row;heading.onclick=e=>selectRow(row,e);line.appendChild(heading);for(let id=row*10+1;id<=row*10+10;id++){const b=document.createElement('button');b.dataset.cell=id;b.textContent=id;bindCellOrderDrag(b,id);b.onclick=e=>{if(!editingOrder)selectCell(id,e)};line.appendChild(b)}managerRows.appendChild(line)}
refreshCellOrder();
document.addEventListener?.('keydown',e=>{if(e.key==='Escape')cancelOrderDrag()});
function openCellManager(){if(!currentPeer().has('table'))return;closeCellMenu();paintSelection();cellManager.showModal()}
function closeCellMenu(){cellMenu.hidden=true}
function menuItem(label,icon,action,disabled=false){const b=document.createElement('button');b.type='button';b.setAttribute('role','menuitem');const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.setAttribute('viewBox','0 0 24 24');const path=document.createElementNS(svg.namespaceURI,'path');path.setAttribute('d',icon);svg.appendChild(path);b.appendChild(svg);b.appendChild(document.createTextNode(label));b.disabled=disabled;b.onclick=()=>{closeCellMenu();action()};cellMenu.appendChild(b)}
function openCellMenu(id,x,y){if(id&&!selectedCells.has(id))selectCell(id);cellMenu.replaceChildren();const blocked=!authenticated||cellBatchBusy||!selectedCells.size;for(const value of ['white','red','off'])menuItem(value+' · '+selectedCells.size+' cells','M12 3v3m0 12v3M3 12h3m12 0h3M8 8h8v8H8z',()=>applyCellBatch(value),blocked);cellMenu.appendChild(document.createElement('hr'));menuItem('Manage cells…','M4 6h16M4 12h16M4 18h16M8 3v6m8 0v6M8 15v6',openCellManager);if(id)menuItem('Select row '+(Math.floor(cellOrder.indexOf(id)/10)+1),'M4 6h16M4 12h16M4 18h16',()=>selectRow(Math.floor(cellOrder.indexOf(id)/10),{ctrlKey:true}));menuItem('Clear selection','M6 6l12 12M6 18L18 6',clearCellSelection);cellMenu.hidden=false;const r=cellMenu.getBoundingClientRect();cellMenu.style.left=Math.max(8,Math.min(x,innerWidth-r.width-8))+'px';cellMenu.style.top=Math.max(8,Math.min(y,innerHeight-r.height-8))+'px';cellMenu.querySelector('button:not(:disabled)')?.focus()}
cellMenu.addEventListener('keydown',e=>{const buttons=[...cellMenu.querySelectorAll('button:not(:disabled)')],at=buttons.indexOf(document.activeElement);if(e.key==='Escape'){closeCellMenu();grid.querySelector('[data-id="'+cellFocus+'"]')?.focus()}else if(e.key==='ArrowDown'||e.key==='ArrowUp'){e.preventDefault();buttons[(at+(e.key==='ArrowDown'?1:buttons.length-1))%buttons.length]?.focus()}});
document.addEventListener?.('pointerdown',e=>{if(!cellMenu.contains(e.target))closeCellMenu()});
grid.addEventListener('contextmenu',e=>{if(e.target===grid){e.preventDefault();openCellMenu(null,e.clientX,e.clientY)}});
grid.addEventListener('pointerdown',e=>{if(editingOrder||e.button!==0||e.pointerType==='touch')return;cellDrag={x:e.clientX,y:e.clientY,base:new Set(selectedCells),additive:e.ctrlKey||e.metaKey||e.shiftKey,started:false,pointer:e.pointerId}});
grid.addEventListener('pointermove',e=>{if(!cellDrag||cellDrag.pointer!==e.pointerId)return;const d=cellDrag;if(!d.started&&Math.hypot(e.clientX-d.x,e.clientY-d.y)<5)return;d.started=true;grid.setPointerCapture(e.pointerId);selectedCells=d.additive?new Set(d.base):new Set();const x1=Math.min(d.x,e.clientX),x2=Math.max(d.x,e.clientX),y1=Math.min(d.y,e.clientY),y2=Math.max(d.y,e.clientY);for(const b of grid.querySelectorAll('.cell')){const r=b.getBoundingClientRect(),x=r.left+r.width/2,y=r.top+r.height/2;if(x>=x1&&x<=x2&&y>=y1&&y<=y2)selectedCells.add(Number(b.dataset.id))}let box=grid.querySelector('.marquee');if(!box){box=document.createElement('div');box.className='marquee';grid.appendChild(box)}const r=grid.getBoundingClientRect();Object.assign(box.style,{left:x1-r.left+'px',top:y1-r.top+'px',width:x2-x1+'px',height:y2-y1+'px'});paintSelection()});
grid.addEventListener('pointerup',e=>{if(!cellDrag)return;suppressCellClick=cellDrag.started;grid.querySelector('.marquee')?.remove();if(grid.hasPointerCapture?.(e.pointerId))grid.releasePointerCapture(e.pointerId);cellDrag=null});
grid.addEventListener('pointercancel',()=>{if(cellDrag)selectedCells=cellDrag.base;cellDrag=null;grid.querySelector('.marquee')?.remove();paintSelection()});
function cancelCellBatch(){cellBatchCancelled=true}
async function applyCellBatch(value){if(!currentPeer().has('table')||cellBatchBusy||!authenticated||!selectedCells.size||!['white','red','off'].includes(value))return;const ids=[...selectedCells].sort((a,b)=>a-b),transition=bulkEffect.value;cellBatchBusy=true;cellBatchCancelled=false;bulkApply.disabled=true;bulkCancel.disabled=false;let sent=0;try{for(const id of ids){if(cellBatchCancelled||!currentPeer().has('table')){cellBatchCancelled=true;break;}await api('/api/v1/cell',{method:'POST',body:new URLSearchParams({id,state:value,effect:transition})});sent++;setUiText(bulkStatus,'Sent {sent} of {total} cell edits; awaiting device readback',{sent,total:ids.length});await new Promise(resolve=>setTimeout(resolve,50))}const actual=await api('/api/v1/status');render(actual);if(cellBatchCancelled)setUiText(bulkStatus,'Cancelled unsent edits. {sent} previously sent edits may have applied.',{sent});else if(ids.every(id=>actual.cells[id-1]===value))setUiText(bulkStatus,'Verified {sent} cell states on the device',{sent});else setUiText(bulkStatus,'Readback did not confirm every edit. Refresh and inspect; batch is not atomic.')}catch(error){setUiText(bulkStatus,'Stopped after {sent} edits. Uncertain writes are not retried.',{sent});report(error)}finally{cellBatchBusy=false;bulkApply.disabled=false;bulkCancel.disabled=true}}
paintSelection();
for(let i=0;i<8;i++){const l=document.createElement('label'),c=document.createElement('input');c.type='checkbox';c.onchange=setOutputs;c.dataset.bit=i;l.append(c,document.createTextNode(' '+(i+1)));outputs.appendChild(l)}
let authenticated=false;
async function api(path,options={}){
  const method=String(options.method||'GET').toUpperCase();
  // Never include query strings, bodies, headers, auth fields or raw responses.
  const endpoint=String(path).split('?')[0].slice(0,128);
  try {
    const r=await fetch(path,{credentials:'same-origin',...options}),t=await r.text();
    if(r.status===401&&path!=='/login'){showLogin();throw Object.assign(Error('Please sign in again'),{status:401})}
    if(!r.ok)throw Object.assign(Error(t||r.statusText),{status:r.status});
    if(method!=='GET'&&endpoint!=='/api/v1/clients/report'&&typeof traceBrowserEvent==='function')traceBrowserEvent('info','device.command','HTTP request accepted; device readback may still be required',{source:'http',action:method,endpoint});
    try{return JSON.parse(t)}catch{return t}
  } catch(error) {
    if(typeof traceBrowserEvent==='function')traceBrowserEvent('error','transport.error','HTTP request failed',{source:'http',action:method,endpoint,...browserErrorMetadata(error)},
      {dedupeKey:'http.failure:'+method+endpoint,dedupeWindowMs:3000});
    throw error;
  }
}
function showLogin(){if(typeof settingsInvalidate==="function")settingsInvalidate();authenticated=false;contextReceivedAt=0;renderContext();workspace.hidden=true;loginPanel.hidden=false;stopStream();if(socket){socket.onclose=null;socket.close();socket=null}}
async function openWorkspace(){await refreshAuthenticated();authenticated=true;workspace.hidden=false;loginPanel.hidden=true;loginPassword.value='';connectWs();remoteRefresh();reportBrowser();loadMappings();loadCloud();loadDeviceName();const info=await api('/api/v1/auth/info');accessUser.value=info.username;accessWarning.textContent=info.default_credentials?'Default password active. Change it in Device access.':''}
async function refreshAuthenticated(){render(await api('/api/v1/status'+(cellShape.value!=='rounded'?'?pixels=1':'')))}
async function bootstrap(){try{const info=await api('/api/v1/auth/info');loginUser.value=info.username;loginHint.textContent=!info.ready?'Device access storage is damaged. USB recovery is required.':info.default_credentials?'First login: admin / pakarcontroller. Change these in Device access after signing in.':'Use your configured device credentials.';if(info.ready){try{await openWorkspace()}catch{showLogin()}}}catch(e){loginError.textContent=e.message}}
loginForm.addEventListener('submit',async e=>{e.preventDefault();loginError.textContent='';try{await api('/login',{method:'POST',body:new URLSearchParams({username:loginUser.value,password:loginPassword.value})});await openWorkspace()}catch(error){loginError.textContent=error.message;loginPassword.value=''}});
accessForm.addEventListener('submit',async e=>{e.preventDefault();if(accessPassword.value!==accessConfirm.value)return report('Passwords do not match');try{await api('/api/v1/auth',{method:'POST',body:new URLSearchParams({current_password:accessCurrent.value,username:accessUser.value,password:accessPassword.value})});accessCurrent.value='';accessPassword.value='';accessConfirm.value='';showLogin();await bootstrap();loginHint.textContent='Credentials saved. Sign in with your new password.'}catch(error){report(error)}});
async function loadDeviceName(){try{const value=await api('/api/v1/device/name');if(typeof value.hostname==='string'&&validBoardName(value.display_name)){savedIdentityHost=value.hostname;deviceName.value=value.hostname+'.local';boardDisplayName.value=value.display_name;nameSave.disabled=false}else{nameSave.disabled=true}}catch(e){nameSave.disabled=true;report(e)}}
nameForm.addEventListener('submit',async e=>{e.preventDefault();const name=deviceName.value.toLowerCase().replace(/\.local$/,'');if(!/^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/.test(name)||name==='localhost'||name==='local')return report('Invalid hostname. Use 1..63 letters/digits/hyphens, optionally ending in .local.');const label=boardDisplayName.value;if(!validBoardName(label))return report('Board name must be 1..63 UTF-8 bytes without controls or edge spaces.');try{nameSave.disabled=true;const value=await api('/api/v1/device/name',{method:'POST',body:new URLSearchParams({hostname:name,display_name:label})});if(!value.accepted||!value.persisted||value.hostname!==name||value.display_name!==label)throw Error('Device did not confirm the saved identity');if(!value.apply_pending){const check=await api('/api/v1/device/name');if(check.hostname!==name||check.display_name!==label)throw Error('Identity saved but readback differs. Refresh before retrying.');savedIdentityHost=name;nameSave.disabled=false;nameResult.hidden=true;report('Board name saved and verified. Wi-Fi was not restarted.','success');await refresh();return}stopStream();if(socket){socket.onclose=null;socket.close();socket=null}authenticated=false;nameResult.href='http://'+name+'.local/';nameResult.textContent='Identity saved. Open '+nameResult.href+' and sign in again.';nameResult.hidden=false;report(nameResult.textContent,'success')}catch(error){nameSave.disabled=false;report(error)}});
async function signOut(){try{await api('/logout',{method:'POST'})}catch{}showLogin()}
function report(e,tone){(webToasts??=createToastController({root:notice,translate:uiTr})).showToast(tone||(e instanceof Error?'error':'info'),e instanceof Error?uiTr('Error'):uiTr('Status'),e instanceof Error?e.message:String(e))}
globalThis.addEventListener?.('pagehide',()=>{webToasts?.destroy();webToasts=null});
function renderSegments(masks=[]){displayStop.hidden=!state.tm1637_host_active;const lines=[[8,5,28,5],[31,8,31,27],[31,32,31,51],[8,54,28,54],[5,32,5,51],[5,8,5,27],[8,29.5,28,29.5]];segmentDisplay.innerHTML='';for(let d=0;d<4;d++){const mask=masks[d]||0,svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.setAttribute('viewBox','0 0 36 60');for(let bit=0;bit<7;bit++){const line=document.createElementNS(svg.namespaceURI,'line');['x1','y1','x2','y2'].forEach((k,i)=>line.setAttribute(k,lines[bit][i]));if(mask&(1<<bit))line.setAttribute('class','lit');svg.appendChild(line)}const dot=document.createElementNS(svg.namespaceURI,'circle');dot.setAttribute('cx','32');dot.setAttribute('cy','54');dot.setAttribute('r','2.4');if(mask&128)dot.setAttribute('class','lit');svg.appendChild(dot);segmentDisplay.appendChild(svg)}}
function render(s){state=s;contextReceivedAt=Date.now();renderContext();renderAudio(s);summary.textContent=[s.hostname,s.canonical_url,currentPeer().has('leds')&&Number.isInteger(s.pixels)?s.pixels+' pixels':null].filter(Boolean).join(' · ');avr.textContent=s.avr_online?'online':'offline';avr.className=s.avr_online?'good':'bad';tm.textContent=s.tm1637_ack?'ready':'not responding';tm.className=s.tm1637_ack?'good':'bad';clients.textContent=s.websocket_clients??0;station.textContent=s.station_connected?`${s.station_ip} (${s.station_rssi} dBm)`:'disconnected';ap.textContent=s.ap_started?s.ap_ip:'disabled';discovery.textContent=`mDNS ${s.mdns_started?'on':'off'} · SSDP ${s.ssdp_started?'on':'off'} · NBNS ${s.netbios_started?'on':'off'}`;ntp.textContent=s.ntp_synced?new Date(s.utc_epoch*1000).toLocaleString():'waiting';uart.textContent=`ESP ${s.esp_uart_errors} / AVR ${s.avr_uart_errors} / USB ${s.usb_serial_errors??0}`;mqttState.textContent=s.mqtt_enabled?(s.mqtt_connected?'connected':`offline (${s.mqtt_state})`):'disabled';mqttState.className=s.mqtt_connected?'good':s.mqtt_enabled?'warn':'';cloudWsState.textContent=s.cloud_websocket_enabled?(s.cloud_websocket_connected?'connected':'offline'):'disabled';cloudWsState.className=s.cloud_websocket_connected?'good':s.cloud_websocket_enabled?'warn':'';winner.textContent=s.winner||'—';brightness.value=s.brightness;brightnessValue.textContent=s.brightness;pattern.value=s.effect||'none';renderSegments(s.tm1637_segments);document.querySelectorAll('[data-mode]').forEach(b=>b.classList.toggle('active',b.dataset.mode===s.mode));for(const b of grid.querySelectorAll('.cell')){const id=Number(b.dataset.id),v=s.cells[id-1]||'off';b.classList.toggle('red',v==='red');b.classList.toggle('off',v==='off');if(cellShape.value==='rounded')b.textContent=mapping.names[id-1]||id;if(cellShape.value==='rounded')b.title=`Logical ${id} · physical ${mapping.physical[id-1]||id}`;}paintSelection(true);for(const c of outputs.querySelectorAll('input'))c.checked=!!(s.active_outputs&(1<<Number(c.dataset.bit)))}
async function refresh(){try{render(await api('/api/v1/status'+(cellShape.value!=='rounded'?'?pixels=1':'')))}catch(e){report(e)}}
function connectWs(){
  if(!authenticated||webSocketPaused)return;
  if(webSocketRetry)clearTimeout(webSocketRetry);webSocketRetry=0;
  if(socket){socket.onclose=null;socket.close()}
  const scheme=location.protocol==='https:'?'wss:':'ws:',active=new WebSocket(`${scheme}//${location.host}/ws`);
  socket=active;active.binaryType='arraybuffer';
  active.onopen=()=>{
    if(socket!==active)return;
    wsState.textContent='WebSocket live';wsState.className='good';
    traceBrowserEvent('info','websocket.connected','WebSocket transport connected',{source:'websocket',endpoint:'/ws'});
    active.send('{"type":"status"}');
  };
  active.onmessage=e=>{if(socket!==active)return;if(typeof e.data==='string'){try{const x=JSON.parse(e.data);if(x.hostname)render(x)}catch{}}};
  active.onclose=e=>{
    if(socket!==active||!authenticated||webSocketPaused)return;
    wsState.textContent='WebSocket retrying';wsState.className='warn';stopStream();
    traceBrowserEvent('warn','websocket.disconnected','WebSocket closed; reconnect scheduled',
      {source:'websocket',endpoint:'/ws',error_code:Number.isInteger(e?.code)?e.code:'unknown_close'},
      {dedupeKey:'websocket.closed',dedupeWindowMs:3000});
    webSocketRetry=setTimeout(()=>{webSocketRetry=0;if(authenticated&&!webSocketPaused)refresh().then(()=>connectWs())},1500);
  };
  active.onerror=()=>{if(socket===active)active.close()};
}
async function post(path,data={}){try{if(!contextAllows(path,data))throw Error('This board has not acknowledged this capability');await api(path+'?'+new URLSearchParams(data),{method:'POST'});commandHaptic('command.success');await refresh()}catch(e){commandHaptic('command.error');report(e)}}
const setMode=value=>post('/api/v1/mode',{value}),resetGame=()=>post('/api/v1/game/reset');function setCell(cellState){post('/api/v1/cell',{id:cellId.value,state:cellState,effect:effect.value})}function setPattern(){post('/api/v1/effect',{value:pattern.value})}function setBrightness(value){post('/api/v1/brightness',{value})}function showDisplay(){post('/api/v1/display',{value:displayValue.value})}function showMarquee(){post('/api/v1/display',{text:displayText.value,speed_ms:displaySpeed.value,hold_ms:displayHold.value,repeat:displayRepeat.value,force_scroll:displayScroll.checked,interval_seconds:displayPause.value})}function playTone(){post('/api/v1/buzzer',{frequency:frequency.value,duration_ms:duration.value})}function playCue(){post('/api/v1/audio',{cue:audioCue.value})}function setOutputs(){let mask=0;for(const c of outputs.querySelectorAll('input'))if(c.checked)mask|=1<<Number(c.dataset.bit);post('/api/v1/outputs',{mask})}
function displayControls(){displayPauseLabel.hidden=displayRepeat.value!=='interval';displayHoldLabel.hidden=displayText.value.length>4||displayScroll.checked||displayRepeat.value==='loop'}
displayText.addEventListener('input',displayControls);displayScroll.addEventListener('change',displayControls);displayControls();
async function loadMappings(){try{mapping=await api('/api/v1/cells/mapping');loadMappingEditor(Number(cellId.value)||1);if(state.hostname)render(state)}catch(e){report(e)}}function loadMappingEditor(id){mapLogical.value=id;mapPhysical.value=mapping.physical[id-1]||id;mapName.value=mapping.names[id-1]||id}async function saveMapping(){await post('/api/v1/cells/mapping',{logical_id:mapLogical.value,physical_cell:mapPhysical.value,name:mapName.value});await loadMappings()}
function snakeDirection(value){post('/api/v1/snake/direction',{value})}
async function scanWifi(){try{const x=await api('/api/v1/wifi/scan');ssid.innerHTML='';for(const n of x.networks){const o=document.createElement('option');o.value=n.ssid;o.textContent=`${n.ssid} · ${n.rssi} dBm`;ssid.appendChild(o)}}catch(e){report(e)}}
async function applyNetwork(){await settingsSave('network')}
async function loadCloud(){try{await settingsLoad('cloud')}catch(e){report(e)}}
async function applyCloud(){await settingsSave('cloud')}
async function uploadAvr(){const f=avrFile.files[0];if(!f)return report('Choose a 32 KiB image');const d=new FormData();d.append('image',f);try{report(await api('/api/v1/avr/image',{method:'POST',body:d}))}catch(e){report(e)}}
async function programAvr(){if(!confirm('Back up, program, and byte-verify the AVR?'))return;try{report(await api('/api/v1/avr/program',{method:'POST'}));refresh()}catch(e){report(e)}}
function rgb(h){const f=n=>{const k=(n+h/30)%12;return Math.round((.5-.5*Math.max(-1,Math.min(k-3,9-k,1)))*255)};return[f(0),f(8),f(4)]}
function packet(offset,count,commit){const b=new Uint8Array(10+count*3);b.set([80,75,82,49,1,0,offset&255,offset>>8,count,commit?1:0]);for(let i=0;i<count;i++)b.set(rgb((streamHue+(offset+i)*1.5)%360),10+i*3);return b}
function streamTick(){if(!socket||socket.readyState!==1)return stopStream();socket.send(packet(0,240,false));socket.send(packet(240,240,true));streamHue=(streamHue+4)%360}
function startStream(){if(!currentPeer().has('table')||!currentPeer().has('streaming')||streamTimer||!socket||socket.readyState!==1)return;streamStart.disabled=true;streamStop.disabled=false;streamTimer=setInterval(streamTick,40)}function stopStream(){if(streamTimer)clearInterval(streamTimer);streamTimer=0;streamStart.disabled=false;streamStop.disabled=true}
let remoteJob=null;
function validRemoteJob(j){return j&&j.schema===1&&Number.isInteger(j.job_id)&&j.job_id>=0&&typeof j.boot_id==='string'&&j.boot_id.length>0&&Number.isInteger(j.targets)&&j.targets>=1&&j.targets<=3&&(j.job_id===0||/^[0-9a-f]{64}$/.test(j.manifest_sha256))&&['idle','manifest','download_esp','download_avr','ready','prepare','program_avr','program_esp','awaiting_reboot','verify_pair','release_pair','complete','failed','cancelled'].includes(j.phase)}
function showRemoteJob(j){if(!validRemoteJob(j))throw Error('Invalid remote job identity');remoteJob=j;remoteState.textContent=JSON.stringify(j,null,2);remoteStage.disabled=j.busy===true;remoteApply.disabled=j.phase!=='ready';remoteCancel.disabled=!(j.cancellable===true||j.phase==='ready')}
async function remoteRefresh(){try{showRemoteJob(await api('/api/v1/firmware/remote'))}catch(e){remoteStage.disabled=remoteApply.disabled=remoteCancel.disabled=true;report(e)}}
remoteForm.addEventListener('submit',async e=>{e.preventDefault();if(!/^https:\/\//.test(remoteUrl.value)||!/^([0-9a-f]{64})$/.test(remoteHash.value))return report('Direct HTTPS URL and lowercase manifest SHA-256 required');stopStream();const body=new URLSearchParams({url:remoteUrl.value,sha256:remoteHash.value,targets:remoteTargets.value,apply:'false',bearer:remoteBearer.value});remoteBearer.value='';try{showRemoteJob(await api('/api/v1/firmware/fetch',{method:'POST',body}))}catch(error){report(error)}});
async function remoteAction(action){const j=remoteJob;if(!validRemoteJob(j)||!['apply','cancel'].includes(action))return;if(action==='apply'&&!confirm('Apply this exact verified job to the selected boards? Confirm stable power and correct wiring. Power loss is not an atomic rollback.'))return;const body=new URLSearchParams({job_id:j.job_id,boot_id:j.boot_id,sha256:j.manifest_sha256});try{showRemoteJob(await api('/api/v1/firmware/'+action,{method:'POST',body}))}catch(e){report(e)}}
async function loadTelemetry(){try{telemetryView.textContent=JSON.stringify(await api('/api/v1/telemetry'),null,2)}catch(e){report(e)}}
const browserStart=performance.now(),clientBytes=crypto.getRandomValues(new Uint8Array(16)),browserId=Array.from(clientBytes,b=>b.toString(16).padStart(2,'0')).join('');
async function reportBrowser(){if(!authenticated)return;const reportValue={schema:1,id:browserId,application:'PakarController Web',version:state.version,os:navigator.platform||undefined,transport:'http',uptime_seconds:Math.floor((performance.now()-browserStart)/1000),screen_width:screen.width,screen_height:screen.height};if(Number.isInteger(navigator.hardwareConcurrency))reportValue.cpu_count=navigator.hardwareConcurrency;try{await api('/api/v1/clients/report',{method:'POST',body:new URLSearchParams({report:JSON.stringify(reportValue)})})}catch{}}
setInterval(()=>{if(authenticated){reportBrowser();if(remoteJob)remoteRefresh()}},30000);
setInterval(()=>{if(authenticated&&remoteJob?.busy)remoteRefresh()},2000);
setInterval(()=>{if(authenticated)renderContext()},500);
// BEGIN DEVICE SETTINGS CONTRACT
const DEVICE_SETTINGS={"schema":1,"pages":[{"id":"overview","label":"Overview","icon":"CIRCUITRY","keywords":"device version identity status hardware NTP time discovery build flags","features":[]},{"id":"identity","label":"Board identity","icon":"GLOBE","keywords":"identity hostname rename board name application title icon mDNS AP","features":["identity"]},{"id":"wifi","label":"Wi-Fi","icon":"WIFI_HIGH","keywords":"network station access point recovery SSID password scan","features":["network"]},{"id":"mqtt","label":"MQTT","icon":"BROADCAST","keywords":"cloud broker port topic username password TLS telemetry","features":["network"]},{"id":"websocket","label":"WebSocket","icon":"LINK","keywords":"cloud websocket host port path token TLS telemetry","features":["network"]},{"id":"access","label":"Device access","icon":"LOCK","keywords":"login authentication username password credentials security","features":["network"]},{"id":"leds","label":"LEDs & cells","icon":"GRID_FOUR","keywords":"table LED brightness effects mapping names logical physical modes games","features":["table"]},{"id":"front-panel","label":"Front panel","icon":"GAUGE","keywords":"TM1637 display scrolling marquee once interval melodies buzzer audio","features":["display","text","buzzer"]},{"id":"avr","label":"AVR & inputs","icon":"CPU","keywords":"ATmega328P shift registers outputs ADC6 ninth input UART ISP busy build flags","features":["inputs","outputs","settings"]},{"id":"firmware","label":"Firmware & storage","icon":"FLOPPY_DISK","keywords":"OTA firmware flash EEPROM backup ISP update","features":["isp","ota"]}],"build_only":["link_mode","AVR clock and fuses","pin ownership","WS2811 chipset and channel order","pixel count"],"network":{"station_enabled":"boolean","ap_enabled":"boolean","station_ssid":"string"},"cloud":{"mqtt_enabled":"boolean","mqtt_tls":"boolean","mqtt_host":"string","mqtt_port":"port","mqtt_user":"string","mqtt_base_topic":"string","mqtt_password_set":"boolean","websocket_enabled":"boolean","websocket_tls":"boolean","websocket_host":"string","websocket_port":"port","websocket_path":"string","websocket_token_set":"boolean","telemetry_seconds":"interval"}};
// Adapted actual desktop/src/device_settings.rs Draft/SecretEdit/Write contracts.
// Pealayer HeaderBar's draft/modal lifecycle and PCController card visibility
// controls are bound to capability ACKs, not guessed hardware or default values.
const webSettings={network:null,cloud:null,pending:false,epoch:0,page:'overview',cards:[],ready:false,navSignature:''};
const settingsFields={station_enabled:'stationEnabled',ap_enabled:'apEnabled',station_ssid:'wifiSsid',mqtt_enabled:'mqttEnabled',mqtt_tls:'mqttTls',mqtt_host:'mqttHost',mqtt_port:'mqttPort',mqtt_user:'mqttUser',mqtt_base_topic:'mqttTopic',websocket_enabled:'cloudWsEnabled',websocket_tls:'cloudWsTls',websocket_host:'cloudWsHost',websocket_port:'cloudWsPort',websocket_path:'cloudWsPath',telemetry_seconds:'telemetrySeconds'};
function settingsNode(id){return document.getElementById?.(id)??document.querySelector('#'+id)}
function settingsBounded(value,max,label){if(typeof value!=='string'||new TextEncoder().encode(value).length>max||/[\u0000-\u001f\u007f-\u009f]/.test(value))throw Error(label+' exceeds its UTF-8 limit or contains controls')}
function settingsSnapshot(group,value){
 const spec=DEVICE_SETTINGS[group],result={};
 for(const[key,type]of Object.entries(spec)){
  const v=value?.[key];if(type==='port'||type==='interval'){if(!Number.isInteger(v)||v<1||v>(type==='port'?65535:3600))throw Error('Invalid '+key)}else if(typeof v!==type)throw Error('Missing or invalid '+key);
  result[key]=v;
 }
 settingsValidate(group,result);return result;
}
function settingsValidate(group,c){
 if(group==='network'){settingsBounded(c.station_ssid,32,'SSID');if(!c.station_enabled&&!c.ap_enabled)throw Error('Keep station or recovery AP enabled');if(c.station_enabled&&!c.station_ssid)throw Error('Enter the station SSID');return}
 for(const prefix of ['mqtt','websocket']){settingsBounded(c[prefix+'_host'],63,'Host');if((c[prefix+'_enabled']&&!c[prefix+'_host'])||!/^[A-Za-z0-9.-]*$/.test(c[prefix+'_host']))throw Error('Enter a hostname without scheme or path');if(!Number.isInteger(c[prefix+'_port'])||c[prefix+'_port']<1||c[prefix+'_port']>65535)throw Error('Ports must be 1–65535')}
 for(const[key,max]of [['mqtt_user',32],['mqtt_base_topic',64],['websocket_path',96]]){settingsBounded(c[key],max,key);if(/["\\]/.test(c[key]))throw Error('These public settings do not currently support quotes or backslashes')}
 if(!c.mqtt_base_topic||/[+#]/.test(c.mqtt_base_topic))throw Error('MQTT topic must be nonempty without wildcards');if(!c.websocket_path.startsWith('/'))throw Error('WebSocket path must start with /');if(!Number.isInteger(c.telemetry_seconds)||c.telemetry_seconds<1||c.telemetry_seconds>3600)throw Error('Telemetry interval must be 1–3600 seconds');
}
function settingsEqual(a,b){return !!a&&!!b&&Object.keys(a).length===Object.keys(b).length&&Object.keys(a).every(k=>a[k]===b[k])}
function settingsReadForm(group){const d=webSettings[group];if(!d)throw Error('Read the actual saved settings first');const c={...d.saved};for(const[key,type]of Object.entries(DEVICE_SETTINGS[group])){const n=settingsNode(settingsFields[key]);if(n)c[key]=type==='boolean'?n.checked:type==='port'||type==='interval'?Number(n.value):n.value}return c}
function settingsSecretsDirty(group){return group==='network'?!!settingsNode('wifiPassword')?.value||settingsNode('wifiOpen')?.checked===true:['mqttSecretMode','websocketSecretMode'].some(id=>settingsNode(id)?.value!=='keep')}
function settingsDirty(group){const d=webSettings[group];return !!d&&(!settingsEqual(d.saved,settingsReadForm(group))||settingsSecretsDirty(group))}
function settingsPopulate(group,c){for(const[key,type]of Object.entries(DEVICE_SETTINGS[group])){const n=settingsNode(settingsFields[key]);if(n){if(type==='boolean')n.checked=c[key];else n.value=c[key]}}}
function settingsObserve(group,value){const c=settingsSnapshot(group,value),d=webSettings[group];if(!d||!settingsDirty(group)){webSettings[group]={saved:c,conflict:false};settingsPopulate(group,c)}else if(!settingsEqual(d.saved,c))d.conflict=true;settingsSecretControls()}
function settingsState(message){const n=settingsNode('settingsMessage');if(!n)return;const conflict=['network','cloud'].some(g=>webSettings[g]?.conflict);n.textContent=uiTr(message|| (webSettings.pending?'Waiting for acknowledgment and saved-settings readback…':conflict?'Settings changed on another client. Your edits are preserved; discard and reload before saving.':['network','cloud'].some(settingsDirty)?'Unsaved device edits':'Observed device settings; no pending edits'));n.className=conflict?'warn':'hint';for(const group of ['network','cloud'])for(const key of Object.keys(DEVICE_SETTINGS[group])){const input=settingsNode(settingsFields[key]);if(input)input.disabled=webSettings.pending||!webSettings[group]||!currentPeer().has('network')}for(const[id,group]of [['wifiPassword','network'],['wifiOpen','network'],['mqttSecretMode','cloud'],['websocketSecretMode','cloud'],['mqttPassword','cloud'],['cloudWsToken','cloud']]){const input=settingsNode(id);if(input)input.disabled=webSettings.pending||!webSettings[group]||!currentPeer().has('network')||(id==='mqttPassword'&&settingsNode('mqttSecretMode').value!=='replace')||(id==='cloudWsToken'&&settingsNode('websocketSecretMode').value!=='replace')}for(const id of ['networkSave','cloudSave','settingsReload']){const b=settingsNode(id);if(b)b.disabled=webSettings.pending||(id!=='settingsReload'&&(!currentPeer().has('network')||!webSettings[id==='networkSave'?'network':'cloud']||conflict))}}
async function settingsLoad(group){if(webSettings.pending)return;const epoch=webSettings.epoch;const value=await api('/api/v1/'+(group==='network'?'network':'cloud'));if(epoch===webSettings.epoch&&!webSettings.pending)settingsObserve(group,value)}
function settingsClearSecrets(){for(const id of ['wifiPassword','mqttPassword','cloudWsToken']){const n=settingsNode(id);if(n)n.value=''}for(const id of ['mqttSecretMode','websocketSecretMode']){const n=settingsNode(id);if(n)n.value='keep'}const open=settingsNode('wifiOpen');if(open)open.checked=false;settingsSecretControls()}
function settingsInvalidate(){webSettings.epoch++;webSettings.network=webSettings.cloud=null;webSettings.pending=false;settingsClearSecrets();if(settingsNode('deviceSettingsDialog')?.open)settingsNode('deviceSettingsDialog').close();settingsState('Reconnect to read device settings')}
function settingsSecretControls(){for(const[mode,field,presence,label]of [['mqttSecretMode','mqttPassword','mqtt_password_set','password'],['websocketSecretMode','cloudWsToken','websocket_token_set','token']]){const m=settingsNode(mode),n=settingsNode(field);if(m&&n){n.disabled=m.value!=='replace';const row=n.closest?.('.settings-field');if(row)row.hidden=m.value!=='replace';const keep=m.querySelector?.('option[value="keep"]');if(keep)keep.textContent=uiTr(webSettings.cloud?.saved[presence]===true?'Keep stored '+label:'Leave '+label+' unset')}}settingsState()}
function settingsWrite(group){
 const d=webSettings[group];if(!d||d.conflict)throw Error('Reload the current saved settings before saving');const expected=settingsReadForm(group);settingsValidate(group,expected);const form={...expected};
 if(group==='network'){const password=settingsNode('wifiPassword').value,open=settingsNode('wifiOpen').checked;settingsBounded(password,63,'Wi-Fi password');if(expected.station_enabled&&!open&&(new TextEncoder().encode(password).length<8))throw Error('Enter an 8–63 byte Wi-Fi password, or explicitly select an open network');form.ssid=form.station_ssid;delete form.station_ssid;form.password=open?'':password}
 else{for(const[modeId,input,key,presence]of [['mqttSecretMode','mqttPassword','mqtt_password','mqtt_password_set'],['websocketSecretMode','cloudWsToken','websocket_token','websocket_token_set']]){const mode=settingsNode(modeId).value;if(!['keep','replace','clear'].includes(mode))throw Error('Invalid secret mode');if(mode!=='keep'){const v=mode==='clear'?'':settingsNode(input).value;settingsBounded(v,64,'Secret');if(mode==='replace'&&!v)throw Error('Enter a replacement secret or explicitly choose Clear');form[key]=v;expected[presence]=mode==='replace'}delete form[presence]}}
 return {form,expected};
}
async function settingsSave(group){
 if(webSettings.pending||!currentPeer().has('network'))return;let write;try{write=settingsWrite(group)}catch(e){settingsState(e.message);return}
 if(group==='network'&&!confirm('Save this Wi-Fi policy? Connectivity may change. The station password is replaced on every save.'))return;
 const epoch=++webSettings.epoch;webSettings.pending=true;settingsState();
 try{
  const current=settingsSnapshot(group,await api('/api/v1/'+group));if(epoch!==webSettings.epoch)return;if(!settingsEqual(current,webSettings[group].saved)){webSettings[group].conflict=true;throw Error('Another client changed these settings. Discard and reload before saving.')}
  const ack=await api('/api/v1/'+group,{method:'POST',body:new URLSearchParams(write.form)});if(epoch!==webSettings.epoch)return;
  if(ack?.[group==='network'?'accepted':'updated']!==true)throw Error('Device did not confirm a saved configuration write');
  const actual=settingsSnapshot(group,await api('/api/v1/'+group));if(epoch!==webSettings.epoch)return;
  if(!settingsEqual(write.expected,actual))throw Error('Write accepted, but readback differs. Refresh before retrying; no write was replayed.');
  webSettings[group]={saved:actual,conflict:false};settingsPopulate(group,actual);if(group==='network'){settingsNode('wifiPassword').value='';settingsNode('wifiOpen').checked=false}else{for(const id of ['mqttPassword','cloudWsToken'])settingsNode(id).value='';for(const id of ['mqttSecretMode','websocketSecretMode'])settingsNode(id).value='keep'}settingsSecretControls();settingsState('Saved public settings verified by device readback. Secret values remain write-only.');
 }catch(e){if(epoch===webSettings.epoch)settingsState(e.message+' Uncertain writes are not retried.')}
 finally{if(epoch===webSettings.epoch){webSettings.pending=false;settingsState(settingsNode('settingsMessage').textContent)}}
}
function settingsCanClose(){if(webSettings.pending){settingsState('Wait for the current operation to finish');return false}return !['network','cloud'].some(settingsDirty)||confirm('Close settings with unsaved edits? They remain in this session; no changes will be sent.')}
function closeDeviceSettings(){if(settingsCanClose())settingsNode('deviceSettingsDialog').close()}
async function settingsReload(){if(webSettings.pending)return;if(['network','cloud'].some(settingsDirty)&&!confirm('Discard network/cloud drafts and secret edits, then read the device again?'))return;webSettings.network=webSettings.cloud=null;settingsClearSecrets();try{await settingsLoad('network');await settingsLoad('cloud')}catch(e){settingsState(e.message)}}
function settingsPageAvailable(page){return !page.features.length||page.features.some(f=>currentPeer().has(f))}
function settingsNavigate(id){const page=DEVICE_SETTINGS.pages.find(p=>p.id===id&&settingsPageAvailable(p));if(!page)return;webSettings.page=id;for(const card of webSettings.cards)card.hidden=!card.dataset.settingsPages.split(' ').includes(id)||!currentPeer().has(card.dataset.capability);for(const section of settingsNode('settingsPages').querySelectorAll('[data-cloud-domain]'))section.hidden=section.dataset.cloudDomain!==id;settingsNode('settingsOverview').hidden=id!=='overview';settingsNode('settingsWorkspaceLinks').hidden=!['leds','front-panel','avr'].includes(id);settingsNode('settingsPageTitle').textContent=uiTr(page.label);for(const b of settingsNode('settingsNavigation').querySelectorAll('button'))b.setAttribute('aria-current',b.dataset.page===id?'page':'false');settingsState()}
function settingsNavigation(){const nav=settingsNode('settingsNavigation');if(!nav)return;const query=settingsNode('settingsSearch').value.trim().toLowerCase(),pages=DEVICE_SETTINGS.pages.filter(p=>settingsPageAvailable(p)),signature=JSON.stringify([query,pages.map(p=>p.id)]);if(signature!==webSettings.navSignature){webSettings.navSignature=signature;nav.replaceChildren();for(const page of pages){if(!(page.label+' '+uiTr(page.label)+' '+page.keywords).toLowerCase().includes(query))continue;const b=document.createElement('button');b.type='button';b.dataset.page=page.id;b.textContent=uiTr(page.label);b.onclick=()=>settingsNavigate(page.id);nav.appendChild(b)}}if(!settingsPageAvailable(DEVICE_SETTINGS.pages.find(p=>p.id===webSettings.page)))webSettings.page='overview';for(const b of nav.querySelectorAll('button'))b.setAttribute('aria-current',b.dataset.page===webSettings.page?'page':'false')}
function settingsBuildInfo(){const n=settingsNode('buildInformation');if(!n)return;const observed={};if(currentPeer().known)for(const k of ['version','link_mode','pixels','avr_clock_hz'])if(Object.hasOwn(state,k))observed[k]=state[k];n.textContent=Object.keys(observed).length?JSON.stringify(observed,null,2):uiTr('Build information not reported by this peer');settingsNavigation();if(document.getElementById)settingsNavigate(webSettings.page)}
async function openDeviceSettings(page='overview'){settingsNavigation();settingsNavigate(page);settingsNode('deviceSettingsDialog').showModal();if(currentPeer().has('network'))try{await settingsLoad('network');await settingsLoad('cloud')}catch(e){settingsState(e.message)}}
function settingsReveal(feature){closeDeviceSettings();if(settingsNode('deviceSettingsDialog').open)return;const card=[...document.querySelectorAll('.layout .card[data-capability]')].find(c=>c.dataset.capability===feature);if(card&&currentPeer().has(feature)){panelPreferences[card.dataset.panelId]={hidden:false,collapsed:false};savePanelPreferences();applyPanelVisibility();card.scrollIntoView({behavior:'smooth',block:'start'})}}
function initializeDeviceSettings(){
 const destinations={'Board identity':'identity','Wi-Fi':'wifi','Cloud links':'mqtt websocket','Device access':'access','Verified remote update':'firmware','Firmware vault':'firmware'};
 for(const card of document.querySelectorAll('.layout .card')){const page=destinations[card.querySelector('h2')?.textContent];if(page){card.dataset.settingsPages=page;webSettings.cards.push(card);settingsNode('settingsPages').appendChild(card)}}
 settingsNode('settingsSearch').addEventListener('input',settingsNavigation);settingsNode('deviceSettingsDialog').addEventListener('cancel',e=>{e.preventDefault();closeDeviceSettings()});
 for(const group of ['network','cloud'])for(const key of Object.keys(DEVICE_SETTINGS[group])){const n=settingsNode(settingsFields[key]);n?.addEventListener('input',()=>settingsState());n?.addEventListener('change',()=>settingsState())}
 for(const id of ['wifiPassword','wifiOpen','mqttSecretMode','websocketSecretMode','mqttPassword','cloudWsToken']){settingsNode(id)?.addEventListener('input',()=>settingsState());settingsNode(id)?.addEventListener('change',settingsSecretControls)}
 settingsNode('ssid').addEventListener('change',()=>{settingsNode('wifiSsid').value=settingsNode('ssid').value;settingsState()});
 globalThis.addEventListener?.('beforeunload',e=>{if(webSettings.pending||['network','cloud'].some(settingsDirty)){e.preventDefault();e.returnValue=''}});
 initializeCloudFields();initializeNetworkFields();settingsSecretControls();settingsNavigation();webSettings.ready=true;
}
function initializeNetworkFields(){
 const card=webSettings.cards.find(c=>c.dataset.settingsPages==='wifi');if(!card)return;
 const heading=card.querySelector('h2'),scan=settingsNode('ssid').parentElement,save=settingsNode('networkSave');settingsNode('ssid').setAttribute('aria-label',uiTr('Scanned networks'));
 const fields=[['wifiSsid','Station SSID'],['wifiPassword','Station password · re-enter on save'],['wifiOpen','Open station network · no password'],['stationEnabled','Station enabled'],['apEnabled','Recovery AP enabled']].map(([id,title])=>{const row=document.createElement('label');row.className='settings-field';const label=document.createElement('span');setUiText(label,title);row.append(label,settingsNode(id));return row});
 const hint=document.createElement('p');hint.className='hint';setUiText(hint,'The station password is write-only. Re-enter it to save Wi-Fi changes, or explicitly select an open network. Keep at least one network path enabled.');card.replaceChildren(heading,scan,...fields,hint,save);
}
function initializeCloudFields(){
 const card=webSettings.cards.find(c=>c.dataset.settingsPages==='mqtt websocket');if(!card)return;
 const heading=card.querySelector('h2'),buttons=[...card.querySelectorAll('button')],hint=card.querySelector('.hint');
 const groups=[['mqtt',[['mqttEnabled','Enable MQTT'],['mqttTls','TLS'],['mqttHost','Broker hostname'],['mqttPort','Port'],['mqttUser','Username'],['mqttTopic','Base topic'],['mqttSecretMode','Password handling'],['mqttPassword','Replacement password']]],['websocket',[['cloudWsEnabled','Enable outbound WebSocket'],['cloudWsTls','TLS'],['cloudWsHost','Server hostname'],['cloudWsPort','Port'],['cloudWsPath','Path'],['websocketSecretMode','Token handling'],['cloudWsToken','Replacement token']]]];
 const sections=groups.map(([domain,fields])=>{const section=document.createElement('div');section.dataset.cloudDomain=domain;section.className='settings-fields';for(const[id,label]of fields){const row=document.createElement('label');row.className='settings-field';const text=document.createElement('span');setUiText(text,label);row.append(text,settingsNode(id));section.appendChild(row)}return section});
 const interval=document.createElement('label');interval.className='settings-field';const label=document.createElement('span');setUiText(label,'Telemetry interval · seconds');interval.append(label,settingsNode('telemetrySeconds'));const actions=document.createElement('div');actions.className='row';actions.append(...buttons);card.replaceChildren(heading,...sections,interval,actions);if(hint)card.appendChild(hint);
}
if(document.getElementById)initializeDeviceSettings();

// Adapted actual PCController CardLayoutEditor/Frame hidden/collapsed actions.
// Native panel identity and ordering remain independent browser preferences;
// capability loss always wins over visibility preferences.
let workspacePanels=[],panelPreferences={},panelMenuSignature='';
try{panelPreferences=JSON.parse(localStorage.getItem('pakar-panels')||'{}')}catch{}
function savePanelPreferences(){const encoded=JSON.stringify(panelPreferences);try{if(localStorage.getItem('pakar-panels')!==encoded)localStorage.setItem('pakar-panels',encoded)}catch(e){report(e)}}
function applyPanelVisibility(){for(const panel of workspacePanels){const p=panelPreferences[panel.dataset.panelId]||{};panel.hidden=!currentPeer().has(panel.dataset.capability)||p.hidden===true;panel.classList.toggle('collapsed',p.collapsed===true);panel.querySelector('[aria-expanded]')?.setAttribute('aria-expanded',String(p.collapsed!==true))}renderPanelMenu()}
function panelToggle(id,key){const panel=workspacePanels.find(p=>p.dataset.panelId===id);if(!panel||!currentPeer().has(panel.dataset.capability))return;panelPreferences[id]={...panelPreferences[id],[key]:!panelPreferences[id]?.[key]};savePanelPreferences();applyPanelVisibility()}
function renderPanelMenu(){const menu=settingsNode('webPanelsMenu');if(!menu)return;const panels=workspacePanels.filter(panel=>currentPeer().has(panel.dataset.capability)),signature=JSON.stringify([uiLanguage,panels.map(p=>p.dataset.panelId),panelPreferences]);if(signature===panelMenuSignature)return;panelMenuSignature=signature;menu.replaceChildren();for(const panel of panels){const b=document.createElement('button');b.type='button';b.setAttribute('role','menuitemcheckbox');b.setAttribute('aria-checked',String(panelPreferences[panel.dataset.panelId]?.hidden!==true));b.textContent=uiTr(panelPreferences[panel.dataset.panelId]?.hidden?'Show':'Hide')+' '+uiTr(panel.dataset.panelLabel);b.onclick=()=>panelToggle(panel.dataset.panelId,'hidden');menu.appendChild(b)}if(!menu.children.length){const p=document.createElement('p');p.className='hint';p.textContent=uiTr('No acknowledged workspace panels');menu.appendChild(p)}const reset=document.createElement('button');reset.type='button';reset.textContent=uiTr('Reset panel layout');reset.onclick=()=>{panelPreferences={};savePanelPreferences();applyPanelVisibility()};menu.appendChild(reset)}
function initializeWorkspacePanels(){for(const panel of document.querySelectorAll('.layout .card[data-capability]')){const title=panel.querySelector('h2');if(!title)continue;panel.dataset.panelId=title.textContent.toLowerCase().replace(/[^a-z0-9]+/g,'-');panel.dataset.panelLabel=title.textContent;panel.classList.add('workspace-panel');const header=document.createElement('header');header.className='panel-heading';header.appendChild(title);const collapse=document.createElement('button');collapse.type='button';collapse.textContent='⌄';collapse.setAttribute('aria-label','Collapse or expand '+title.textContent);collapse.setAttribute('aria-expanded','true');collapse.onclick=()=>panelToggle(panel.dataset.panelId,'collapsed');header.appendChild(collapse);header.addEventListener('contextmenu',e=>{e.preventDefault();const menu=settingsNode('webPanelsMenu');menu.parentElement.open=true;menu.querySelector('button')?.focus()});panel.prepend(header);workspacePanels.push(panel)}applyPanelVisibility()}
if(document.getElementById)initializeWorkspacePanels();

// Independent socket lifecycle. A delayed close from an obsolete socket must
// never schedule another connection or repaint the currently selected state.
let webSocketPaused=false,webSocketRetry=0;
function pauseWebSocket(){webSocketPaused=true;if(webSocketRetry)clearTimeout(webSocketRetry);webSocketRetry=0;stopStream();if(socket){socket.onclose=null;socket.close();socket=null}wsState.textContent='HTTP refresh · WS off';wsState.className='';}
function reconnectWebSocket(){pauseWebSocket();webSocketPaused=false;connectWs()}

if(document.getElementById){
 document.addEventListener('keydown',e=>{const input=e.target?.matches?.('input,textarea,select,[contenteditable=true]');if((e.ctrlKey||e.metaKey)&&e.shiftKey&&e.key===','){e.preventDefault();openDeviceSettings()}else if((e.ctrlKey||e.metaKey)&&e.key===','){e.preventDefault();appearanceOpen.click()}else if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='f'&&currentPeer().has('table')&&!settingsNode('deviceSettingsDialog').open){e.preventDefault();cellSearch.focus();cellSearch.select()}else if(e.key==='F5'&&!input){e.preventDefault();refresh()}});
 setInterval(()=>{if(authenticated&&settingsNode('deviceSettingsDialog').open&&!webSettings.pending&&currentPeer().has('network')){settingsLoad('network').catch(e=>settingsState(e.message));settingsLoad('cloud').catch(e=>settingsState(e.message))}},15000);
}
// END DEVICE SETTINGS CONTRACT
initializeLocalization();
bootstrap();setInterval(()=>{if(authenticated&&(cellShape.value!=='rounded'||!socket||socket.readyState!==1))refresh()},2000);
</script></body></html>
)HTML";
