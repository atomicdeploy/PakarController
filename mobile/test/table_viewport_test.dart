import 'package:flutter/material.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/table_viewport.dart';

import 'dart:math' as math;

void main() {
  testWidgets('Phone retains its compact explicit zoom controls', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(390, 800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final view = TableViewportController();
    await tester.pumpWidget(
      MaterialApp(
        theme: ThemeData(platform: TargetPlatform.android),
        home: Scaffold(
          body: TableViewport(controller: view, child: const SizedBox()),
        ),
      ),
    );
    expect(find.text('Fit'), findsOneWidget);
    expect(find.byTooltip('Zoom in'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
    view.dispose();
  });
  for (final width in [320.0, 1400.0]) {
    testWidgets('Fit, anchored zoom, hit testing and reset at width $width', (
      tester,
    ) async {
      tester.view.physicalSize = Size(width, 800);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final view = TableViewportController();
      var taps = 0;
      final child = SizedBox(
        width: 960,
        height: 228,
        child: Stack(
          children: [
            Positioned(
              left: 912,
              top: 170,
              width: 44,
              height: 44,
              child: TextButton(
                key: const Key('last'),
                onPressed: () => taps++,
                child: const Text('80'),
              ),
            ),
          ],
        ),
      );
      await tester.pumpWidget(
        MaterialApp(
          theme: ThemeData(platform: TargetPlatform.windows),
          home: Scaffold(
            body: TableViewport(controller: view, child: child),
          ),
        ),
      );
      await tester.pump();
      final last = find.byKey(const Key('last'));
      final fitted = tester.getRect(last);
      expect(fitted.left, greaterThanOrEqualTo(0));
      expect(fitted.right, lessThanOrEqualTo(width));
      await tester.tap(last);
      expect(taps, 1);
      await tester.sendKeyDownEvent(LogicalKeyboardKey.controlLeft);
      await tester.sendEventToBinding(
        PointerScrollEvent(
          position: fitted.center,
          scrollDelta: const Offset(0, -200),
        ),
      );
      await tester.sendKeyUpEvent(LogicalKeyboardKey.controlLeft);
      await tester.pump();
      expect(view.zoom, greaterThan(1));
      // Preserve the focal point until the no-empty-margin boundary clamps it.
      // At wide fit the table is height-limited, so anchoring the far-right
      // cell exactly would expose blank space beyond the table's right edge.
      final fit = math.min(width / 960, 304 / 228);
      final limit = math.max(0.0, (960 * fit * view.zoom - width) / 2);
      final anchor = fitted.center.dx - width / 2;
      final expectedX =
          width / 2 +
          anchor * view.zoom +
          (anchor * (1 - view.zoom)).clamp(-limit, limit);
      expect(tester.getRect(last).center.dx, closeTo(expectedX, 1));
      await tester.tap(last);
      expect(taps, 2);
      await tester.tap(find.text('Fit'));
      await tester.pump();
      expect(view.zoom, 1);
      expect(tester.getRect(last), fitted);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      view.dispose();
    });
  }
  testWidgets(
    'Android tablet hides zoom buttons and zoomed canvas owns touch and wheel pan',
    (tester) async {
      tester.view.physicalSize = const Size(1200, 900);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final view = TableViewportController()..change(3, Offset.zero);
      final page = ScrollController();
      var taps = 0;
      await tester.pumpWidget(
        MaterialApp(
          theme: ThemeData(platform: TargetPlatform.android),
          home: Scaffold(
            body: SingleChildScrollView(
              controller: page,
              child: Column(
                children: [
                  const SizedBox(height: 50),
                  TableViewport(
                    controller: view,
                    child: GestureDetector(
                      onTap: () => taps++,
                      child: const ColoredBox(color: Colors.blue),
                    ),
                  ),
                  const SizedBox(height: 1500),
                ],
              ),
            ),
          ),
        ),
      );
      await tester.pump();
      expect(find.text('Fit'), findsNothing);
      expect(find.byTooltip('Zoom in'), findsNothing);
      expect(find.byTooltip('Zoom out'), findsNothing);
      final center = tester.getCenter(
        find.byKey(const Key('table-viewport-gestures')),
      );
      await tester.dragFrom(center, const Offset(0, -100));
      await tester.pump();
      expect(view.pan.dy, lessThan(-20));
      expect(page.offset, 0);
      expect(taps, 0);
      final pan = view.pan;
      await tester.sendEventToBinding(
        PointerScrollEvent(position: center, scrollDelta: const Offset(30, 40)),
      );
      await tester.pump();
      expect(view.pan, pan - const Offset(30, 40));
      expect(page.offset, 0);
      view.fit();
      await tester.pump();
      await tester.dragFrom(center, const Offset(0, -100));
      await tester.pumpAndSettle();
      expect(page.offset, greaterThan(20));
      expect(view.pan, Offset.zero);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      view.dispose();
      page.dispose();
    },
  );
  testWidgets(
    'Continuous pinch updates within one frame retain the exact focal anchor',
    (tester) async {
      final view = TableViewportController()..change(2, Offset.zero);
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: TableViewport(
              controller: view,
              child: const ColoredBox(color: Colors.blue),
            ),
          ),
        ),
      );
      final detector = tester.widget<RawGestureDetector>(
        find.byKey(const Key('table-viewport-gestures')),
      );
      final factory = detector.gestures.values.single;
      final recognizer = factory.constructor() as ScaleGestureRecognizer;
      factory.initializer(recognizer);
      final center = tester
          .getSize(find.byKey(const Key('table-viewport-gestures')))
          .center(Offset.zero);
      recognizer.onStart!(
        ScaleStartDetails(localFocalPoint: center, pointerCount: 2),
      );
      recognizer.onUpdate!(
        ScaleUpdateDetails(
          localFocalPoint: center + const Offset(10, 10),
          scale: 1.1,
          pointerCount: 2,
        ),
      );
      recognizer.onUpdate!(
        ScaleUpdateDetails(
          localFocalPoint: center + const Offset(20, 20),
          scale: 1.2,
          pointerCount: 2,
        ),
      );
      expect(view.zoom, closeTo(2.4, .000001));
      expect(view.pan.dx, closeTo(20, .000001));
      expect(view.pan.dy, closeTo(20, .000001));
      recognizer.onUpdate!(
        ScaleUpdateDetails(
          localFocalPoint: center + const Offset(20, 20),
          scale: .4,
          pointerCount: 2,
        ),
      );
      expect(view.zoom, closeTo(.8, .000001));
      expect(
        view.pan,
        Offset.zero,
        reason: 'A table smaller than its viewport is centered, never lost',
      );
      recognizer.onEnd!(ScaleEndDetails());
      final pan = view.pan;
      recognizer.onUpdate!(
        ScaleUpdateDetails(
          localFocalPoint: center + const Offset(80, 80),
          scale: 2,
          pointerCount: 2,
        ),
      );
      expect(view.pan, pan);
      expect(view.zoom, closeTo(.8, .000001));
      recognizer.dispose();
      await tester.pumpWidget(const SizedBox());
      view.dispose();
    },
  );
  testWidgets(
    'Pinch zoom does not activate a cell and survives a telemetry rebuild',
    (tester) async {
      final view = TableViewportController();
      var taps = 0;
      Widget app() => MaterialApp(
        home: Scaffold(
          body: TableViewport(
            controller: view,
            child: GestureDetector(
              onTap: () => taps++,
              child: const ColoredBox(color: Colors.blue),
            ),
          ),
        ),
      );
      await tester.pumpWidget(app());
      final first = await tester.startGesture(
        const Offset(250, 180),
        pointer: 1,
      );
      final second = await tester.startGesture(
        const Offset(450, 180),
        pointer: 2,
      );
      await first.moveTo(const Offset(200, 180));
      await second.moveTo(const Offset(500, 180));
      await tester.pump();
      await first.moveTo(const Offset(150, 180));
      await second.moveTo(const Offset(550, 180));
      await tester.pump();
      await first.up();
      await second.up();
      expect(view.zoom, greaterThan(1));
      expect(taps, 0);
      final zoom = view.zoom;
      await tester.pumpWidget(app());
      await tester.pump();
      expect(view.zoom, zoom);
      await tester.pumpWidget(const SizedBox());
      view.dispose();
    },
  );
  testWidgets(
    'Nested page preserves holds and owns two-finger zoom with bounded snap',
    (tester) async {
      final view = TableViewportController();
      final scroll = ScrollController();
      var holds = 0, taps = 0;
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SingleChildScrollView(
              controller: scroll,
              child: Column(
                children: [
                  TableViewport(
                    controller: view,
                    child: GestureDetector(
                      onTap: () => taps++,
                      onLongPress: () => holds++,
                      child: const ColoredBox(color: Colors.blue),
                    ),
                  ),
                  const SizedBox(height: 1500),
                ],
              ),
            ),
          ),
        ),
      );
      final center = tester.getCenter(
        find.byKey(const Key('table-viewport-gestures')),
      );
      // Losing the arena to the enclosing page retires that pointer. A later
      // single-finger hold must not be mistaken for a two-finger gesture.
      await tester.dragFrom(center, const Offset(0, -60));
      await tester.pumpAndSettle();
      expect(scroll.offset, greaterThan(0));
      scroll.jumpTo(0);
      await tester.pump();
      await tester.longPressAt(center);
      expect(holds, 1);
      expect(taps, 0);
      final a = await tester.startGesture(
        center - const Offset(70, 0),
        pointer: 1,
      );
      final b = await tester.startGesture(
        center + const Offset(70, 0),
        pointer: 2,
      );
      await a.moveBy(const Offset(-90, -40));
      await b.moveBy(const Offset(90, 40));
      await tester.pump();
      await a.moveBy(const Offset(-40, -30));
      await b.moveBy(const Offset(40, 30));
      await tester.pump();
      await a.up();
      await b.up();
      await tester.pumpAndSettle();
      expect(view.zoom, greaterThan(1));
      expect(scroll.offset, 0);
      expect(taps, 0);
      await tester.dragFrom(center, const Offset(-2000, -2000));
      await tester.pumpAndSettle();
      // For the default 800x600 viewport, fit = 2/3 (height-limited).
      final viewport = tester.getSize(
        find.byKey(const Key('table-viewport-gestures')),
      );
      final fit = (viewport.width / 960).clamp(0, viewport.height / 228);
      expect(
        view.pan.dx.abs(),
        lessThanOrEqualTo(
          (960 * fit * view.zoom - viewport.width).clamp(0, double.infinity) /
                  2 +
              .01,
        ),
      );
      expect(
        view.pan.dy.abs(),
        lessThanOrEqualTo(
          (228 * fit * view.zoom - viewport.height).clamp(0, double.infinity) /
                  2 +
              .01,
        ),
      );
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      view.dispose();
      scroll.dispose();
    },
  );
}
