import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/main.dart';

import 'connection_lifecycle_test.dart' show LifecycleApi;

void main() {
  for (final width in [390.0, 800.0]) {
    for (final scale in [1.0, 1.4]) {
      testWidgets('Connection choice keeps two lines at ${width}px × $scale', (
        tester,
      ) async {
        tester.view.physicalSize = Size(width, 1100);
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);
        final api = LifecycleApi();
        await tester.pumpWidget(
          MaterialApp(
            home: MediaQuery(
              data: MediaQueryData(textScaler: TextScaler.linear(scale)),
              child: Scaffold(
                body: SettingsPage(
                  api: api,
                  reconnect: () async {},
                  active: false,
                ),
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        final selector = find.byType(DropdownButtonFormField<ConnectionMethod>);
        final field = tester.widget<DropdownButton<ConnectionMethod>>(
          find.descendant(
            of: selector,
            matching: find.byType(DropdownButton<ConnectionMethod>),
          ),
        );
        // A dense DropdownButton forces a 24px selected child, even when menu
        // itemHeight is larger. The title/detail rows require their real height.
        expect(field.isDense, isFalse);
        expect(find.text('Network'), findsOneWidget);
        expect(find.text('Wi-Fi or Ethernet'), findsOneWidget);
        final title = tester.getRect(find.text('Network'));
        final detail = tester.getRect(find.text('Wi-Fi or Ethernet'));
        expect(detail.top, greaterThanOrEqualTo(title.bottom));
        expect(tester.takeException(), isNull);

        await tester.tap(selector);
        await tester.pumpAndSettle();
        expect(find.text('USB OTG').hitTestable(), findsOneWidget);
        expect(find.text('Bluetooth LE').hitTestable(), findsOneWidget);
        expect(tester.takeException(), isNull);
        expect(api.writes, 0);
        await tester.pumpWidget(const SizedBox());
        await tester.pump();
        await api.close();
      });
    }
  }
}
