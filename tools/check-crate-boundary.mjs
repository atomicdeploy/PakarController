// Adapted from Pealayer scripts/check-crate-boundary.mjs, donor SHA-256
// da9b469ac596e6eb1999e4d499742a51ebf289335285549569116c1124963e38.
// Pakar currently has one binary. Enforce the shared-library checks automatically
// if lib.rs is introduced, instead of silently omitting this useful donor tool.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {pathToFileURL} from 'node:url';

const modules = source => [...source.matchAll(/^\s*(?:pub\s+)?mod\s+(\w+)\s*;/gm)].map(match => match[1]);
export function checkCrateBoundary({library, binary, build}) {
  const entryModules = modules(binary);
  assert.equal(new Set(entryModules).size, entryModules.length,
    'The binary must not declare the same contract module twice');
  if (library !== null) {
    const shared = new Set(modules(library));
    assert.deepEqual(entryModules.filter(name => shared.has(name)), [],
      'The binary must reuse library modules, not compile a second set of contracts and process globals');
    assert.match(binary, /pakarcontroller::/, 'The executable must delegate to the authoritative library');
    assert.equal(entryModules.length, 0, 'The executable must remain a thin entry point');
  }
  assert.doesNotMatch(build, /println!\([^\n]*rustc-link-arg-bin[^\n]*\)[\s\S]{0,100}resource\.lib/,
    'MSVC resources must not be linked twice through the library and binary');
  assert.doesNotMatch(build, /join\("resource\.lib"\)/,
    'winres already links the MSVC resource object');
  return {architecture: library === null ? 'single-binary' : 'shared-library', modules: entryModules.length};
}

if (process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url) {
  const binary = await readFile(new URL('../desktop/src/main.rs', import.meta.url), 'utf8');
  const build = await readFile(new URL('../desktop/build.rs', import.meta.url), 'utf8');
  let library;
  try { library = await readFile(new URL('../desktop/src/lib.rs', import.meta.url), 'utf8'); }
  catch (error) { if (error.code !== 'ENOENT') throw error; library = null; }
  console.log('Crate contract guard passed:', checkCrateBoundary({library, binary, build}));
}
