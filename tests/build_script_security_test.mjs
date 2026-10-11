import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {decodeHtmlSourceText} from '../tools/html-source-text.mjs';
assert.equal(decodeHtmlSourceText('&amp; &lt; &gt;'),'& < >');
assert.equal(decodeHtmlSourceText('&amp;lt;SCRIPT&amp;gt;'),'&lt;SCRIPT&gt;');
assert.equal(decodeHtmlSourceText('&amp;amp; &unknown;'),'&amp; &unknown;');
for(const path of ['web_appearance_test.mjs','web_remote_test.mjs']){
  const source=readFileSync(new URL(path,import.meta.url),'utf8');
  assert(source.includes('match(/<script>([\\s\\S]*?)<\\/script>/i)'),path+' extracts mixed-case tags too');
}
const art=readFileSync(new URL('../tools/generate-readme-artwork.mjs',import.meta.url),'utf8');
assert(!art.includes('existsSync'));
assert(art.includes("if(error.code!=='ENOENT')throw error"),'Only missing artwork permits regeneration');
assert.match('<ScRiPt>real source</sCrIpT>',/<script>([\s\S]*?)<\/script>/i);
console.log('Build-script CodeQL regressions: single-pass entities, mixed-case script extraction and direct read/ENOENT handling.');
