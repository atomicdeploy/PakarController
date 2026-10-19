import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/app_support.dart';
import 'package:pakarcontroller/app_updates_page.dart';
import 'package:pakarcontroller/update_center.dart';

import 'update_pages_test.dart' show updatePageFixture;

void main() {
  testWidgets(
    'One update workspace retains app support without an attached board',
    (tester) async {
      final api = PakarApi();
      final apps = updatePageFixture();
      final support = AppSupportSession(
        api: () => api,
        observed: () => null,
        health: () => {},
      );
      addTearDown(api.close);
      addTearDown(apps.dispose);
      addTearDown(support.dispose);
      await tester.pumpWidget(
        MaterialApp(
          home: UpdateCenter(
            api: api,
            apps: apps,
            support: support,
            authorized: (_) => false,
            busyChanged: (_) {},
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(find.text('Updates & support'), findsOneWidget);
      expect(find.text('ESP / AVR'), findsNothing);
      await tester.scrollUntilVisible(
        find.text('Start support session'),
        450,
        scrollable: find.descendant(
          of: find.byType(AppUpdatesPage),
          matching: find.byType(Scrollable),
        ).first,
      );
      expect(support.running, false);
      expect(tester.takeException(), isNull);
    },
  );
}
