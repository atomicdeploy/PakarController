import 'package:flutter/material.dart';

import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/table_layout.dart';
import 'package:pakarcontroller/table_viewport.dart';
import 'package:pakarcontroller/cell_controls.dart';

import 'peer_fixture.dart';

void main() {
  testWidgets(
    'Touch selection releases Find focus without dragging the viewport back to its caret',
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
      final search = find.widgetWithText(TextField, 'Find logical cell');
      await tester.enterText(search, '1-3');
      await tester.pumpAndSettle();
      final vertical = tester
          .widget<ListView>(
            find.byKey(const PageStorageKey('table-page-scroll')),
          )
          .controller!;
      vertical.jumpTo(vertical.position.maxScrollExtent);
      await tester.pumpAndSettle();
      final y = vertical.offset;
      tester
          .widget<CellButton>(find.byKey(const ValueKey('cell-5')))
          .onPressed!();
      await tester.pumpAndSettle();
      expect(
        tester
            .widget<CellButton>(find.byKey(const ValueKey('cell-5')))
            .highlighted,
        true,
      );
      expect(
        FocusManager.instance.primaryFocus?.context?.widget,
        isNot(isA<EditableText>()),
      );
      expect(vertical.offset, y);
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'Cell commands and incoming state preserve both scroll axes and canvas position',
    (tester) async {
      tester.view.physicalSize = const Size(380, 820);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      var status = peerFixture();
      late StateSetter update;
      final reply = Completer<bool>();
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: StatefulBuilder(
              builder: (context, setState) {
                update = setState;
                return TablePage(
                  key: const ValueKey('stable-peer'),
                  status: status,
                  mapping: CellMap.defaults(),
                  command: (_, [values = const {}]) async {},
                  verifiedCommand: (_, _) => reply.future,
                );
              },
            ),
          ),
        ),
      );
      final vertical = tester
          .widget<ListView>(
            find.byKey(const PageStorageKey('table-page-scroll')),
          )
          .controller!;
      final horizontal = tester
          .widget<TableViewport>(
            find.byKey(const ValueKey('table-cell-scroll')),
          )
          .controller;
      vertical.jumpTo(vertical.position.maxScrollExtent - 20);
      horizontal.change(2, const Offset(-100, 0));
      await tester.pumpAndSettle();
      tester
          .widget<CellButton>(find.byKey(const ValueKey('cell-7')))
          .onPressed!();
      await tester.pumpAndSettle();
      final y = vertical.offset, x = horizontal.pan;
      final canvas = tester.getRect(
        find.byKey(const ValueKey('table-cell-canvas')),
      );
      tester
          .widget<CellColorAction>(find.widgetWithText(CellColorAction, 'Red'))
          .onPressed!();
      await tester.pump();
      update(
        () => status = DeviceStatus({
          ...status.json,
          'cells': List.filled(80, 'red'),
        }),
      );
      await tester.pump();
      reply.complete(true);
      await tester.pumpAndSettle();
      expect(vertical.offset, y);
      expect(horizontal.pan, x);
      expect(
        tester.getRect(find.byKey(const ValueKey('table-cell-canvas'))),
        canvas,
      );
      expect(tester.takeException(), isNull);
    },
  );
  test('Physical columns align all four cells with cell 1 at the origin', () {
    final occupied = <(double, double)>{};
    for (var row = 0; row < 4; row++) {
      for (var column = 0; column < 20; column++) {
        final id = row * 20 + column ~/ 2 + (column.isOdd ? 11 : 1);
        expect(tableCellPosition(id), (
          column: column,
          row: row,
          x: column.toDouble(),
          y: row * tableDisplayRowPitch + (column.isOdd ? .5 : 0),
        ));
        final position = tableCellPosition(id);
        expect(occupied.add((position.x, position.y)), true);
        expect(position.x, greaterThanOrEqualTo(0));
        expect(position.y, greaterThanOrEqualTo(0));
        expect(position.x + 1, lessThanOrEqualTo(tableDisplayWidth));
        expect(position.y + 1, lessThanOrEqualTo(tableDisplayHeight));
      }
    }
    expect(occupied, hasLength(80));
    expect(tableCellPosition(1).x, 0);
    expect(tableCellPosition(1).y, 0);
    for (var column = 0; column < 20; column++) {
      final top = column ~/ 2 + (column.isOdd ? 11 : 1);
      expect([
        for (var row = 0; row < 4; row++) tableCellPosition(top + row * 20).x,
      ], List.filled(4, column.toDouble()));
    }
    expect(
      [1, 21, 41, 61].map((id) => tableCellPosition(id).column),
      everyElement(0),
    );
    expect(
      [11, 31, 51, 71].map((id) => tableCellPosition(id).column),
      everyElement(1),
    );
    expect(() => tableCellPosition(0), throwsRangeError);
    expect(() => tableCellPosition(81), throwsRangeError);
  });
  testWidgets(
    'Phone table pans horizontally; 11 is right and half below 1, then 2 above-right',
    (tester) async {
      tester.view.physicalSize = const Size(380, 820);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final mapping = CellMap(
        physical: List.generate(80, (index) => 80 - index),
        names: List.generate(80, (index) => '${index + 1}'),
      );
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: TablePage(
              status: peerFixture(),
              mapping: mapping,
              command: (path, [values = const {}]) async {},
            ),
          ),
        ),
      );
      final first = find.byKey(const ValueKey('cell-1'));
      final eleven = find.byKey(const ValueKey('cell-11'));
      final second = find.byKey(const ValueKey('cell-2'));
      await tester.ensureVisible(first);
      await tester.pumpAndSettle();
      final a = tester.getRect(first),
          b = tester.getRect(eleven),
          c = tester.getRect(second);
      final pitch = b.left - a.left;
      expect(pitch, lessThan(48));
      expect(b.top - a.top, closeTo(pitch / 2, .01));
      expect(c.left - b.left, closeTo(pitch, .01));
      expect(c.top, a.top);
      expect(
        a.top,
        closeTo(
          tester.getRect(find.byKey(const ValueKey('table-cell-canvas'))).top +
              pitch / 24,
          .01,
        ),
      );
      expect(tester.widget<CellButton>(first).physical, 80);
      expect(tester.widget<CellButton>(eleven).physical, 70);
      await tester.tap(eleven);
      await tester.pumpAndSettle();
      expect(tester.widget<CellButton>(eleven).highlighted, true);
      await tester.tap(find.byTooltip('Zoom in'));
      await tester.pumpAndSettle();
      await tester.ensureVisible(eleven);
      await tester.pumpAndSettle();
      final before = tester.getRect(first).left;
      await tester.timedDragFrom(
        tester.getCenter(find.byKey(const ValueKey('cell-5'))),
        const Offset(-100, 0),
        const Duration(milliseconds: 300),
      );
      await tester.pumpAndSettle();
      expect(tester.getRect(first).left, lessThan(before));
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );
  for (final size in [const Size(380, 1100), const Size(1400, 1100)]) {
    for (final direction in TextDirection.values) {
      testWidgets(
        'Aligned columns and vertical stagger at $size in $direction',
        (tester) async {
          tester.view.physicalSize = size;
          tester.view.devicePixelRatio = 1;
          addTearDown(tester.view.resetPhysicalSize);
          addTearDown(tester.view.resetDevicePixelRatio);
          var writes = 0;
          await tester.pumpWidget(
            MaterialApp(
              home: Directionality(
                textDirection: direction,
                child: Scaffold(
                  body: TablePage(
                    status: peerFixture(),
                    mapping: CellMap.defaults(),
                    command: (_, [values = const {}]) async {
                      ++writes;
                    },
                  ),
                ),
              ),
            ),
          );
          final canvas = tester.getRect(
            find.byKey(const ValueKey('table-cell-canvas')),
          );
          Rect cell(int id) => tester.getRect(find.byKey(ValueKey('cell-$id')));
          final first = cell(1), pitch = cell(11).left - first.left;
          expect(first.top, closeTo(canvas.top + pitch / 24, .001));
          expect(first.left, closeTo(canvas.left + pitch / 24, .001));
          expect(cell(11).top - first.top, closeTo(pitch / 2, .001));
          expect(cell(2).top, first.top);
          for (var row = 0; row < 4; ++row) {
            final start = cell(1 + row * 20);
            expect(start.left - first.left, closeTo(0, .001));
            expect(
              start.top - first.top,
              closeTo(row * tableDisplayRowPitch * pitch, .001),
            );
          }
          for (var id = 1; id <= 80; ++id) {
            final bounds = cell(id);
            expect(bounds.right, lessThanOrEqualTo(canvas.right));
            expect(bounds.bottom, lessThanOrEqualTo(canvas.bottom));
            for (var other = id + 1; other <= 80; ++other) {
              expect(
                bounds.overlaps(cell(other)),
                false,
                reason: 'Cell $id must not cover cell $other or steal its taps',
              );
            }
          }
          expect(writes, 0);
          expect(tester.takeException(), isNull);
          await tester.pumpWidget(const SizedBox());
        },
      );
    }
  }
}
