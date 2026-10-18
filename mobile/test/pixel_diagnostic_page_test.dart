import 'dart:async';
import 'dart:io';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';
import 'package:pakarcontroller/pixel_diagnostic_page.dart';

import 'peer_fixture.dart';

DeviceStatus diagnosticFixture() => DeviceStatus({
  ...peerFixture(features: [...knownDiagnosticFeatures]).json,
  'board_id': 'fixture',
  'boot_id': 'a' * 32,
  'pixels': 480,
});
const knownDiagnosticFeatures = [
  'leds',
  'streaming',
  'pixel-diagnostic',
  'command-ack',
  'table',
];

class DiagnosticApi extends PakarApi {
  final sent = <(PixelProbe, bool)>[];
  Completer<void>? held;
  bool fail = false;
  @override
  Future<DeviceStatus> pixelDiagnostic(
    PixelProbe probe, {
    bool off = false,
    required bool Function() authorize,
  }) async {
    if (!authorize()) throw const StaleConnection();
    sent.add((probe, off));
    await held?.future;
    if (fail) throw const OperationUncertain();
    if (!authorize()) throw const StaleConnection();
    return diagnosticFixture();
  }
}

Widget diagnosticPage(
  DiagnosticApi api, {
  DeviceStatus? Function()? observed,
  PixelDiagnosticWorkspace? workspace,
  CellMap? mapping,
}) => MaterialApp(
  debugShowCheckedModeBanner: false,
  theme: studioTheme(),
  home: Scaffold(
    body: PixelDiagnosticPage(
      api: api,
      observed: observed ?? diagnosticFixture,
      busyChanged: (_) {},
      workspace: workspace,
      mapping: () => mapping,
    ),
  ),
);

