import 'package:flutter/material.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/panel_inventory.dart';

import 'peer_fixture.dart';
import 'widget_test.dart' show TestApi;

Widget hardware(DeviceStatus? status, [PanelInventory? inventory]) =>
    MaterialApp(
      home: Scaffold(
        body: HardwarePage(status: status, inventory: inventory),
      ),
    );

Future<void> expandCapabilities(WidgetTester tester) async {
  await tester.scrollUntilVisible(
    find.text('Capabilities'),
    150,
    scrollable: find.byType(Scrollable).first,
  );
  await tester.tap(find.text('Capabilities'));
  await tester.pumpAndSettle();
}

void main() {
  setUp(() {
    SharedPreferences.setMockInitialValues({});
    FlutterSecureStorage.setMockInitialValues({});
  });

  testWidgets('Overview retains identity but no capability details', (
    tester,
  ) async {
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(body: BoardOverview(status: peerFixture())),
      ),
    );
    expect(find.text('Table board'), findsOneWidget);
    expect(find.text('ledboard'), findsOneWidget);
    expect(find.byType(Chip), findsNothing);
    expect(find.text('Acknowledged capabilities'), findsNothing);
    expect(find.text('Capabilities'), findsNothing);
  });

  testWidgets('Hardware owns every acknowledged capability exactly once', (
    tester,
  ) async {
    final status = peerFixture();
    await tester.pumpWidget(hardware(status));
    await expandCapabilities(tester);
    expect(find.text('Capabilities'), findsOneWidget);
    expect(find.byType(Chip), findsNWidgets(status.peer.features.length));
    for (final feature in status.peer.features) {
      expect(find.byKey(ValueKey('capability:$feature')), findsOneWidget);
    }
    expect(find.text('No acknowledged hardware measurements'), findsNothing);
  });

  testWidgets(
    'A capability-only board can open Hardware without network telemetry',
    (tester) async {
      await tester.pumpWidget(
        PakarControllerApp(
          api: TestApi(
            snapshot: peerFixture(features: ['identity'], kind: 'controller'),
          ),
        ),
      );
      await tester.pump();
      await tester.pump();
      expect(find.text('Hardware'), findsWidgets);
      expect(find.text('Capabilities'), findsNothing);
      await tester.tap(find.text('Hardware').last);
      await tester.pump();
      expect(find.text('Capabilities'), findsOneWidget);
      await expandCapabilities(tester);
      expect(find.byKey(const ValueKey('capability:identity')), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );

  testWidgets(
    'Unavailable capabilities stay dimmed without becoming fresh ACKs',
    (tester) async {
      final status = peerFixture(
        features: ['identity', 'inputs'],
        kind: 'controller',
      );
      final inventory = PanelInventory()..observe(status);
      await tester.pumpWidget(hardware(status, inventory));
      await expandCapabilities(tester);
      final chip = find.byKey(const ValueKey('capability:inputs'));
      double opacity() => tester
          .widget<Opacity>(
            find.ancestor(of: chip, matching: find.byType(Opacity)).first,
          )
          .opacity;
      expect(opacity(), 1);
      await tester.pumpWidget(hardware(null, inventory));
      expect(chip, findsOneWidget);
      expect(opacity(), 0.45);
      expect(find.text('0x00'), findsNothing);
      expect(inventory.has('inputs'), true); // Presentation only.
      await tester.pumpWidget(hardware(status, inventory));
      expect(opacity(), 1);

      final replacement = peerFixture(
        features: ['identity'],
        kind: 'controller',
        hostname: 'otherboard',
      );
      inventory.observe(replacement);
      await tester.pumpWidget(hardware(replacement, inventory));
      expect(
        chip,
        findsNothing,
        reason: 'Never retain another board’s capabilities',
      );
    },
  );

  testWidgets('Unknown board cannot populate hardware capabilities', (
    tester,
  ) async {
    await tester.pumpWidget(
      hardware(DeviceStatus({'hostname': 'ledboard', 'pixels': 480})),
    );
    expect(find.byType(Chip), findsNothing);
    expect(find.text('No acknowledged hardware measurements'), findsOneWidget);
  });

  testWidgets('Hardware capabilities and measurements fit phone and tablet', (
    tester,
  ) async {
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    for (final size in [const Size(390, 800), const Size(1100, 1000)]) {
      tester.view.physicalSize = size;
      tester.view.devicePixelRatio = 1;
      for (final scale in [1.0, 2.0]) {
        await tester.pumpWidget(
          MaterialApp(
            builder: (context, child) => MediaQuery(
              data: MediaQuery.of(context)
                  .copyWith(textScaler: TextScaler.linear(scale)),
              child: child!,
            ),
            home: Scaffold(body: HardwarePage(status: peerFixture())),
          ),
        );
        await tester.pump();
        await tester.scrollUntilVisible(
          find.text('Capabilities'),
          150,
          scrollable: find.byType(Scrollable).first,
        );
        expect(find.text('Capabilities'), findsOneWidget);
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      }
    }
  });
}
