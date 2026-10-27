import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/command_readback.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/output_panel.dart';
import 'package:pakarcontroller/table_workspace.dart';
import 'package:pakarcontroller/workspace_activity_strip.dart';

import 'peer_fixture.dart';

void main() {
  testWidgets(
    'Phone activity navigation keeps eight panels reachable without squeezing',
    (tester) async {
      tester.view.physicalSize = const Size(360, 640);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final pages = [
        for (var i = 0; i < 8; i++) ('$i', 'Panel $i', Icons.settings),
      ];
      var selected = '0';
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            bottomNavigationBar: StatefulBuilder(
              builder: (_, update) => WorkspaceActivityStrip(
                pages: pages,
                selected: selected,
                changed: (id) => update(() => selected = id),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await tester.drag(
        find.byType(SingleChildScrollView),
        const Offset(-900, 0),
      );
      await tester.pumpAndSettle();
      await tester.tap(find.text('Panel 7'));
      await tester.pumpAndSettle();
      expect(selected, '7');
      expect(tester.takeException(), isNull);
      expect(
        tester.getRect(find.text('Panel 7')).right,
        lessThanOrEqualTo(360),
      );
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'Output sends the existing board commands without optimistic service state',
    (tester) async {
      final calls = <(String, Map<String, String>)>[];
      final observed = peerFixture();
      observed.json['led_output'] = {'enabled': true, 'mode': 'static'};
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: OutputPanel(
              status: observed,
              connected: true,
              command: (path, [values = const {}]) async {
                calls.add((path, values));
              },
              openDiagnostics: () {},
            ),
          ),
        ),
      );
      await tester.ensureVisible(find.text('LED output service'));
      await tester.tap(find.byType(SwitchListTile));
      await tester.pumpAndSettle();
      expect(calls, hasLength(1));
      expect(calls.single.$1, '/api/led/output');
      expect(calls.single.$2, {'enabled': 'false'});
      expect(
        tester.widget<SwitchListTile>(find.byType(SwitchListTile)).value,
        true,
      );
      await tester.ensureVisible(find.text('Rainbow test'));
      await tester.tap(find.text('Rainbow test'));
      await tester.pumpAndSettle();
      expect(calls, hasLength(2));
      expect(calls.last.$1, '/api/effect');
      expect(calls.last.$2, {'value': 'rainbow'});
      await tester.pumpWidget(const SizedBox());
    },
  );
  test('Off readback requires actual disabled service observation', () {
    final status = peerFixture();
    for (final enabled in [null, true, false]) {
      status.json['led_output'] = {'enabled': enabled};
      expect(
        commandReadbackMatches(status, '/api/led/output', {'enabled': 'false'}),
        enabled == false,
      );
    }
  });
  testWidgets(
    'Off resends black when already disabled without optimistic or duplicate writes',
    (tester) async {
      tester.view.physicalSize = const Size(900, 1400);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final completion = Completer<void>();
      final initialObserved = peerFixture()
        ..json['led_output'] = {'enabled': false, 'mode': 'static'};
      DeviceStatus? observed = initialObserved;
      var connected = true;
      final calls = <(String, Map<String, String>)>[];
      Widget controls() => MaterialApp(
        home: Scaffold(
          body: OutputPanel(
            status: observed,
            connected: connected,
            command: (path, [values = const {}]) {
              calls.add((path, values));
              return completion.future;
            },
            openDiagnostics: () {},
          ),
        ),
      );
      await tester.pumpWidget(controls());
      await tester.pumpAndSettle();
      final off = find.byKey(const ValueKey('output-off'));
      final rainbow = find.widgetWithText(OutlinedButton, 'Rainbow test');
      void expectOutputDisabled() {
        expect(tester.widget<TextButton>(off).onPressed, isNull);
        expect(tester.widget<OutlinedButton>(rainbow).onPressed, isNull);
        expect(
          tester.widget<SwitchListTile>(find.byType(SwitchListTile)).onChanged,
          isNull,
        );
        for (final label in ['Static', 'Effects', 'State colors', 'Fire']) {
          expect(
            tester
                .widget<ChoiceChip>(find.widgetWithText(ChoiceChip, label))
                .onSelected,
            isNull,
          );
        }
      }

      await tester.ensureVisible(off);
      await tester.pumpAndSettle();
      expect(tester.widget<TextButton>(off).onPressed, isNotNull);
      expect(tester.widget<OutlinedButton>(rainbow).onPressed, isNull);
      await tester.tap(off);
      await tester.pump();
      expect(calls, hasLength(1));
      expect(calls.single.$1, '/api/led/output');
      expect(calls.single.$2, {'enabled': 'false'});
      expect(tester.widget<TextButton>(off).onPressed, isNull);
      expect(
        tester.widget<SwitchListTile>(find.byType(SwitchListTile)).value,
        false,
      );
      completion.complete();
      await tester.pumpAndSettle();
      expect(tester.widget<TextButton>(off).onPressed, isNotNull);
      expect(
        tester.widget<SwitchListTile>(find.byType(SwitchListTile)).value,
        false,
      );
      initialObserved.json['led_output'] = {'enabled': true, 'mode': 'static'};
      await tester.pumpWidget(controls());
      expect(tester.widget<OutlinedButton>(rainbow).onPressed, isNotNull);
      connected = false;
      await tester.pumpWidget(controls());
      expectOutputDisabled();
      connected = true;
      for (final status in [
        null,
        peerFixture(),
        peerFixture(features: ['identity'])
          ..json['led_output'] = {'enabled': false, 'mode': 'static'},
      ]) {
        observed = status;
        await tester.pumpWidget(controls());
        expectOutputDisabled();
      }
      expect(calls, hasLength(1));
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );
  test(
    'Output order is bounded, durable and independent of table selection',
    () {
      final workspace = TableWorkspace(
        saved: {
          'selection': [1, 21],
          'query': '1,21',
          'output_cards': ['diagnostics', 'diagnostics', 'unknown', 'scene'],
        },
      );
      expect(workspace.outputCards, ['diagnostics', 'scene', 'output']);
      workspace.reorderOutputCard(0, 2);
      final restored = TableWorkspace(saved: workspace.snapshot());
      expect(restored.outputCards, ['scene', 'output', 'diagnostics']);
      expect(restored.selection, {1, 21});
      expect(restored.query.text, '1, 21');
      workspace.moveKey(4, const Offset(0.18, 0.82));
      final positioned = TableWorkspace(saved: workspace.snapshot());
      expect(positioned.keyPosition(4), const Offset(0.18, 0.82));
      expect(positioned.keyPosition(1), const Offset(0, 0));
      positioned.resetKeyPositions();
      expect(positioned.keyPosition(4), const Offset(0, 0.5));
      positioned.dispose();
      restored.dispose();
      workspace.dispose();
    },
  );

  testWidgets(
    'Table owns selection while output owns modes and service controls',
    (tester) async {
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: TablePage(
              status: peerFixture(),
              mapping: CellMap.defaults(),
              command: (_, [values = const {}]) async {},
            ),
          ),
        ),
      );
      expect(find.text('Find logical cell'), findsOneWidget);
      expect(find.text('LED output service'), findsNothing);
      expect(find.text('Rainbow test'), findsNothing);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'Card arranging sends no commands and uses only explicit drag handles',
    (tester) async {
      var commands = 0;
      final workspace = TableWorkspace();
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: OutputPanel(
              status: peerFixture(),
              connected: true,
              workspace: workspace,
              command: (_, [values = const {}]) async {
                commands++;
              },
              openDiagnostics: () {},
            ),
          ),
        ),
      );
      expect(find.byType(ReorderableDragStartListener), findsNothing);
      await tester.tap(find.text('Arrange panels'));
      await tester.pumpAndSettle();
      expect(find.byType(ReorderableDragStartListener), findsWidgets);
      final list = tester.widget<ReorderableListView>(
        find.byType(ReorderableListView),
      );
      expect(list.buildDefaultDragHandles, false);
      list.onReorderItem!(0, 2);
      await tester.pump();
      expect(workspace.outputCards, ['output', 'diagnostics', 'scene']);
      tester
          .widget<ReorderableListView>(find.byType(ReorderableListView))
          .onReorderItem!(2, 0);
      await tester.pump();
      expect(workspace.outputCards, ['scene', 'output', 'diagnostics']);
      expect(commands, 0);
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );

  testWidgets(
    'Single-mode table has no empty Scene and hidden cards keep their saved order',
    (tester) async {
      tester.view.physicalSize = const Size(900, 1400);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final workspace = TableWorkspace();
      DeviceStatus tableStatus({bool multiple = false}) {
        final status = peerFixture();
        (status.json['capabilities'] as Map)['modes'] = [
          {'id': 'cell', 'code': 1, 'kind': 'presentation', 'label': 'Cells'},
          if (multiple)
            {'id': 'first-wins', 'code': 2, 'kind': 'game', 'label': 'First wins'},
        ];
        return status;
      }

      DeviceStatus? observed = tableStatus();
      var commands = 0;
      Widget controls() => MaterialApp(
        home: Scaffold(
          body: OutputPanel(
            status: observed,
            connected: observed != null,
            workspace: workspace,
            command: (_, [values = const {}]) async => commands++,
            openDiagnostics: () {},
          ),
        ),
      );
      final scene = find.byKey(const ValueKey('output-card-scene'));
      await tester.pumpWidget(controls());
      expect(scene, findsNothing);
      await tester.tap(find.text('Arrange panels'));
      await tester.pumpAndSettle();
      tester
          .widget<ReorderableListView>(find.byType(ReorderableListView))
          .onReorderItem!(0, 1);
      await tester.pump();
      expect(workspace.outputCards, ['scene', 'diagnostics', 'output']);
      observed = null;
      await tester.pumpWidget(controls());
      expect(scene, findsNothing);
      observed = tableStatus(multiple: true);
      await tester.pumpWidget(controls());
      expect(scene, findsOneWidget);
      expect(find.text('Cells'), findsOneWidget);
      expect(find.text('First wins'), findsOneWidget);
      observed = null;
      await tester.pumpWidget(controls());
      expect(scene, findsOneWidget);
      expect(
        tester
            .widget<ChoiceChip>(find.widgetWithText(ChoiceChip, 'Cells'))
            .onSelected,
        isNull,
      );
      observed = tableStatus();
      await tester.pumpWidget(controls());
      expect(scene, findsNothing);
      expect(workspace.outputCards, ['scene', 'diagnostics', 'output']);
      expect(commands, 0);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );

  testWidgets(
    'First-wins box arrangement is local, persistent and uses 12-pixel diagnostics',
    (tester) async {
      final base = peerFixture().json;
      final capabilities =
          Map<String, dynamic>.from(
              base['capabilities'] as Map<String, dynamic>,
            )
            ..['role'] = 'first-wins'
            ..['board_kind'] = 'game'
            ..['acknowledged_features'] = [
              'leds',
              'inputs',
              'outputs',
              'key-settings',
              'command-ack',
            ]
            ..remove('table')
            ..['modes'] = [
              {
                'id': 'first-wins',
                'code': 2,
                'kind': 'game',
                'label': 'First wins',
              },
            ];
      final status = DeviceStatus({
        ...base,
        'board_id': 'keycontroller-fixture',
        'boot_id': '0123456789abcdef0123456789abcdef',
        'mode': 'first-wins',
        'winner': 0,
        'active_inputs': 0,
        'input_9': false,
        'key_mask_applied': true,
        'key_game': {
          'enabled_mask': 15,
          'enabled_count': 4,
          'idle_rgb': 0xffffff,
          'win_rgb': 0x00ff00,
          'lose_rgb': 0xff0000,
          'extended_effects': true,
          'auto_reset_ms': 0,
        },
        'capabilities': capabilities,
      });
      var commands = 0;
      final workspace = TableWorkspace();
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: OutputPanel(
              status: status,
              connected: true,
              workspace: workspace,
              command: (_, [values = const {}]) async => commands++,
              openDiagnostics: () {},
            ),
          ),
        ),
      );
      expect(find.byKey(const ValueKey('output-card-scene')), findsOneWidget);
      expect(find.byKey(const ValueKey('key-layout-canvas')), findsOneWidget);
      expect(find.text('Arrange boxes'), findsOneWidget);
      await tester.tap(find.text('Arrange boxes'));
      await tester.pump();
      expect(
        tester
            .widget<FilterChip>(
              find.widgetWithText(FilterChip, 'Arrange boxes'),
            )
            .selected,
        true,
      );
      final before = workspace.keyPosition(1);
      tester
          .widget<GestureDetector>(
            find.byKey(const ValueKey('game-key-drag-1')),
          )
          .onPanUpdate!(
        DragUpdateDetails(
          delta: const Offset(40, 0),
          globalPosition: Offset.zero,
        ),
      );
      await tester.pump();
      expect(workspace.keyPosition(1), isNot(before));
      expect(commands, 0);
      final restored = TableWorkspace(saved: workspace.snapshot());
      expect(restored.keyPosition(1), workspace.keyPosition(1));
      restored.dispose();
      await tester.drag(
        find.byType(ReorderableListView),
        const Offset(0, -900),
      );
      await tester.pumpAndSettle();
      expect(find.text('Single pixel or 12-pixel group'), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );
}
