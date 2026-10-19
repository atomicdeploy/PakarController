import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/main.dart';

void main() {
  testWidgets(
    'Command fallback saves offline without reconnecting or losing drafts',
    (tester) async {
      SharedPreferences.setMockInitialValues({});
      final api = PakarApi()..disconnect();
      addTearDown(api.close);
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SettingsPage(
              api: api,
              reconnect: () async => fail('Policy edits must not reconnect'),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await tester.enterText(
        find.widgetWithText(TextField, 'Device hostname or URL'),
        'draft.local',
      );
      final revision = api.connectionRevision;
      final toggle = find.byKey(const Key('command-http-fallback'));
      await tester.ensureVisible(toggle);
      await tester.tap(toggle);
      await tester.pumpAndSettle();
      expect(api.commandHttpFallback, false);
      expect(api.connectionRevision, revision);
      expect(
        (await SharedPreferences.getInstance()).getBool(
          'command_http_fallback',
        ),
        false,
      );
      expect(find.text('draft.local'), findsOneWidget);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );
}
