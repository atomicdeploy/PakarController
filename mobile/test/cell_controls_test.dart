import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/cell_controls.dart';
import 'package:pakarcontroller/main.dart'
    show bundledApplicationArtwork, TablePage;
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/table_workspace.dart';

import 'peer_fixture.dart';

void main() {
  testWidgets(
    'Table restores selected count and switches labels without sending commands',
    (tester) async {
      final workspace = TableWorkspace(
        saved: {
          'selection': [1, 2, 3],
          'query': '80',
        },
      );
      var writes = 0;
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: TablePage(
              status: peerFixture(),
              mapping: CellMap.defaults(),
              workspace: workspace,
              command: (path, [values = const {}]) async {
                writes++;
              },
            ),
          ),
        ),
      );
      expect(find.text('3 selected'), findsOneWidget);
      expect(workspace.selection, {1, 2, 3});
      expect(find.text('Red'), findsOneWidget);
      await tester.tap(find.byKey(const ValueKey('table-display-options')));
      await tester.pumpAndSettle();
      final dropdown = tester.widget<DropdownButtonFormField<bool>>(
        find.byKey(const ValueKey('cell-label-mode')),
      );
      dropdown.onChanged!(false);
      await tester.pump();
      await tester.tap(find.text('Done'));
      await tester.pumpAndSettle();
      expect(find.text('Lost'), findsOneWidget);
      expect(workspace.snapshot()['cell_labels'], 'states');
      expect(writes, 0);
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );
  test('All, none and partial selection survive with canonical search and label preference', () {
    for (final ids in [
      <int>[],
      [1, 7, 80],
      List.generate(80, (i) => i + 1),
    ]) {
      final workspace = TableWorkspace(
        saved: {
          'selection': ids,
          'query': '4',
          'cell_labels': 'states',
          'cell_label_preferences': 1,
        },
      );
      expect(workspace.selection, ids.toSet());
      expect(
        workspace.query.text,
        ids.isEmpty
            ? ''
            : ids.length == 80
            ? '1-80'
            : '1, 7, 80',
      );
      expect(workspace.colorLabels, false);
      final restored = TableWorkspace(saved: workspace.snapshot());
      expect(restored.selection, ids.toSet());
      expect(restored.colorLabels, false);
      workspace.dispose();
      restored.dispose();
    }
    final defaults = TableWorkspace();
    expect(defaults.colorLabels, true);
    defaults.dispose();
  });
  test(
    'Color labels reflect configured palette without changing state identity',
    () {
      final color = cellActionColor('lost', {
        'state_presentation': {
          'states': {
            'lost': {'rgb': 0x0000ff},
          },
        },
      });
      expect(cellActionLabel('lost', color, colors: true), 'Blue');
      expect(cellActionLabel('lost', color, colors: false), 'Lost');
    },
  );
  testWidgets('Selected options keep readable ink on muted dark fill', (
    tester,
  ) async {
    final theme = ThemeData.dark();
    await tester.pumpWidget(
      MaterialApp(
        theme: theme,
        home: Scaffold(
          body: CellOption(
            label: 'LED indicators',
            icon: Icons.lightbulb_outline,
            selected: true,
            onSelected: (_) {},
          ),
        ),
      ),
    );
    final chip = tester.widget<FilterChip>(find.byType(FilterChip));
    expect(chip.labelStyle!.color, theme.colorScheme.primary);
    expect(chip.checkmarkColor, theme.colorScheme.primary);
  });
  testWidgets(
    'Color commands have icons, accessible height and disabled guards',
    (tester) async {
      var sent = 0;
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Wrap(
              children: [
                CellColorAction(
                  value: 'red',
                  label: 'Red',
                  onPressed: () => sent++,
                ),
                const CellColorAction(value: 'off'),
              ],
            ),
          ),
        ),
      );
      expect(find.byType(FilledButton), findsNWidgets(2));
      expect(find.byType(Icon), findsNWidgets(2));
      expect(
        tester.getSize(find.byType(FilledButton).first).height,
        greaterThanOrEqualTo(48),
      );
      expect(
        tester.getSize(find.byType(FilledButton).first).width,
        lessThan(112),
      );
      final button = tester.widget<FilledButton>(
        find.byType(FilledButton).first,
      );
      expect(button.style!.minimumSize!.resolve({})!.height, 36);
      expect(
        button.style!.backgroundColor!.resolve({}),
        Color.lerp(
          const Color(0xffd9364a),
          ThemeData().colorScheme.surfaceContainerHighest,
          .12,
        ),
      );
      expect(button.style!.side!.resolve({})!.color, Colors.transparent);
      await tester.tap(find.text('Red'));
      await tester.tap(find.text('Set off'));
      expect(sent, 1);
    },
  );
  testWidgets('Identical telemetry artwork keeps its image provider bytes', (
    tester,
  ) async {
    Widget frame(Uint8List data) => MaterialApp(
      home: StableApplicationIcon(
        bytes: data,
        fallback: bundledApplicationArtwork,
      ),
    );
    await tester.pumpWidget(
      frame(Uint8List.fromList(bundledApplicationArtwork)),
    );
    final first = tester.widget<Image>(find.byType(Image)).image as MemoryImage;
    await tester.pumpWidget(
      frame(Uint8List.fromList(bundledApplicationArtwork)),
    );
    final next = tester.widget<Image>(find.byType(Image)).image as MemoryImage;
    expect(identical(first.bytes, next.bytes), isTrue);
    expect(tester.widget<Image>(find.byType(Image)).gaplessPlayback, isTrue);
  });
}
