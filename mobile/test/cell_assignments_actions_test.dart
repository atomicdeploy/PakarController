import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/cell_assignments.dart';
import 'package:pakarcontroller/cell_assignments_actions.dart';
import 'package:pakarcontroller/documents.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

DeviceStatus tableStatus({bool lock = false, String boot = 'boot'}) {
  final json = peerFixture().json;
  json['board_id'] = 'board';
  json['boot_id'] = boot;
  json['red_lock'] = lock;
  json['capabilities']['role'] = 'table-selection';
  json['capabilities']['acknowledged_features'] = [
    ...json['capabilities']['acknowledged_features'],
    'cell-assignments',
    'cell-lock',
  ];
  return DeviceStatus(json);
}

void main() {
  testWidgets('Color selection actions do not command the board', (
    tester,
  ) async {
    String? selected;
    final observed = tableStatus();
    observed.json['cells'] = List.filled(80, 'white');
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: CellAssignmentsActions(
            status: observed,
            connected: true,
            selectColor: (value) => selected = value,
            command: (_, _) async =>
                throw StateError('Selection must not send'),
            readStatus: () async =>
                throw StateError('Selection uses current observation'),
          ),
        ),
      ),
    );
    await tester.tap(find.text('Select all red'));
    expect(selected, 'lost');
    await tester.tap(find.text('Select all white'));
    expect(selected, 'idle');
    expect(find.text('Import colors…'), findsNothing);
    expect(find.byTooltip('Import and export colors'), findsOneWidget);
  });
  TestWidgetsFlutterBinding.ensureInitialized();
  final messenger =
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
  tearDown(
    () => messenger.setMockMethodCallHandler(AndroidDocuments.channel, null),
  );

  testWidgets(
    'Lock help follows configured color names without changing state identity',
    (tester) async {
      final status = tableStatus();
      status.json['state_presentation'] = {
        'states': {
          'lost': {'rgb': 0x0000ff},
          'idle': {'rgb': 0xffff00},
        },
      };
      var colors = true;
      String? selected;
      final commands = <(String, Map<String, String>)>[];
      Widget view() => MaterialApp(
        home: Scaffold(
          body: CellAssignmentsActions(
            status: status,
            connected: true,
            colorLabels: colors,
            selectColor: (value) => selected = value,
            readStatus: () async => status,
            command: (path, values) async {
              commands.add((path, values));
              return true;
            },
          ),
        ),
      );
      await tester.pumpWidget(view());
      expect(
        find.byTooltip('Prevent blue cells returning to yellow'),
        findsOneWidget,
      );
      expect(find.text('Lock blue cells'), findsOneWidget);
      await tester.tap(find.text('Select all blue'));
      expect(selected, 'lost');
      expect(commands, isEmpty);
      await tester.tap(find.text('Lock blue cells'));
      await tester.pumpAndSettle();
      expect(commands, hasLength(1));
      expect(commands.single.$1, '/api/cells/lock');
      expect(commands.single.$2, {'enabled': 'true'});
      colors = false;
      await tester.pumpWidget(view());
      expect(
        find.byTooltip('Prevent lost cells returning to idle'),
        findsOneWidget,
      );
      expect(find.text('Lock lost cells'), findsOneWidget);
      expect(commands, hasLength(1));
    },
  );

  testWidgets('Lock sends one board command and waits for observed state', (
    tester,
  ) async {
    final status = tableStatus();
    final commands = <(String, Map<String, String>)>[];
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: CellAssignmentsActions(
            status: status,
            connected: true,
            readStatus: () async => status,
            command: (path, values) async {
              commands.add((path, values));
              return true;
            },
          ),
        ),
      ),
    );
    await tester.tap(find.text('Lock red cells'));
    await tester.pumpAndSettle();
    expect(commands, hasLength(1));
    expect(commands.single.$1, '/api/cells/lock');
    expect(commands.single.$2, {'enabled': 'true'});
    expect(tester.widget<FilterChip>(find.byType(FilterChip)).selected, false);
  });

  testWidgets(
    'Import requires preview confirmation and sends one heterogeneous batch',
    (tester) async {
      final status = tableStatus();
      final commands = <(String, Map<String, String>)>[];
      messenger.setMockMethodCallHandler(
        AndroidDocuments.channel,
        (call) async => {
          'name': 'cells.json',
          'bytes': Uint8List.fromList(
            utf8.encode(
              '{"role":"table-selection","assignments":[{"cell":1,"state":"red"},{"cell":80,"state":"off"}]}',
            ),
          ),
        },
      );
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CellAssignmentsActions(
              status: status,
              connected: true,
              readStatus: () async => status,
              command: (path, values) async {
                commands.add((path, values));
                return true;
              },
            ),
          ),
        ),
      );
      await tester.tap(find.byTooltip('Import and export colors'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Import colors…'));
      await tester.pumpAndSettle();
      expect(find.text('2 assignments · 78 cells unchanged'), findsOneWidget);
      expect(find.text('White → Red'), findsOneWidget);
      expect(commands, isEmpty);
      await tester.tap(find.text('Apply atomically'));
      await tester.pumpAndSettle();
      expect(commands.length, 1);
      expect(commands.single.$1, '/api/cells/assignments');
      expect(jsonDecode(commands.single.$2['assignments']!), [
        {'cell': 1, 'state': 'red'},
        {'cell': 80, 'state': 'off'},
      ]);
    },
  );

  testWidgets(
    'Locked import uses configured palette names; cancellation never sends',
    (tester) async {
      final status = tableStatus(lock: true);
      status.json['cells'][0] = 'red';
      status.json['state_presentation'] = {
        'states': {
          'lost': {'rgb': 0x0000ff},
          'idle': {'rgb': 0xffff00},
        },
      };
      var writes = 0;
      messenger.setMockMethodCallHandler(
        AndroidDocuments.channel,
        (call) async => {
          'name': 'cells.csv',
          'bytes': Uint8List.fromList(utf8.encode('cell,state\n1,white\n')),
        },
      );
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CellAssignmentsActions(
              status: status,
              connected: true,
              readStatus: () async => status,
              command: (path, values) async {
                writes++;
                return true;
              },
            ),
          ),
        ),
      );
      await tester.tap(find.byTooltip('Import and export colors'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Import colors…'));
      await tester.pumpAndSettle();
      expect(find.textContaining('Blue lock blocks cells 1'), findsOneWidget);
      expect(find.text('Blue → Yellow'), findsOneWidget);
      expect(
        tester
            .widget<FilledButton>(
              find.widgetWithText(FilledButton, 'Apply atomically'),
            )
            .onPressed,
        isNull,
      );
      await tester.tap(find.text('Cancel'));
      await tester.pumpAndSettle();
      expect(writes, 0);
    },
  );

  testWidgets(
    'XLSX export reads current board then writes real workbook through SAF once',
    (tester) async {
      final status = tableStatus();
      var reads = 0;
      final calls = <MethodCall>[];
      messenger.setMockMethodCallHandler(AndroidDocuments.channel, (
        call,
      ) async {
        calls.add(call);
        return call.method == 'create' ? 'destination' : null;
      });
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CellAssignmentsActions(
              status: status,
              connected: true,
              readStatus: () async {
                reads++;
                return status;
              },
              command: (_, _) async =>
                  throw StateError('Export must never command the board'),
            ),
          ),
        ),
      );
      await tester.tap(find.byTooltip('Import and export colors'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Excel workbook (.xlsx)'));
      await tester.pumpAndSettle();
      expect(reads, 1);
      expect(calls.map((call) => call.method), ['create', 'write']);
      expect(calls.first.arguments['name'], 'pakar-cells.xlsx');
      final bytes = calls.last.arguments['bytes'] as Uint8List;
      expect(CellAssignments.decode('cells.xlsx', bytes).values.length, 80);
    },
  );

  testWidgets(
    'Changing board boot while picker is open cancels without replay',
    (tester) async {
      final picked = Completer<Object?>();
      var status = tableStatus();
      var writes = 0;
      messenger.setMockMethodCallHandler(
        AndroidDocuments.channel,
        (_) => picked.future,
      );
      Widget view() => MaterialApp(
        home: Scaffold(
          body: CellAssignmentsActions(
            status: status,
            connected: true,
            readStatus: () async => status,
            command: (_, _) async {
              writes++;
              return true;
            },
          ),
        ),
      );
      await tester.pumpWidget(view());
      await tester.tap(find.byTooltip('Import and export colors'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Import colors…'));
      await tester.pump();
      status = tableStatus(boot: 'other-boot');
      await tester.pumpWidget(view());
      picked.complete({
        'name': 'cells.csv',
        'bytes': Uint8List.fromList(utf8.encode('cell,state\n1,red\n')),
      });
      await tester.pumpAndSettle();
      expect(find.textContaining('Board changed'), findsOneWidget);
      expect(find.text('Apply atomically'), findsNothing);
      expect(writes, 0);
    },
  );
  testWidgets(
    'Retained toolbar keeps bounds on lost ACKs without authorizing actions',
    (tester) async {
      var status = tableStatus();
      var connected = true;
      var writes = 0;
      Widget view() => MaterialApp(
        home: Scaffold(
          body: CellAssignmentsActions(
            status: status,
            connected: connected,
            seenFeatures: const {'cell-assignments', 'cell-lock'},
            readStatus: () async => status,
            command: (_, _) async {
              writes++;
              return true;
            },
          ),
        ),
      );
      await tester.pumpWidget(view());
      final importBounds = tester.getRect(
        find.byTooltip('Import and export colors'),
      );
      final lockBounds = tester.getRect(find.text('Lock red cells'));
      final raw = tableStatus().json;
      raw['capabilities']['acknowledged_features'] = ['network'];
      raw['capabilities']['modes'] = [];
      status = DeviceStatus(raw);
      connected = false;
      await tester.pumpWidget(view());
      await tester.pumpAndSettle();
      expect(
        tester.getRect(find.byTooltip('Import and export colors')),
        importBounds,
      );
      expect(tester.getRect(find.text('Lock red cells')), lockBounds);
      expect(
        tester
            .widgetList<OutlinedButton>(find.byType(OutlinedButton))
            .map((button) => button.onPressed),
        everyElement(isNull),
      );
      expect(
        tester.widget<FilterChip>(find.byType(FilterChip)).onSelected,
        isNull,
      );
      expect(
        tester
            .widget<PopupMenuButton<String>>(
              find.byType(PopupMenuButton<String>),
            )
            .enabled,
        false,
      );
      expect(writes, 0);
    },
  );
}
