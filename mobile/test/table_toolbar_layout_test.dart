import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/cell_controls.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/table_workspace.dart';

import 'peer_fixture.dart';

/// Local widget evidence only: no board, socket, or actuator is used here.
void main() {
  for (final width in [390.0, 800.0, 1280.0]) {
    for (final scale in [1.0, 1.4]) {
      testWidgets('Table toolbar fits ${width}px at text scale $scale', (
        tester,
      ) async {
        tester.view.physicalSize = Size(width, 1100);
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);
        final workspace = TableWorkspace();
        addTearDown(workspace.dispose);
        var commands = 0;
        await tester.pumpWidget(
          MaterialApp(
            theme: ThemeData.dark(useMaterial3: true),
            home: MediaQuery(
              data: MediaQueryData(textScaler: TextScaler.linear(scale)),
              child: Scaffold(
                body: TablePage(
                  status: peerFixture(
                    features: const [
                      'table',
                      'cell-batch',
                      'leds',
                      'state-presentation',
                    ],
                  ),
                  mapping: CellMap.defaults(),
                  workspace: workspace,
                  command: (_, [values = const {}]) async {
                    commands++;
                  },
                  verifiedCommand: (_, _) async {
                    commands++;
                    return true;
                  },
                ),
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        expect(tester.takeException(), isNull);

        final count = find.byKey(const ValueKey('table-selection-count'));
        final options = find.byKey(const ValueKey('table-display-options'));
        final effect = find.byKey(const ValueKey('table-transition'));
        final colorActions = find.byKey(const ValueKey('table-colour-actions'));
        expect(count, findsOneWidget);
        expect(find.text('0 selected'), findsOneWidget);
        expect(options, findsOneWidget);
        expect(tester.getRect(options).right, closeTo(width - 16, .1));
        expect(
          tester.getRect(count).center.dy,
          closeTo(tester.getRect(options).center.dy, .1),
        );
        if (width >= 800 && scale == 1) {
          expect(
            tester.getRect(find.text('Table')).center.dy,
            closeTo(tester.getRect(options).center.dy, .1),
          );
        }
        expect(find.text('LED indicators'), findsNothing);
        expect(find.byKey(const ValueKey('cell-label-mode')), findsNothing);
        expect(
          find.descendant(of: colorActions, matching: effect),
          findsOneWidget,
        );
        expect(tester.getSize(effect).width, lessThanOrEqualTo(200));
        expect(
          tester.getRect(count).bottom,
          lessThan(tester.getRect(colorActions).top),
        );
        for (final target in [count, options, effect, colorActions]) {
          final rect = tester.getRect(target);
          expect(rect.left, greaterThanOrEqualTo(0));
          expect(rect.right, lessThanOrEqualTo(width));
        }
        if (width >= 800 && scale == 1) {
          final first = tester.getRect(find.byType(CellColorAction).first);
          final transition = tester.getRect(effect);
          expect(
            first.top < transition.bottom && transition.top < first.bottom,
            isTrue,
            reason: 'Tablet transition control shares the color action row.',
          );
        }

        // Display preferences are local and cannot accidentally send LED data.
        await tester.tap(options);
        await tester.pumpAndSettle();
        expect(
          find.widgetWithText(AlertDialog, 'Table display'),
          findsOneWidget,
        );
        final indicator = find.byKey(const ValueKey('table-led-indicators'));
        expect(tester.widget<SwitchListTile>(indicator).value, isTrue);
        await tester.tap(indicator);
        await tester.pumpAndSettle();
        expect(workspace.showIndicators, isFalse);
        expect(tester.widget<SwitchListTile>(indicator).value, isFalse);
        expect(find.byKey(const ValueKey('cell-label-mode')), findsOneWidget);
        expect(commands, 0);
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      });
    }
  }

  testWidgets(
    'Selection count follows none, some, all and survives reopening',
    (tester) async {
      tester.view.physicalSize = const Size(800, 1100);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final workspace = TableWorkspace();
      addTearDown(workspace.dispose);
      var commands = 0;
      Widget page(TableWorkspace state) => MaterialApp(
        home: Scaffold(
          body: TablePage(
            status: peerFixture(),
            mapping: CellMap.defaults(),
            workspace: state,
            command: (_, [values = const {}]) async {
              commands++;
            },
          ),
        ),
      );
      await tester.pumpWidget(page(workspace));
      expect(find.text('0 selected'), findsOneWidget);
      await tester.enterText(
        find.widgetWithText(TextField, 'Find logical cell'),
        '1,2,3',
      );
      await tester.pumpAndSettle();
      expect(find.text('3 selected'), findsOneWidget);
      expect(workspace.selection, {1, 2, 3});

      await tester.tap(find.widgetWithText(TextButton, 'Select all'));
      await tester.pumpAndSettle();
      expect(find.text('80 selected'), findsOneWidget);
      final restored = TableWorkspace(saved: workspace.snapshot());
      addTearDown(restored.dispose);
      await tester.pumpWidget(const SizedBox());
      await tester.pumpWidget(page(restored));
      await tester.pumpAndSettle();
      expect(find.text('80 selected'), findsOneWidget);

      await tester.tap(find.widgetWithText(TextButton, 'Clear selection'));
      await tester.pumpAndSettle();
      expect(find.text('0 selected'), findsOneWidget);
      expect(restored.selection, isEmpty);
      expect(restored.snapshot()['selection'], isEmpty);
      expect(commands, 0);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );

  test(
    'Color names migrate once and later explicit state labels are retained',
    () {
      final legacy = TableWorkspace(
        saved: {
          'cell_labels': 'states',
          'selection': [1, 21, 80],
          'indicators': false,
        },
      );
      addTearDown(legacy.dispose);
      expect(legacy.colorLabels, isTrue);
      expect(legacy.selection, {1, 21, 80});
      expect(legacy.showIndicators, isFalse);
      expect(legacy.snapshot()['cell_label_preferences'], 1);
      legacy.colorLabels = false;
      final restored = TableWorkspace(saved: legacy.snapshot());
      addTearDown(restored.dispose);
      expect(restored.colorLabels, isFalse);
      expect(restored.selection, {1, 21, 80});
      expect(restored.showIndicators, isFalse);
    },
  );
}
