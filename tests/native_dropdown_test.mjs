// Wiring/provenance checks, not execution of egui keyboard or painted pixels.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const dropdown=read('desktop/src/ui/dropdown.rs');
const icons=read('desktop/src/ui/icons.rs');
const colors=read('desktop/src/ui/color_picker.rs');
const prefs=read('desktop/src/ui/preferences.rs');
const shell=read('desktop/src/ui/shell.rs');
const settings=read('desktop/src/ui/device_settings.rs');
const controls=read('desktop/src/ui/connection_controls.rs');
for(const name of ['menu_ui','clear_menu','register_search','register_choice','choice']) {
  assert(dropdown.includes(`pub fn ${name}`), name);
}
for(const name of ['dropdown_arrows_seed_focus_skip_disabled_commit_enter_and_cancel_escape',
  'dropdown_search_keeps_text_focus_until_arrow_enters_choices',
  'dropdown_color_rows_are_full_width_equal_height_and_hover_stable']) assert(dropdown.includes(`fn ${name}`));
assert(dropdown.includes('output.textures_delta.clear();\n                    for row in &rows'));
assert(dropdown.includes('egui::StrokeKind::Inside'));
assert(dropdown.includes('entries.iter().filter')||dropdown.includes('.filter(|entry| entry.enabled)'));
for(const hook of ['register_search','register_choice','menu_ui','clear_menu','ComboBox']) assert(icons.includes(`super::dropdown::${hook}`));
assert(!icons.includes('egui::ComboBox::'));
assert(!prefs.includes('egui::ComboBox::'));
assert(prefs.includes('DropdownUiExt')&&prefs.includes('ui.dropdown_value('));
assert(prefs.includes('super::color_picker::accent_picker(ui,&mut draft.config.accent'),
  'the imported Custom/recent palette must be consumed by real Preferences');
for(const source of [shell,settings]) {
  assert(!source.includes('egui::ComboBox::'));
  assert(source.includes('DropdownUiExt')&&source.includes('ui.dropdown_value('));
}
assert(!controls.includes('▾'));
assert(controls.includes('icons::CARET_DOWN'));
assert.equal(controls.match(/dropdown::menu_ui\(ui, owner/g)?.length,2);
assert.equal(controls.match(/dropdown::clear_menu\(ui.ctx\(\), owner/g)?.length,2);
for(const hook of ['remember_color','custom_menu_row','palette_popup']) assert(colors.includes(`pub fn ${hook}`));
assert(colors.includes('colors.truncate(12)'));
assert(colors.includes('data.get_temp::<Vec<egui::Color32>>'));
assert(!colors.includes('insert_persisted'));
for(const label of ['Recent colors','Shared colors','Choose color']) {
  assert(colors.includes(`super::i18n::text("${label}")`));
  assert(read('desktop/src/ui/i18n.rs').includes(`"${label}" =>`));
}
assert(read('tools/build-desktop.ps1').includes('-Concern native-dropdowns -Gate'));
assert(read('desktop/src/ui/mod.rs').includes('pub mod dropdown;'));
console.log('Actual donor dropdown/color/consumer wiring passed; native execution and runtime visuals remain separate.');
