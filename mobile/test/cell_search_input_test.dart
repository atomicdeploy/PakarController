import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/table_workspace.dart';

import 'peer_fixture.dart';

void main() {
  test(
    'Indicators default on and multiselect off, explicit saved choices survive',
    () {
      final fresh = TableWorkspace();
      expect(fresh.showIndicators, true);
      expect(fresh.multiple, false);
      final restored = TableWorkspace(
        saved: {'indicators': false, 'multiple': true},
      );
      expect(restored.showIndicators, false);
      expect(restored.multiple, true);
      final roundtrip = TableWorkspace(saved: restored.snapshot());
      expect(roundtrip.showIndicators, false);
      expect(roundtrip.multiple, true);
      fresh.dispose();
      restored.dispose();
      roundtrip.dispose();
    },
  );

  testWidgets(
    'Find uses number keypad, punctuation helpers and canonicalizes without commands',
    (tester) async {
      final workspace = TableWorkspace();
      var commands = 0;
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: TablePage(
              status: peerFixture(),
              mapping: CellMap.defaults(),
              workspace: workspace,
              command: (path, [values = const {}]) async {
                commands++;
              },
            ),
          ),
        ),
      );
      final field = find.widgetWithText(TextField, 'Find logical cell');
      expect(
        tester.widget<TextField>(field).keyboardType,
        const TextInputType.numberWithOptions(signed: true),
      );
      expect(tester.widget<TextField>(field).enableSuggestions, false);
      await tester.enterText(field, '۳، ۱,۲,۳');
      await tester.pump();
      expect(workspace.query.text, '3,1,2,3');
      expect(workspace.selection, {1, 2, 3});
      await tester.testTextInput.receiveAction(TextInputAction.done);
      await tester.pump();
      expect(workspace.query.text, '1-3');
      await tester.enterText(field, '12');
      await tester.tap(find.byTooltip('Insert range dash'));
      await tester.pump();
      expect(workspace.query.text, '12-');
      await tester.enterText(field, '12-14');
      await tester.tap(find.byTooltip('Insert list comma'));
      await tester.pump();
      expect(workspace.query.text, '12-14,');
      await tester.enterText(field, '12-14,80');
      tester.widget<TextField>(field).focusNode!.unfocus();
      await tester.pump();
      expect(workspace.query.text, '12-14, 80');
      expect(workspace.selection, {12, 13, 14, 80});
      expect(commands, 0);
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );
}
