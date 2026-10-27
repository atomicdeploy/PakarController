import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/host_wifi.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  test('network hints use reported SSIDs, never hostname inference', () {
    expect(
      HostWifi.knownNetworks({
        'hostname': 'not-an-ssid',
        'station_ssid': 'router',
      }),
      ['router'],
    );
    expect(HostWifi.knownNetworks({'ap_ssid': 'a', 'station_ssid': 'a'}), [
      'a',
    ]);
    expect(HostWifi.knownNetworks(null), isEmpty);
  });
  test('SSID byte bounds and credentials remain separate from API login', () {
    expect(HostWifi.validate('ledcontroller', '', true), isNull);
    expect(HostWifi.validate('ledcontroller', '12345678', false), isNull);
    expect(HostWifi.validate('é' * 17, '', true), isNotNull);
    expect(HostWifi.validate('a\nb', '', true), isNotNull);
    expect(HostWifi.validate('board', '', false), isNotNull);
    expect(HostWifi.validate('board', 'x' * 64, false), isNotNull);
  });
  test(
    'invalid input never invokes native networking; open omits password',
    () async {
      final calls = <MethodCall>[];
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(HostWifi.channel, (call) async {
            calls.add(call);
            return 'cancelled';
          });
      addTearDown(
        () => TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
            .setMockMethodCallHandler(HostWifi.channel, null),
      );
      await expectLater(
        HostWifi.request('join', '', '', true),
        throwsFormatException,
      );
      expect(calls, isEmpty);
      expect(
        await HostWifi.request('join', 'board', 'unused secret', true),
        'cancelled',
      );
      expect(calls.single.arguments['password'], '');
    },
  );
  testWidgets(
    'phone dialog remains usable with larger text and scrollable controls',
    (tester) async {
      SharedPreferences.setMockInitialValues({});
      tester.view.physicalSize = const Size(360, 740);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      await tester.pumpWidget(
        MaterialApp(
          home: Builder(
            builder: (context) => MediaQuery(
              data: MediaQuery.of(context)
                  .copyWith(textScaler: const TextScaler.linear(1.3)),
              child: Builder(
                builder: (context) => TextButton(
                  onPressed: () =>
                      showHostWifi(context, knownNetworks: ['a' * 32]),
                  child: const Text('Open'),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.tap(find.text('Open'));
      await tester.pumpAndSettle();
      expect(tester.takeException(), isNull);
      await tester.ensureVisible(find.text('Release app-only Wi-Fi'));
      expect(find.text('Close').hitTestable(), findsOneWidget);
      expect(tester.takeException(), isNull);
      await tester.tap(find.text('Close'));
      await tester.pumpAndSettle();
      expect(find.text('This device’s Wi-Fi'), findsNothing);
    },
  );
  testWidgets(
    'opening shows explicit actions without requesting network; saves no password',
    (tester) async {
      SharedPreferences.setMockInitialValues({});
      final calls = <MethodCall>[];
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(HostWifi.channel, (call) async {
            calls.add(call);
            return 'saved';
          });
      addTearDown(
        () => TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
            .setMockMethodCallHandler(HostWifi.channel, null),
      );
      await tester.pumpWidget(
        MaterialApp(
          home: Builder(
            builder: (context) => TextButton(
              onPressed: () => showHostWifi(
                context,
                scope: 'board-1',
                knownNetworks: ['known-network'],
              ),
              child: const Text('Open'),
            ),
          ),
        ),
      );
      await tester.tap(find.text('Open'));
      await tester.pumpAndSettle();
      expect(calls, isEmpty);
      await tester.tap(find.text('known-network'));
      await tester.pumpAndSettle();
      await tester.enterText(find.byType(TextField).last, '12345678');
      await tester.ensureVisible(find.text('Save in Android'));
      await tester.tap(find.text('Save in Android'));
      await tester.pumpAndSettle();
      expect(calls.single.method, 'save');
      expect(find.textContaining('Network saved in Android.'), findsOneWidget);
      final prefs = await SharedPreferences.getInstance();
      expect(prefs.getString('pakar.host-wifi.board-1'), 'known-network');
      expect(prefs.getKeys(), {'pakar.host-wifi.board-1'});
    },
  );
}
