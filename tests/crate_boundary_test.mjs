import assert from 'node:assert/strict';
import {checkCrateBoundary} from '../tools/check-crate-boundary.mjs';

const single = {library: null, binary: 'mod config;\nmod ui;\nfn main() {}', build: 'resources.compile();'};
assert.equal(checkCrateBoundary(single).architecture, 'single-binary');
assert.throws(() => checkCrateBoundary({...single, binary: 'mod config;\nmod config;'}), /same contract module twice/);
const shared = {...single, library: 'pub mod startup;\npub mod config;', binary: 'fn main() { pakarcontroller::startup::run() }'};
assert.equal(checkCrateBoundary(shared).architecture, 'shared-library');
assert.throws(() => checkCrateBoundary({...shared, binary: 'mod config;\nfn main() { pakarcontroller::startup::run() }'}), /reuse library modules/);
assert.throws(() => checkCrateBoundary({...shared, binary: 'mod ui;\nfn main() { pakarcontroller::startup::run() }'}), /thin entry point/);
assert.throws(() => checkCrateBoundary({...shared, binary: 'fn main() {}'}), /delegate/);
for (const build of ['out.join("resource.lib")', 'println!("cargo:rustc-link-arg-bin=pakarcontroller");\n// resource.lib']) {
  assert.throws(() => checkCrateBoundary({...single, build}), /winres|linked twice/);
}
console.log('Actual adapted Pealayer crate/resource guards pass current/future architecture and reject six regressions; no compiler or fixture writes.');
