import 'package:flutter/material.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_corrections_page.dart';
import 'package:pakarcontroller/pixel_diagnostic_page.dart';
import 'package:pakarcontroller/workspace_widgets.dart';

import 'connection_lifecycle_test.dart' show LifecycleApi;
import 'peer_fixture.dart';

DeviceStatus pixelPeer({required bool corrections}) => DeviceStatus({
  ...peerFixture(
    features: [
      'table',
      'leds',
      'streaming',
      'pixel-diagnostic',
      'command-ack',
      if (corrections) 'pixel-corrections',
    ],
  ).json,
  'board_id': 'pixel-menu-fixture',
  'boot_id': 'fixture-boot',
  'pixels': 480,
});

class PixelMenuApi extends LifecycleApi {
  int correctionReads = 0;
  @override
  DeviceStatus? get freshStatus => observed;
  @override
  Future<Map<String, dynamic>> getJson(String path) async {
    expect(path, '/api/pixels/corrections');
    correctionReads++;
    return {
      'enabled': true,
      'revision': 1,
      'pixels': 480,
      'rules': <dynamic>[],
      'sources': [0, 1, 2],
    };
  }
}

void main() {
  setUp(() {
    SharedPreferences.setMockInitialValues({});
    FlutterSecureStorage.setMockInitialValues({});
  });

  test('Pixel corrections capability survives the acknowledged feature filter only when advertised', () {
    expect(pixelPeer(corrections: true).peer.has('pixel-corrections'), true);
    expect(pixelPeer(corrections: false).peer.has('pixel-corrections'), false);
    final mixed = pixelPeer(corrections: true).json;
    mixed['capabilities']['hostname'] = 'different-board';
    expect(DeviceStatus(mixed).peer.has('pixel-corrections'), false);
  });

  for (final supported in [true, false]) {
    testWidgets(
      'Actual overflow menu opens capability-scoped linked pixel tabs: corrections=$supported',
      (tester) async {
        tester.view.physicalSize = const Size(1200, 900);
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);
        final api = PixelMenuApi()
          ..healthyWs = true
          ..observed = pixelPeer(corrections: supported);
        await tester.pumpWidget(PakarControllerApp(api: api));
        await tester.pumpAndSettle();
        await tester.tap(find.byTooltip('Device context menu'));
        await tester.pumpAndSettle();
        expect(find.text('Pixel diagnostics…'), findsOneWidget);
        expect(find.text('Devices…'), findsOneWidget);
        expect(
          tester.getTopLeft(find.text('Devices…')).dy,
          lessThan(tester.getTopLeft(find.text('Pixel diagnostics…')).dy),
        );
        expect(
          tester.getTopLeft(find.text('Pixel diagnostics…')).dy,
          lessThan(tester.getTopLeft(find.text('Updates & support…')).dy),
        );
        expect(
          find.text('Pixel corrections…'),
          supported ? findsOneWidget : findsNothing,
        );
        await tester.tap(
          find.text(supported ? 'Pixel corrections…' : 'Pixel diagnostics…'),
        );
        await tester.pumpAndSettle();
        if (supported) {
          Finder tab(String label) => find.descendant(
            of: find.byType(TabBar),
            matching: find.text(label),
          );
          expect(find.byType(PixelCorrectionsPage), findsOneWidget);
          expect(tab('Diagnostics'), findsOneWidget);
          expect(tab('Corrections'), findsOneWidget);
          expect(
            find.descendant(
              of: find.byType(TabBar),
              matching: find.byIcon(pixelCorrectionIcon),
            ),
            findsOneWidget,
          );
          expect(
            find.descendant(
              of: find.byType(TabBar),
              matching: find.byIcon(pixelDiagnosticIcon),
            ),
            findsOneWidget,
          );
          final tabs = tester.widget<TabBar>(find.byType(TabBar)).controller!;
          expect(tabs.index, 1);
          await tester.tap(tab('Diagnostics'));
          await tester.pumpAndSettle();
          expect(tabs.index, 0);
          expect(find.byType(PixelDiagnosticPage), findsOneWidget);
          await tester.tap(tab('Corrections'));
          await tester.pumpAndSettle();
          expect(tabs.index, 1);
          expect(api.correctionReads, 1);
        } else {
          expect(find.byType(TabBar), findsNothing);
          expect(find.byType(PixelCorrectionsPage), findsNothing);
          expect(find.byType(PixelDiagnosticPage), findsOneWidget);
          expect(api.correctionReads, 0);
        }
        expect(
          api.writes,
          0,
          reason: 'Opening tools and switching tabs never changes the production output',
        );
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox.shrink());
        await tester.pump();
      },
    );
  }
}
