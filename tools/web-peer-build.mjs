// Build the browser's own release identity; never substitute live board status.
import {readFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
export async function webPeerSource(source) {
  const release=JSON.parse(await readFile(new URL('../interfaces/client-release.json',import.meta.url),'utf8'));
  const policy=JSON.parse(await readFile(new URL('../interfaces/peer-policy.json',import.meta.url),'utf8'));
  assert.equal(release.product,'PakarController');assert.match(release.version,/^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/);
  assert(Number.isSafeInteger(release.build)&&release.build>=1);assert.equal(typeof policy.enforce,'boolean');
  const identity={application:'pakarcontroller-web',version:release.version,build:release.build,
    minimum_board:policy.minimum_board,maximum_board:policy.maximum_board,enforcePeerVersions:policy.enforce};
  assert.equal((source.match(/^const PAKAR_WEB_BUILD = .*;$/gm)||[]).length,1,'Exactly one generated browser identity');
  return source.replace(/^const PAKAR_WEB_BUILD = .*;$/m,'const PAKAR_WEB_BUILD = '+JSON.stringify(identity)+';');
}
