// One living product contract. This guard is independent of API implementation.
import {execFileSync} from 'node:child_process';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url));
export function violations(path, source) {
  source=source.replaceAll('\\"','"'); // Also inspect emitted C++ JSON literals.
  const rules=[
    ['numbered REST namespace', /\/api\/v\d+(?=[/\s"'\x60]|$)/g],
    ['product contract selector', /\b(?:schemaVersion|schema_version|api_version|apiVersion|protocol_version|protocolVersion|contractVersion|contract_version|kProtocolVersion|kEnvelopeSchema)\b/g],
    ['numbered product discriminator', /(?:pakarcontroller-v\d+|pakar-nodemcu-atmega328p-v\d+|pakar-locale-v\d+|PKR1)/g],
    ['JSON generation field', /(?:["']schema["']\s*:|\[\s*(?:F\()?["']schema["']\)?\s*\]|\.schema\s*(?:===|!==)|pub\s+schema\s*:|self\.schema\b)/g],
  ];
  return rules.flatMap(([reason,re])=>[...source.matchAll(re)].map(m=>({
    path,line:source.slice(0,m.index).split('\n').length,reason,marker:m[0]
  })));
}
export function check() {
  // Persisted binary layouts are deliberately not interpreted as JSON/API
  // generations. They remain checksum-validated; see API-POLICY.md.
  const paths=execFileSync('git',['ls-files','firmware','desktop/src','mobile/lib','services','interfaces','assets/web','assets/contracts','libraries/BoardCore/src',
    'tools/create-remote-manifest.mjs','tools/telemetry-reassembly.mjs','tools/set-device-name.ps1','tools/set-device-access.ps1','tools/prepare-release.mjs'],{cwd:root,encoding:'utf8'}).trim().split(/\r?\n/);
  const errors=paths.filter(p=>!p.includes('/pccontroller/')&&/\.(?:ino|rs|dart|h|cpp|js|mjs|json|ps1)$/.test(p))
    .flatMap(p=>violations(p,readFileSync(resolve(root,p),'utf8')));
  if(errors.length)throw Error('Living API policy violated:\n'+errors.map(e=>e.path+':'+e.line+' '+e.reason+' '+e.marker).join('\n'));
  return paths.length;
}
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url))
  console.log('Living API guard: '+check()+' tracked shipping sources inspected; no product generations.');
