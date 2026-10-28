import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/signal_indicator.dart';

import 'peer_fixture.dart';

void main() {
  setUp(() => SharedPreferences.setMockInitialValues({}));

  Future<void> show(
    WidgetTester tester, {
    int rssi = -57,
    VoidCallback? onConnection,
  }) async {
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: Center(
            child: SignalIndicator(rssi: rssi, onConnection: onConnection),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();
  }

  testWidgets('Signal defaults to an icon and tapping opens connection', (
    tester,
  ) async {
    var opened = 0;
    await show(tester, onConnection: () => opened++);
    expect(find.byIcon(Icons.network_wifi_3_bar), findsOneWidget);
    expect(find.text('-57 dBm'), findsNothing);
    expect(
      tester.getSize(find.byKey(const Key('status-signal'))),
      const Size(48, 48),
    );
    await tester.tap(find.byKey(const Key('status-signal')));
    expect(opened, 1);
    expect(tester.takeException(), isNull);
  });

  testWidgets(
    'Long press selects either or both and persists across remounts',
    (tester) async {
      var opened = 0;
      await show(tester, onConnection: () => opened++);
      for (final (label, saved, hasIcon, hasValue) in [
        ('Icon and value', 'both', true, true),
        ('Value only', 'value', false, true),
        ('Icon only', 'icon', true, false),
      ]) {
        await tester.longPress(find.byKey(const Key('status-signal')));
        await tester.pumpAndSettle();
        expect(find.text('Icon only'), findsOneWidget);
        expect(find.text('Value only'), findsOneWidget);
        expect(find.text('Icon and value'), findsOneWidget);
        await tester.tap(find.ancestor(
          of: find.text(label), matching: find.byType(CheckedPopupMenuItem<SignalDisplay>),
        ));
        await tester.pumpAndSettle();
        expect(opened, 0);
        expect(
          (await SharedPreferences.getInstance()).getString(
            SignalIndicator.preferenceKey,
          ),
          saved,
        );
        expect(
          find.byIcon(Icons.network_wifi_3_bar),
          hasIcon ? findsOneWidget : findsNothing,
        );
        expect(find.text('-57 dBm'), hasValue ? findsOneWidget : findsNothing);
        await tester.pumpWidget(const SizedBox());
        await show(tester, onConnection: () => opened++);
        expect(
          find.byIcon(Icons.network_wifi_3_bar),
          hasIcon ? findsOneWidget : findsNothing,
        );
        expect(find.text('-57 dBm'), hasValue ? findsOneWidget : findsNothing);
        expect(tester.takeException(), isNull);
      }
    },
  );

  testWidgets('Signal strength changes without resetting the display choice', (
    tester,
  ) async {
    SharedPreferences.setMockInitialValues({
      SignalIndicator.preferenceKey: 'both',
    });
    for (final (rssi, icon) in [
      (-50, Icons.signal_wifi_4_bar),
      (-60, Icons.network_wifi_3_bar),
      (-70, Icons.network_wifi_2_bar),
      (-80, Icons.network_wifi_1_bar),
      (-95, Icons.signal_wifi_0_bar),
      (0, Icons.signal_wifi_0_bar),
    ]) {
      await show(tester, rssi: rssi);
      expect(find.byIcon(icon), findsOneWidget);
      expect(find.text(rssi == 0 ? '—' : '$rssi dBm'), findsOneWidget);
      expect(tester.getSize(find.byKey(const Key('status-signal'))).width, 116);
    }
  });

  testWidgets(
    'Footer stays bounded on phones and tablets with signal and errors',
    (tester) async {
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      for (final display in SignalDisplay.values) {
        SharedPreferences.setMockInitialValues({
          SignalIndicator.preferenceKey: display.name,
        });
        for (final width in [320.0, 800.0]) {
          for (final direction in TextDirection.values) {
            tester.view.physicalSize = Size(width, 800);
            tester.view.devicePixelRatio = 1;
            final status = peerFixture();
            status.json.addAll({
              'station_connected': true,
              'station_ip': '192.168.4.1',
              'station_rssi': -57,
            });
            await tester.pumpWidget(const SizedBox());
            await tester.pumpWidget(
              MaterialApp(
                builder: (context, child) => MediaQuery(
                  data: MediaQuery.of(context)
                      .copyWith(textScaler: TextScaler.linear(3)),
                  child: Directionality(
                    textDirection: direction,
                    child: child!,
                  ),
                ),
                home: Scaffold(
                  body: Align(
                    alignment: Alignment.bottomCenter,
                    child: StatusBar(
                      status: status,
                      message: 'Connected',
                      commandFailed: true,
                    ),
                  ),
                ),
              ),
            );
            await tester.pumpAndSettle();
            expect(
              tester.getSize(find.byType(StatusBar)).height,
              inInclusiveRange(48, 80),
            );
            expect(find.byKey(const Key('status-signal')), findsOneWidget);
            expect(tester.takeException(), isNull);
          }
        }
      }
    },
  );
}
