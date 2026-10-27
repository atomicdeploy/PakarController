// Widget/model regressions stay separate from real HTTP/WS and USB ownership
// tests so their real-clock transports cannot inherit the widget fake clock.
import 'pixel_correction_default_preview_test.dart' as defaults;
import 'pixel_correction_menu_test.dart' as menu;
import 'pixel_correction_intent_test.dart' as intents;
import 'pixel_corrections_editor_test.dart' as editor;
import 'pixel_corrections_exit_test.dart' as exit_guard;
import 'pixel_correction_preview_test.dart' as lease;
import 'pixel_correction_mapping_test.dart' as mapping;
import 'pixel_corrections_test.dart' as corrections;
import 'avr_command_wire_test.dart' as avr;
import 'appearance_live_test.dart' as appearance;
import 'table_viewport_test.dart' as viewport;
import 'status_bar_layout_test.dart' as footer;

void main() {
  defaults.main();
  menu.main();
  intents.main();
  editor.main();
  exit_guard.main();
  lease.main();
  mapping.main();
  corrections.main();
  avr.main();
  appearance.main();
  viewport.main();
  footer.main();
}
