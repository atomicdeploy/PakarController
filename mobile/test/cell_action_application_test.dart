import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/cell_controls.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/semantic_cells.dart';
import 'package:pakarcontroller/table_workspace.dart';

import 'peer_fixture.dart';

void main() {
  test(
    'Applied action requires every selected authoritative semantic state',
    () {
      final status = <String, dynamic>{
        'cells': ['white', 'white', 'white', 'white'],
        'cell_states': ['lost', 'red', 'won', 'off'],
      };
      expect(selectionHasCellState(status, {1, 2}, 'lost'), isTrue);
      expect(selectionHasCellState(status, {1, 2}, 'red'), isTrue);
      expect(selectionHasCellState(status, {1, 2}, 'white'), isFalse);
      expect(selectionHasCellState(status, {1, 3}, 'lost'), isFalse);
      expect(selectionHasCellState(status, {3}, 'won'), isTrue);
      expect(selectionHasCellState(status, {4}, 'off'), isTrue);
      for (final ids in [
        <int>{},
        {0},
        {5},
        {1, 5},
      ]) {
        expect(selectionHasCellState(status, ids, 'lost'), isFalse);
      }
      expect(
        selectionHasCellState(
          {
            'cells': ['white', 'idle'],
          },
          {1, 2},
          'idle',
        ),
        isTrue,
      );
      expect(
        selectionHasCellState(
          {
            'cells': ['#ff0000'],
          },
          {1},
          'lost',
        ),
        isFalse,
      );
      expect(
        selectionHasCellState(
          {
            'cells': ['unknown'],
          },
          {1},
          'off',
        ),
        isFalse,
      );
      expect(selectionHasCellState(status, {1}, 'unknown'), isFalse);
    },
  );

  test(
    'Satisfied actions are translucent, legible and keep identical geometry',
    () {
      for (final brightness in Brightness.values) {
        final colors = ColorScheme.fromSeed(
          seedColor: Colors.blue,
          brightness: brightness,
        );
        for (final tone in [
          Colors.white,
          Colors.red,
          Colors.green,
          Colors.black,
        ]) {
          final filled = cellColorActionStyle(colors, tone);
          final applied = cellColorActionStyle(
            colors,
            tone,
            alreadyApplied: true,
          );
          final background = applied.backgroundColor!.resolve({})!;
          expect(background.a, lessThan(.1));
          expect(filled.backgroundColor!.resolve({})!.a, 1);
          expect(applied.minimumSize, filled.minimumSize);
          expect(applied.padding, filled.padding);
          final luminance = Color.alphaBlend(
            background,
            colors.surface,
          ).computeLuminance();
          final ink = applied.foregroundColor!.resolve({})!.computeLuminance();
          final contrast = luminance > ink
              ? (luminance + .05) / (ink + .05)
              : (ink + .05) / (luminance + .05);
          expect(contrast, greaterThanOrEqualTo(4.5));
          for (final state in <Set<WidgetState>>[
            {},
            {WidgetState.hovered},
            {WidgetState.pressed},
            {WidgetState.focused},
            {WidgetState.disabled},
          ]) {
            expect(applied.side!.resolve(state)!.width, 1);
          }
          expect(applied.side!.resolve({})!.color, Colors.transparent);
        }
      }
    },
  );

  testWidgets(
    'Live readback, not a successful send, changes the action treatment',
    (tester) async {
      tester.view.physicalSize = const Size(1280, 1000);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final workspace = TableWorkspace(
        saved: {
          'selection': [1, 2],
        },
      );
      addTearDown(workspace.dispose);
      var commands = 0;
      var observed = List.filled(80, 'idle');
      var connected = true;
      Widget page() => MaterialApp(
        theme: ThemeData.dark(useMaterial3: true),
        home: Scaffold(
          body: TablePage(
            status: DeviceStatus({
              ...peerFixture(
                features: ['table', 'cell-batch', 'leds', 'state-presentation'],
              ).json,
              'cell_states': observed,
              // The rendering palette must not decide the semantic applied state.
              'state_presentation': {
                'states': {
                  'lost': {'rgb': 0x00ff00},
                },
              },
            }),
            mapping: CellMap.defaults(),
            workspace: workspace,
            connected: connected,
            command: (_, [values = const {}]) async {},
            verifiedCommand: (_, values) async {
              commands++;
              expect(values['state'], 'lost');
              return true;
            },
          ),
        ),
      );
      CellColorAction action(String value) => tester.widget<CellColorAction>(
        find.byWidgetPredicate(
          (widget) => widget is CellColorAction && widget.value == value,
        ),
      );
      await tester.pumpWidget(page());
      await tester.pumpAndSettle();
      expect(action('idle').alreadyApplied, isTrue);
      expect(action('lost').alreadyApplied, isFalse);
      final oldSize = tester.getSize(
        find.byWidgetPredicate(
          (widget) => widget is CellColorAction && widget.value == 'lost',
        ),
      );
      await tester.tap(
        find.byWidgetPredicate(
          (widget) => widget is CellColorAction && widget.value == 'lost',
        ),
      );
      await tester.pumpAndSettle();
      expect(commands, 1);
      expect(
        action('lost').alreadyApplied,
        isFalse,
        reason: 'No optimistic state from command success.',
      );
      observed = List.filled(80, 'lost');
      await tester.pumpWidget(page());
      await tester.pumpAndSettle();
      expect(action('lost').alreadyApplied, isTrue);
      expect(action('idle').alreadyApplied, isFalse);
      expect(
        tester.getSize(
          find.byWidgetPredicate(
            (widget) => widget is CellColorAction && widget.value == 'lost',
          ),
        ),
        oldSize,
      );
      expect(workspace.selection, {1, 2});
      // The tonal action remains intentional/replayable, not silently disabled.
      expect(action('lost').onPressed, isNotNull);
      observed = List.filled(80, 'lost')..[1] = 'idle';
      await tester.pumpWidget(page());
      await tester.pumpAndSettle();
      expect(action('lost').alreadyApplied, isFalse);
      expect(action('idle').alreadyApplied, isFalse);
      connected = false;
      observed = List.filled(80, 'lost');
      await tester.pumpWidget(page());
      expect(action('lost').alreadyApplied, isFalse);
      expect(action('lost').onPressed, isNull);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );
}
