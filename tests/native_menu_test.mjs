// Source/import guard; native paint and pointer tests live in ui/menu.rs.
// This is not a substitute for executing those tests or native screenshots.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
const read = path => readFileSync(new URL(`../${path}`, import.meta.url), 'utf8');
const source = read('desktop/src/ui/menu.rs');
const helper = source.slice(source.indexOf('pub fn top_menu_button<R>'), source.indexOf('#[cfg(test)]'))
  .trim().replace(/^pub /, '').replace(/\s+/g, '');
assert.equal(createHash('sha256').update(helper).digest('hex'),
  '4fe943c0efa5b88bd612e61ccb0e2277f9861461f4ca433b12523d38a0b70a2b',
  'actual donor helper must match after application ID/visibility/whitespace adaptation');
const shell = read('desktop/src/ui/shell.rs');
const menu = shell.split('egui::Panel::top("pakar-menu")')[1]
  .split('ui.with_layout(egui::Layout::right_to_left')[0];
assert.equal((menu.match(/menu::top_menu_button\(/g) || []).length, 4);
assert.equal((menu.match(/\.shortcut_text\(/g) || []).length, 6);
for (const binding of ['device_settings', 'connect', 'cell_mapping', 'preferences', 'fullscreen', 'refresh'])
  assert.ok(menu.includes(`.shortcut_text(&shortcuts.${binding})`));
assert.ok(!menu.includes('Ctrl+'), 'shortcut text must come from persisted bindings, not labels');
// The user requested a height explanation without changing the height policy.
const config = read('desktop/src/config.rs');
for (const unchanged of ['font.size = font.size.min(13.0);',
  'style.spacing.item_spacing = eframe::egui::vec2(8.0, 6.0);',
  'crate::ui::control_density::configure(style, config.ui_control_density);']) assert.ok(config.includes(unchanged));
for (const test of ['shortcuts_are_separate_muted_right_aligned_atoms_in_both_themes',
  'sibling_hover_switches_only_an_already_open_menu_and_clears_closed_state',
  'production_menu_rows_use_shortcut_atoms_and_imported_root_menu_helper']) assert.ok(source.includes(`fn ${test}(`));
console.log('Native menu source guard passed: actual donor helper, four headings, six shortcut atoms, height policy unchanged. Paint/pointer acceptance requires Rust tests.');
