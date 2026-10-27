import 'dart:io';
import 'dart:ui' as ui;

import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/key_box_designer.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/minigame_panel.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/table_workspace.dart';

import 'minigame_test.dart' show miniStatus;

void main() {
  testWidgets(
    'Arrange and group drag preserve selection and never trigger IO',
    (tester) async {
      tester.view.physicalSize = const Size(1000, 900);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final workspace = TableWorkspace();
      var commands = 0;
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SingleChildScrollView(
              child: KeyBoxDesigner(
                status: miniStatus(),
                workspace: workspace,
                enabled: true,
                onTap: (_) => commands++,
                openSettings: () {},
              ),
            ),
          ),
        ),
      );
      expect(find.text('Button'), findsNWidgets(9));
      expect(find.text('Indicator'), findsNWidgets(9));
      await tester.tap(find.byKey(const ValueKey('key-designer-arrange')));
      await tester.pumpAndSettle();
      workspace.selectKeys({1, 2});
      await tester.pump();
      final a = workspace.keyPosition(1), b = workspace.keyPosition(2);
      await tester.drag(
        find.byKey(const ValueKey('game-key-drag-1')),
        const Offset(90, 42),
      );
      await tester.pumpAndSettle();
      final movedA = workspace.keyPosition(1),
          movedB = workspace.keyPosition(2);
      expect(movedA, isNot(a));
      expect((movedB - movedA).dx, closeTo((b - a).dx, .0001));
      expect((movedB - movedA).dy, closeTo((b - a).dy, .0001));
      expect(commands, 0);
      final restored = TableWorkspace(saved: workspace.snapshot());
      expect(restored.selectedKeys, {1, 2});
      expect(restored.keyPosition(1), movedA);
      await tester.drag(
        find.byKey(const ValueKey('game-key-drag-1')),
        const Offset(-3000, -3000),
      );
      await tester.pumpAndSettle();
      expect(workspace.keyPosition(1).dx, greaterThanOrEqualTo(0));
      expect(workspace.keyPosition(1).dy, greaterThanOrEqualTo(0));
      expect(commands, 0);
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
      restored.dispose();
    },
  );

  testWidgets(
    'Hover preserves card geometry and explicit disabled actions stay disabled',
    (tester) async {
      tester.view.physicalSize = const Size(1000, 900);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final workspace = TableWorkspace();
      var commands = 0;
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: KeyBoxDesigner(
              status: miniStatus(),
              workspace: workspace,
              enabled: true,
              onTap: (_) => commands++,
              openSettings: () {},
            ),
          ),
        ),
      );
      final finder = find.byKey(const ValueKey('game-key-1'));
      final before = tester.getRect(finder);
      final mouse = await tester.createGesture(kind: PointerDeviceKind.mouse);
      await mouse.addPointer(location: const Offset(2, 2));
      await mouse.moveTo(before.center);
      await tester.pumpAndSettle();
      expect(tester.getRect(finder), before);
      await tester.tap(find.byKey(const ValueKey('game-key-drag-3')));
      expect(commands, 0);
      await tester.tap(find.byTooltip('Box 3 actions'));
      await tester.pumpAndSettle();
      expect(
        tester
            .widget<ListTile>(find.widgetWithText(ListTile, 'Trigger box'))
            .enabled,
        false,
      );
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );

  testWidgets(
    'Phone large text and tablet game workspace fit without overflows',
    (tester) async {
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final workspace = TableWorkspace();
      for (final width in [380.0, 1200.0]) {
        tester.view.physicalSize = Size(width, 1000);
        await tester.pumpWidget(
          MaterialApp(
            home: MediaQuery(
              data: MediaQueryData(
                size: Size(width, 1000),
                textScaler: TextScaler.linear(width < 500 ? 1.3 : 1),
              ),
              child: Scaffold(
                body: MiniGamePanel(
                  status: miniStatus(),
                  connected: true,
                  command: (_, _) async => true,
                  openBoxSettings: () {},
                  workspace: workspace,
                ),
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        await tester.ensureVisible(find.byKey(const ValueKey('game-key-9')));
        await tester.pumpAndSettle();
        expect(tester.takeException(), isNull);
      }
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );

  testWidgets(
    'Capture the actual game workspace with published dual-group RGB',
    (tester) async {
      final path = Platform.environment['PAKAR_MOBILE_CAPTURE_PATH'];
      if (path == null) return;
      for (final font in ['Vazirmatn', 'MaterialIcons']) {
        await (FontLoader(font)..addFont(
              rootBundle.load(
                font == 'MaterialIcons'
                    ? 'fonts/MaterialIcons-Regular.otf'
                    : 'assets/fonts/Vazirmatn-Regular.ttf',
              ),
            ))
            .load();
      }
      tester.view.physicalSize = const Size(1200, 1060);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final base = miniStatus().json;
      final status = DeviceStatus({
        ...base,
        'winner': 2,
        'key_game': {...base['key_game'] as Map, 'enabled_mask': 63},
        'minigame': {
          ...base['minigame'] as Map,
          'phase': 'success',
          'winner': 2,
        },
        'pixel_frame': {
          'revision': 1,
          'pending': false,
          'physical': List.generate(18, (i) => i + 1),
          'rgb': List.generate(
            18,
            (g) => List.generate(
              12,
              (p) => g == 2 || g == 3
                  ? (p == 2 ? '00b85a' : '35ef91')
                  : g < 12
                  ? (p == 3 ? 'a81135' : 'ff4264')
                  : '000000',
            ).join(),
          ).join(),
        },
      });
      final workspace = TableWorkspace(), boundary = GlobalKey();
      await tester.pumpWidget(
        MaterialApp(
          theme: studioTheme(palette: const {'primary': 0x38d27a}),
          home: RepaintBoundary(
            key: boundary,
            child: Scaffold(
              body: MiniGamePanel(
                status: status,
                connected: true,
                command: (_, _) async => true,
                openBoxSettings: () {},
                workspace: workspace,
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(tester.takeException(), isNull);
      await tester.runAsync(() async {
        final render =
            boundary.currentContext!.findRenderObject()
                as RenderRepaintBoundary;
        final image = await render.toImage(pixelRatio: 1);
        final png = await image.toByteData(format: ui.ImageByteFormat.png);
        await File(path).writeAsBytes(png!.buffer.asUint8List(), flush: true);
        image.dispose();
      });
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );
}
