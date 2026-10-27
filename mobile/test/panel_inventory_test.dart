import 'package:flutter/material.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/panel_inventory.dart';

import 'connection_lifecycle_test.dart' show LifecycleApi;
import 'peer_fixture.dart';

DeviceStatus observation({
  bool peripherals = true,
  String board = 'board-A',
  String boot = 'boot-A',
}) => DeviceStatus({
  ...peerFixture(
    features: peripherals
        ? const [
            'network',
            'telemetry',
            'table',
            'cell-batch',
            'leds',
            'display',
            'text',
            'buzzer',
          ]
        : const ['network', 'telemetry', 'table', 'cell-batch', 'leds'],
    displayAck: peripherals,
  ).json,
  'board_id': board,
  'boot_id': boot,
  'avr_online': peripherals,
});

void main() {
  setUp(() {
    SharedPreferences.setMockInitialValues({});
    FlutterSecureStorage.setMockInitialValues({});
  });

  test('Retained panel identity is presentation only, never a fresh capability ACK', () {
    final inventory = PanelInventory()..observe(observation());
    final retainedOwner = inventory.ownerKey;
    final unavailable = observation(peripherals: false);
    inventory.observe(unavailable);
    expect(inventory.frontPanel, true);
    expect(inventory.has('display'), true);
    expect(unavailable.peer.has('display'), false);
    expect(unavailable.peer.allows('/api/audio', {'cue': 'welcome'}), false);
    inventory.observe(observation(peripherals: false, boot: 'boot-B'));
    expect(inventory.ownerKey, retainedOwner);
    inventory.observe(
      DeviceStatus({...observation().json, 'capabilities': null}),
    );
    expect(inventory.ownerKey, retainedOwner);
    expect(
      inventory.frontPanel,
      true,
    ); // Same board's workspace, controls still disabled.
    inventory.observe(observation(peripherals: false, board: 'board-B'));
    expect(inventory.frontPanel, false); // Never copy ACKs to another board.
    inventory.clear();
    expect(inventory.known, false);
  });

  testWidgets(
    'A new application on the same board retires old mode shells while retaining its user-owned table workspace',
    (tester) async {
      tester.view.physicalSize = const Size(1200, 1200);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final initial = DeviceStatus({
        ...peerFixture().json,
        'board_id': 'board-A',
        'boot_id': 'boot-A',
      });
      final api = LifecycleApi()
        ..healthyWs = true
        ..observed = initial;
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Table').last);
      await tester.pumpAndSettle();
      expect(
        find.widgetWithText(ChoiceChip, 'First wins'),
        findsNothing,
        reason: 'Game modes belong to the Output panel, not Table',
      );
      final findCell = find.widgetWithText(TextField, 'Find logical cell');
      await tester.enterText(findCell, '1,80');
      await tester.pumpAndSettle();
      final oldElement = tester.element(find.byType(TablePage));
      final workspace = tester
          .widget<TablePage>(find.byType(TablePage))
          .workspace!;
      workspace.pageScroll.jumpTo(
        workspace.pageScroll.position.maxScrollExtent.clamp(0, 100).toDouble(),
      );
      await tester.pumpAndSettle();
      final vertical = workspace.pageScroll.offset;
      api.updates.add(
        DeviceStatus({
          ...initial.json,
          'boot_id': 'boot-B',
          'capabilities': {
            ...initial.json['capabilities'] as Map,
            'application': {
              'id': 'cell-only',
              'name': 'Cells',
              'title': 'Cells',
            },
            'acknowledged_features': [
              'network',
              'telemetry',
              'table',
              'cell-batch',
              'leds',
            ],
            'modes': [
              {
                'id': 'cell',
                'code': 1,
                'kind': 'presentation',
                'label': 'Cells',
              },
            ],
          },
        }),
      );
      await tester.pumpAndSettle();
      expect(tester.element(find.byType(TablePage)), isNot(same(oldElement)));
      expect(
        tester.widget<TablePage>(find.byType(TablePage)).workspace,
        same(workspace),
      );
      expect(workspace.selection, {1, 80});
      expect(workspace.query.text, '1, 80');
      expect(workspace.pageScroll.offset, vertical);
      expect(find.widgetWithText(ChoiceChip, 'First wins'), findsNothing);
      expect(api.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  testWidgets(
    'ESP mode and output ACK loss keeps table control bounds and selection without authorizing writes',
    (tester) async {
      tester.view.physicalSize = const Size(1200, 1200);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final initial = observation();
      final api = LifecycleApi()
        ..healthyWs = true
        ..observed = DeviceStatus({
          ...initial.json,
          'led_output': {'enabled': true, 'mode': 'static'},
        });
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Table').last);
      await tester.pumpAndSettle();
      final findCell = find.widgetWithText(TextField, 'Find logical cell');
      await tester.enterText(findCell, '1-3');
      await tester.pumpAndSettle();
      final bounds = tester.getRect(findCell);
      final page = tester.element(find.byType(TablePage));
      final raw = Map<String, dynamic>.from(initial.json);
      raw['capabilities'] = {
        ...initial.json['capabilities'] as Map,
        'acknowledged_features': ['network', 'telemetry'],
        'modes': [],
      };
      api.updates.add(DeviceStatus(raw));
      await tester.pumpAndSettle();
      expect(tester.element(find.byType(TablePage)), same(page));
      expect(tester.getRect(findCell), bounds);
      expect(tester.widget<TextField>(findCell).controller!.text, '1-3');
      expect(
        find.text('LED output service'),
        findsNothing,
        reason: 'Output controls are not duplicated inside Table',
      );
      expect(find.widgetWithText(ChoiceChip, 'Cells'), findsNothing);
      expect(api.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  testWidgets(
    'Peripheral ACK loss keeps navigation, text draft and panel bounds; fresh ACK re-enables without remount',
    (tester) async {
      tester.view.physicalSize = const Size(1200, 1000);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = LifecycleApi()
        ..healthyWs = true
        ..observed = observation();
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Front panel').last);
      await tester.pumpAndSettle();
      final draft = find.widgetWithText(TextField, 'Marquee text');
      await tester.enterText(draft, 'KEEP MY DRAFT');
      await tester.pumpAndSettle();
      final bounds = tester.getRect(draft);
      final element = tester.element(find.byType(FrontPanelPage));
      api.updates.add(observation(peripherals: false));
      await tester.pumpAndSettle();
      expect(find.text('Front panel'), findsWidgets);
      expect(tester.element(find.byType(FrontPanelPage)), same(element));
      expect(tester.getRect(draft), bounds);
      expect(tester.widget<TextField>(draft).controller!.text, 'KEEP MY DRAFT');
      final present = tester.widget<FilledButton>(
        find.widgetWithText(FilledButton, 'Present'),
      );
      expect(present.onPressed, isNull);
      expect(
        tester
            .widget<FilledButton>(
              find.widgetWithText(FilledButton, 'Send melody'),
            )
            .onPressed,
        isNull,
      );
      api.updates.add(observation());
      await tester.pumpAndSettle();
      expect(tester.getRect(draft), bounds);
      expect(
        tester
            .widget<FilledButton>(find.widgetWithText(FilledButton, 'Present'))
            .onPressed,
        isNotNull,
      );
      expect(api.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
}
