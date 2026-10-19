import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
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
      expect(restored.query.text, '1,21');
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
      list.onReorderItem!(2, 0);
      await tester.pump();
      expect(workspace.outputCards, ['scene', 'output', 'diagnostics']);
      expect(commands, 0);
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );
}
