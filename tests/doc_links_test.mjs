import assert from 'node:assert/strict';
import {mkdirSync, writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {checkGuides, localLinkReference, markdownAnchors} from '../tools/check-doc-links.mjs';
const root = resolve('build/tests/doc-links');
mkdirSync(root, {recursive: true});
writeFileSync(resolve(root, 'guide.md'), '# Hello\n# Hello\n## **سلام**\n<a id="manual"></a>\n````md\n```\n# Not a heading\n````\n');
assert.deepEqual([...markdownAnchors('# One\n# One\n# One-1\n# One')], ['one','one-1','one-1-1','one-2']);
assert.equal(localLinkReference('https://example.invalid/a'), null);
assert.equal(localLinkReference('//example.invalid/a'), null);
assert.deepEqual(localLinkReference('<guide.md#hello-1>'), {target:'guide.md',fragment:'hello-1'});
writeFileSync(resolve(root, 'README.md'), '[ok](guide.md#hello-1) [rtl](guide.md#سلام) <a href="guide.md#manual">x</a>\n```\n[example](missing.md)\n```');
assert.deepEqual(checkGuides(root, ['README.md']), []);
for (const link of ['guide.md#not-a-heading','missing.md','../outside.md','guide.md#%zz']) {
  writeFileSync(resolve(root, 'README.md'), `[bad](${link})`);
  assert.equal(checkGuides(root, ['README.md']).length, 1, link);
}
assert.equal(checkGuides(root, ['absent.md']).length, 1);
console.log('Donor-adapted documentation checks: local links, headings, fences, Unicode, HTML, boundaries and malformed URLs passed.');
