// One focused canonical invocation; no production transport or device writes.
import 'cell_pixel_dialog_test.dart' as dialog;
import 'cell_pixel_preview_test.dart' as colors;
import 'pixel_corrections_editor_test.dart' as corrections;
import 'pixel_corrections_test.dart' as saved_corrections;
import 'pixel_tools_navigation_test.dart' as navigation;
import 'pixel_diagnostic_advanced_test.dart' as advanced;
import 'pixel_diagnostic_test.dart' as probes;
import 'pixel_diagnostic_page_test.dart' as page;

void main() {
  dialog.main();
  colors.main();
  corrections.main();
  saved_corrections.main();
  navigation.main();
  advanced.main();
  probes.main();
  page.main();
}
