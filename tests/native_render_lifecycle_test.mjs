// Source regression only: actual painting/assertion execution belongs to Cargo.
// Restore Pealayer's clear-before-assert headless texture ownership contract.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../'+path,import.meta.url),'utf8');
const test=(path,name)=>{
  const source=read(path),start=source.indexOf('fn '+name+'(');
  assert(start>=0,name);
  return source.slice(start).split('\n    #[test]')[0];
};
const branding=test('desktop/src/branding.rs','header_and_about_share_one_cached_texture_in_both_themes');
assert.match(branding,/let uploaded_master = output[\s\S]*?output\.textures_delta\.clear\(\);\s*assert!\(uploaded_master\)/);
assert.match(branding,/let uploaded_again = output[\s\S]*?output\.textures_delta\.clear\(\);\s*assert!\(\s*!uploaded_again/);
const surface=test('desktop/src/ui/cell_table.rs','led_state_and_emphasis_never_tint_the_neutral_cell_surface');
assert.match(surface,/output\.textures_delta\.clear\(\);[\s\S]*?let fill = output/);
assert(surface.includes('cell_base_fill(crate::config::live().color_palette, dark, 1)'));
const ports=test('desktop/src/ui/connection_controls.rs','offline_dropdowns_open_independently_and_usb_enumerates_only_when_open');
assert.match(ports,/out\.textures_delta\.clear\(\);\s*\(out, actions, responses/);
assert(ports.includes('assert_eq!(actions, vec![Action::Port("COM7".into())]);'));
const fonts=test('desktop/src/ui/i18n.rs','changing_language_preserves_widget_fonts_and_monospace_fallback');
assert.equal(fonts.match(/drop_without_applying_deltas\(\)/g)?.length,4);
assert(fonts.includes('assert!(defs.font_data.contains_key("elegance-symbols"));'));
const menu=read('desktop/src/ui/menu.rs');
const shortcut=test('desktop/src/ui/menu.rs','shortcuts_are_separate_muted_right_aligned_atoms_in_both_themes');
assert.match(shortcut,/output\.textures_delta\.clear\(\);[\s\S]*?let text = \|value/);
assert(menu.includes('ctx.tessellate('));
const compact=source=>source.replace(/\s+/g,'');
assert(compact(menu).includes('mesh.vertices.into_iter().map(|v|v.color)'));
assert(compact(shortcut).includes('binding_colors.iter().all(|color|*color==weak)'));
assert(compact(shortcut).includes('label_colors.iter().all(|color|*color==primary)'));
assert(compact(shortcut).includes('letprimary=style.visuals.override_text_color.unwrap_or(style.visuals.widgets.inactive.fg_stroke.color)'));
assert(shortcut.includes('assert_ne!(primary, style.visuals.text_color());'));
for(const palette of ['Studio','Native']) {
  for(const dark of ['false','true']) {
    assert(shortcut.includes(`(${dark}, Some(crate::ui::palette::Palette::${palette}))`));
  }
}
for(const role of ['text','muted']) assert(shortcut.includes(`dark, "${role}"`));
assert(!shortcut.includes('galley.job.sections[0].format.color'));
assert(test('desktop/src/ui/menu.rs','headings').includes('drop_without_applying_deltas()'));
console.log('Native render lifecycle source checks passed; actual egui execution and production pixels remain separate.');
