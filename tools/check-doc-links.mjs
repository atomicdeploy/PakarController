// Adapted from PCController .github/scripts/repository-check.mjs and
// repository-policy.mjs (MIT option; docs/licenses/PCController-MIT.txt).
// Scope: maintained entry guides, not immutable donor snapshots or old receipts.
import {existsSync, readFileSync, statSync} from 'node:fs';
import {dirname, relative, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

export const entryGuides = [
  'README.md', 'CONTRIBUTING.md', 'SECURITY.md', 'docs/README.md',
  'docs/SCRIPT-TOOLING.md', 'docs/HOST-WIFI.md', 'docs/USER-FILES.md',
];

function stripHeadingTags(value) {
  let result = '';
  for (let index = 0; index < value.length; index += 1) {
    if (value[index] === '<') {
      const closing = value.indexOf('>', index + 1);
      if (closing >= 0) index = closing;
      continue;
    }
    if (value[index] !== '>') result += value[index];
  }
  return result;
}

function githubHeadingSlug(value) {
  const label = value.replace(/!\[([^\]]*)\]\([^)]*\)/gu, '$1')
    .replace(/\[([^\]]+)\]\([^)]*\)/gu, '$1').replace(/[`*_~]/gu, '');
  return stripHeadingTags(label)
    .replace(/&(?:amp|lt|gt|quot|#39);/gu, entity => ({
      '&amp;': '&', '&lt;': '', '&gt;': '', '&quot;': '"', '&#39;': "'",
    })[entity]).trim().normalize('NFKC').toLocaleLowerCase('en-US')
    .replace(/[^\p{Letter}\p{Number}\p{Mark}\s_-]/gu, '').replace(/\s/gu, '-');
}

// Retain donor fence/duplicate-heading semantics; also handle longer fences
// correctly so embedded examples cannot accidentally end the outer code block.
export function proseLines(text) {
  let marker = '', length = 0;
  return String(text).split(/\r?\n/gu).map(line => {
    const fence = line.match(/^\s{0,3}(`{3,}|~{3,})(.*)$/u);
    if (fence) {
      if (!marker) { marker = fence[1][0]; length = fence[1].length; }
      else if (fence[1][0] === marker && fence[1].length >= length && !fence[2].trim()) marker = '';
      return '';
    }
    return marker ? '' : line;
  });
}

export function markdownAnchors(text) {
  const anchors = new Set(), counts = new Map();
  for (const line of proseLines(text)) {
    for (const match of line.matchAll(/<a\s+(?:id|name)=["']([^"']+)["'][^>]*>/giu)) anchors.add(match[1]);
    const heading = line.match(/^\s{0,3}#{1,6}[\t ]+(.+?)(?:[\t ]+#+)?[\t ]*$/u);
    if (!heading) continue;
    const base = githubHeadingSlug(heading[1]);
    if (!base) continue;
    // GitHub deduplicates across explicit numeric suffixes too.
    let count = counts.get(base) || 0, slug = count ? `${base}-${count}` : base;
    while (anchors.has(slug)) slug = `${base}-${++count}`;
    counts.set(base, count + 1); anchors.add(slug);
  }
  return anchors;
}

export function localLinkReference(rawTarget) {
  let target = rawTarget.trim();
  if (target.startsWith('<') && target.endsWith('>')) target = target.slice(1, -1);
  else target = target.split(/\s+["']/u, 1)[0];
  if (!target || /^[a-z][a-z0-9+.-]*:/iu.test(target) || target.startsWith('//')) return null;
  const hash = target.indexOf('#'), fragment = hash >= 0 ? target.slice(hash + 1) : '';
  target = (hash >= 0 ? target.slice(0, hash) : target).split('?', 1)[0];
  return {target: decodeURIComponent(target), fragment: decodeURIComponent(fragment)};
}

export function checkGuides(root, guides = entryGuides) {
  const errors = [], cache = new Map();
  for (const guide of guides) {
    const path = resolve(root, guide);
    if (!existsSync(path) || !statSync(path).isFile() || !statSync(path).size) {
      errors.push(`${guide}: required guide missing or empty`); continue;
    }
    const prose = proseLines(readFileSync(path, 'utf8')).join('\n');
    // Same inline Markdown links as donor, plus our branded HTML image/link tags.
    const links = [...prose.matchAll(/!?\[[^\]]*\]\(([^)]+)\)/gu)].map(match => match[1]);
    for (const match of prose.matchAll(/<(?:a|img)\b[^>]*?\b(?:href|src)=["']([^"']+)["'][^>]*>/giu)) links.push(match[1]);
    for (const link of links) {
      let ref;
      try { ref = localLinkReference(link); } catch { errors.push(`${guide}: malformed link: ${link}`); continue; }
      if (!ref) continue;
      const dest = ref.target ? resolve(ref.target.startsWith('/') ? root : dirname(path), ref.target.replace(/^\//u, '')) : path;
      const owned = relative(root, dest);
      if (owned === '..' || owned.startsWith('../') || owned.startsWith('..\\') || !existsSync(dest)) {
        errors.push(`${guide}: missing/outside local link: ${link}`); continue;
      }
      if (ref.fragment && dest.toLowerCase().endsWith('.md')) {
        if (!cache.has(dest)) cache.set(dest, markdownAnchors(readFileSync(dest, 'utf8')));
        if (!cache.get(dest).has(ref.fragment)) errors.push(`${guide}: missing heading: ${link}`);
      }
    }
  }
  return errors;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const errors = checkGuides(fileURLToPath(new URL('../', import.meta.url)));
  for (const error of errors) console.error(error);
  if (errors.length) process.exitCode = 1;
  else console.log(`Documentation links passed: ${entryGuides.length} maintained entry guides; no external requests.`);
}