void main() {
  testWidgets(
    'Reopening keeps the physical tail cursor without replay and uses verified map',
    (tester) async {
      tester.view.physicalSize = const Size(600, 1100);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = DiagnosticApi();
      addTearDown(api.close);
      final workspace = PixelDiagnosticWorkspace()
        ..probe = const PixelProbe(index: 479, group: true, rgb: 0xff0000);
      final map = CellMap(
        physical: List.generate(80, (i) => 80 - i),
        names: List.generate(80, (i) => '${i + 1}'),
      );
      for (var reopen = 0; reopen < 2; reopen++) {
        await tester.pumpWidget(
          diagnosticPage(api, workspace: workspace, mapping: map),
        );
        await tester.pumpAndSettle();
        expect(find.text('Pixels 475–480'), findsOneWidget);
        expect(
          find.text('Physical group 80 of 80 · logical cell 1'),
          findsOneWidget,
        );
        expect(
          tester
              .widget<FilledButton>(find.byKey(const Key('pixel-next')))
              .onPressed,
          isNull,
        );
        expect(api.sent, isEmpty);
        await tester.pumpWidget(const SizedBox());
      }
    },
  );
  testWidgets(
    'Failure keeps confirmed cursor, sector taps and off preserve position',
    (tester) async {
      tester.view.physicalSize = const Size(600, 1100);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = DiagnosticApi();
      addTearDown(api.close);
      await tester.pumpWidget(diagnosticPage(api));
      await tester.tap(find.byKey(const Key('pixel-sector-5')));
      await tester.pumpAndSettle();
      expect(find.text('Pixel 6'), findsOneWidget);
      api.fail = true;
      await tester.tap(find.byKey(const Key('pixel-next')));
      await tester.pumpAndSettle();
      expect(find.text('Pixel 6'), findsOneWidget);
      api.fail = false;
      await tester.ensureVisible(find.byKey(const Key('pixel-off')));
      await tester.tap(find.byKey(const Key('pixel-off')));
      await tester.pumpAndSettle();
      expect(api.sent.last.$2, true);
      expect(api.sent.last.$1.index, 5);
      expect(find.text('Pixel 6'), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets('Explicit simulated pixel diagnostic screenshot, no board writes', (
    tester,
  ) async {
    final path = Platform.environment['PAKAR_MOBILE_CAPTURE_PATH'];
    if (path == null) return;
    final output = File(path);
    expect(output.isAbsolute, true);
    expect(output.existsSync(), false);
    for (final font in ['Vazirmatn', 'Phosphor', 'MaterialIcons']) {
      await (FontLoader(font)..addFont(
            rootBundle.load(
              font == 'MaterialIcons'
                  ? 'fonts/MaterialIcons-Regular.otf'
                  : 'assets/fonts/${font == 'Vazirmatn' ? 'Vazirmatn-Regular.ttf' : 'Phosphor.ttf'}',
            ),
          ))
          .load();
    }
    tester.view.physicalSize = const Size(390, 1180);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final boundary = GlobalKey(), api = DiagnosticApi();
    addTearDown(api.close);
    await tester.pumpWidget(
      RepaintBoundary(key: boundary, child: diagnosticPage(api)),
    );
    await tester.pumpAndSettle();
    expect(api.sent, isEmpty);
    expect(tester.takeException(), isNull);
    final render =
        boundary.currentContext!.findRenderObject() as RenderRepaintBoundary;
    await tester.runAsync(() async {
      final image = await render.toImage(pixelRatio: 1);
      final bytes = await image.toByteData(format: ui.ImageByteFormat.png);
      await output.writeAsBytes(bytes!.buffer.asUint8List(), flush: true);
      image.dispose();
    });
    await tester.pumpWidget(const SizedBox());
  });
  for (final size in [const Size(390, 1000), const Size(1200, 1100)]) {
    testWidgets(
      'Actual diagnostic layout $size is inert until action, steps and clears',
      (tester) async {
        tester.view.physicalSize = size;
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);
        final api = DiagnosticApi();
        addTearDown(api.close);
        await tester.pumpWidget(diagnosticPage(api));
        await tester.pumpAndSettle();
        expect(api.sent, isEmpty);
        expect(tester.takeException(), isNull);
        expect(
          tester
              .widget<OutlinedButton>(find.byKey(const Key('pixel-previous')))
              .onPressed,
          isNull,
        );
        await tester.tap(find.text('6-pixel group'));
        await tester.pumpAndSettle();
        expect(api.sent, isEmpty);
        await tester.tap(find.byKey(const Key('pixel-next')));
        await tester.pumpAndSettle();
        expect(api.sent.single.$1.first, 6);
        expect(api.sent.single.$1.count, 6);
        await tester.ensureVisible(find.text('Red'));
        final beforeColour = api.sent.length;
        await tester.tap(find.text('Red'));
        await tester.pumpAndSettle();
        expect(api.sent.length, beforeColour + 1);
        await tester.ensureVisible(find.byKey(const Key('pixel-show')));
        await tester.tap(find.byKey(const Key('pixel-show')));
        await tester.pumpAndSettle();
        expect(api.sent.last.$1.rgb, 0xff0000);
        await tester.ensureVisible(find.byKey(const Key('pixel-off')));
        await tester.tap(find.byKey(const Key('pixel-off')));
        await tester.pumpAndSettle();
        expect(api.sent.last.$2, true);
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      },
    );
  }
  testWidgets(
    'Pending writes cannot repeat; disconnect revokes every control',
    (tester) async {
      final api = DiagnosticApi()..held = Completer<void>();
      addTearDown(api.close);
      DeviceStatus? peer = diagnosticFixture();
      await tester.pumpWidget(diagnosticPage(api, observed: () => peer));
      await tester.tap(find.byKey(const Key('pixel-next')));
      await tester.pump();
      expect(api.sent.length, 1);
      expect(
        tester
            .widget<FilledButton>(find.byKey(const Key('pixel-next')))
            .onPressed,
        isNull,
      );
      peer = null;
      api.disconnect();
      api.held!.complete();
      await tester.pumpAndSettle();
      expect(find.textContaining('Last verified:'), findsNothing);
      expect(
        tester
            .widget<FilledButton>(find.byKey(const Key('pixel-next')))
            .onPressed,
        isNull,
      );
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'Custom RGB dialog remains usable with phone keyboard and sends only on Show',
    (tester) async {
      tester.view.physicalSize = const Size(390, 640);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = DiagnosticApi();
      addTearDown(api.close);
      await tester.pumpWidget(diagnosticPage(api));
      await tester.ensureVisible(find.text('#FFFFFF'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('#FFFFFF'));
      await tester.pumpAndSettle();
      tester.view.viewInsets = const FakeViewPadding(bottom: 240);
      addTearDown(tester.view.resetViewInsets);
      await tester.pumpAndSettle();
      await tester.enterText(
        find.widgetWithText(TextField, 'RGB hex'),
        '12ABEF',
      );
      await tester.tap(find.text('Use colour'));
      await tester.pumpAndSettle();
      expect(api.sent, isEmpty);
      expect(tester.takeException(), isNull);
      tester.view.resetViewInsets();
      await tester.pumpAndSettle();
      await tester.ensureVisible(find.byKey(const Key('pixel-show')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('pixel-show')));
      await tester.pumpAndSettle();
      expect(api.sent.single.$1.rgb, 0x12abef);
      await tester.pumpWidget(const SizedBox());
    },
  );
}
