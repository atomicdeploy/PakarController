import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/cell_pixel_dialog.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

import 'peer_fixture.dart';

void main() {
  DeviceStatus board() => DeviceStatus({
    ...peerFixture(
      features: [
        'table',
        'leds',
        'pixel-diagnostic',
        'pixel-diagnostic-overlay',
      ],
    ).json,
    'board_id': 'inspector-board',
    'boot_id': 'boot',
    'pixels': 480,
    'mode': 'cell',
    'effect': 'none',
  });
  CellMap remapped() {
    final positions = List.generate(80, (i) => i + 1);
    positions[0] = 80;
    positions[79] = 1;
    return CellMap(physical: positions, names: CellMap.defaults().names);
  }

  Future<void> show(
    WidgetTester tester, {
    required OpenPixelDiagnostics open,
    Future<DeviceStatus> Function(PixelProbe)? send,
    bool Function()? current,
  }) async {
    final observed = board();
    await tester.pumpWidget(
      MaterialApp(
        theme: studioTheme(),
        home: Scaffold(
          body: Builder(
            builder: (context) => TextButton(
              onPressed: () => showDialog<void>(
                context: context,
                builder: (_) => CellPixelDialog(
                  id: 1,
                  mapping: remapped(),
                  observed: () => observed,
                  current: current ?? () => true,
                  openDiagnostics: open,
                  sendProbe: send,
                  editMapping: () {},
                ),
              ),
              child: const Text('Inspect'),
            ),
          ),
        ),
      ),
    );
    await tester.tap(find.text('Inspect'));
    await tester.pumpAndSettle();
  }

  testWidgets(
    'Physical numbers follow the map; opening, selection and navigation never send',
    (tester) async {
      PixelProbe? opened;
      var writes = 0;
      await show(
        tester,
        open: (probe, {corrections = false, groups = false}) async {
          opened = probe;
        },
        send: (_) async {
          writes++;
          return board();
        },
      );
      expect(find.text('Cell 1 · physical pixels 475–480'), findsOneWidget);
      await tester.tap(find.byKey(const ValueKey('physical-pixel-479')));
      await tester.pumpAndSettle();
      expect(find.text('Test color · pixel 479'), findsOneWidget);
      expect(writes, 0);
      await tester.tap(find.text('Advanced diagnostics'));
      await tester.pumpAndSettle();
      expect(opened?.index, 478);
      expect(opened?.group, false);
      expect(writes, 0);
    },
  );
  testWidgets(
    'Long press exposes explicit solo pixel and group without changing until chosen',
    (tester) async {
      final sent = <PixelProbe>[];
      await show(
        tester,
        open: (_, {corrections = false, groups = false}) async {},
        send: (probe) async {
          sent.add(probe);
          return board();
        },
      );
      await tester.longPress(find.byKey(const ValueKey('physical-pixel-477')));
      await tester.pumpAndSettle();
      expect(find.text('Solo this pixel'), findsOneWidget);
      expect(find.text('Solo this 6-pixel group'), findsOneWidget);
      expect(sent, isEmpty);
      await tester.tap(find.text('Solo this pixel'));
      await tester.pumpAndSettle();
      expect(sent, hasLength(1));
      expect(sent.single.index, 476);
      expect(sent.single.overlay, false);
      expect(sent.single.group, false);
    },
  );
  testWidgets(
    'Group navigation anchors first pixel and preserves no-write behavior',
    (tester) async {
      PixelProbe? opened;
      await show(
        tester,
        open: (probe, {corrections = false, groups = false}) async {
          opened = probe;
        },
      );
      await tester.ensureVisible(find.text('Diagnose this group'));
      await tester.tap(find.text('Diagnose this group'));
      await tester.pumpAndSettle();
      expect(opened?.index, 474);
      expect(opened?.count, 6);
    },
  );
  testWidgets('Phone and keyboard constrained inspector does not overflow', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(360, 740);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    await show(
      tester,
      open: (_, {corrections = false, groups = false}) async {},
    );
    expect(tester.takeException(), isNull);
    await tester.ensureVisible(find.byType(TextField));
    await tester.enterText(find.byType(TextField), '123');
    await tester.pumpAndSettle();
    expect(find.text('Use six hexadecimal digits'), findsOneWidget);
    expect(
      tester
          .widget<FilledButton>(find.byKey(const ValueKey('pixel-apply-color')))
          .onPressed,
      isNull,
    );
    expect(tester.takeException(), isNull);
  });
}
