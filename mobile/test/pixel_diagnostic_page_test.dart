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
  final pushes = StreamController<DeviceStatus>.broadcast(sync: true);
  DeviceStatus? current;
  DateTime? observedAt;
  @override
  Duration? get statusObservationAge =>
      observedAt == null ? null : DateTime.now().difference(observedAt!);
  @override
  DeviceStatus? get freshStatus =>
      connectionWanted &&
          appForeground &&
          statusObservationAge != null &&
          statusObservationAge! < const Duration(seconds: 6)
      ? current
      : null;
  @override
  Stream<DeviceStatus> get statusUpdates => pushes.stream.map((value) {
    current = value;
    observedAt = DateTime.now();
    return value;
  });
  @override
  Future<void> close() async {
    await pushes.close();
    await super.close();
  }

  final sent = <(PixelProbe, bool)>[];
  Completer<void>? held;
  bool fail = false;
  int reads = 0;
  bool failRead = false;
  Completer<DeviceStatus>? heldRead;
  @override
  Future<DeviceStatus> status() async {
    ++reads;
    if (failRead) throw TimeoutException('fixture read timed out');
    final value = await (heldRead?.future ?? Future.value(diagnosticFixture()));
    current = value;
    observedAt = DateTime.now();
    return value;
  }

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
}) {
  api.current = observed == null ? diagnosticFixture() : observed();
  api.observedAt = DateTime.now();
  return MaterialApp(
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
}

void main() {
  testWidgets(
    'Diagnostics binds only its first fresh owner without sending a frame',
    (tester) async {
      final api = DiagnosticApi();
      addTearDown(api.close);
      await tester.pumpWidget(diagnosticPage(api, observed: () => null));
      expect(
        tester
            .widget<FilledButton>(find.byKey(const Key('pixel-show')))
            .onPressed,
        isNull,
      );
      api.pushes.add(diagnosticFixture());
      await tester.pump();
      expect(
        tester
            .widget<FilledButton>(find.byKey(const Key('pixel-show')))
            .onPressed,
        isNotNull,
      );
      api.pushes.add(
        DeviceStatus({...diagnosticFixture().json, 'boot_id': 'b' * 32}),
      );
      await tester.pump();
      expect(
        tester
            .widget<FilledButton>(find.byKey(const Key('pixel-show')))
            .onPressed,
        isNull,
      );
      expect(
        find.byKey(const Key('pixel-use-current-connection')),
        findsOneWidget,
      );
      expect(api.sent, isEmpty);
      expect(api.reads, 0);
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'Expired diagnostics can refresh, expose failure, and retry only the read',
    (tester) async {
      tester.view.physicalSize = const Size(800, 1200);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = DiagnosticApi();
      addTearDown(api.close);
      final workspace = PixelDiagnosticWorkspace()..position = '480';
      await tester.pumpWidget(diagnosticPage(api, workspace: workspace));
      api.observedAt = DateTime.now().subtract(const Duration(seconds: 6));
      await tester.pump(const Duration(seconds: 6));
      final refresh = find.byKey(const Key('pixel-refresh-state'));
      await tester.ensureVisible(refresh);
      api.failRead = true;
      await tester.tap(refresh);
      await tester.pumpAndSettle();
      expect(find.textContaining('State refresh failed:'), findsOneWidget);
      expect(
        tester
            .widget<FilledButton>(find.byKey(const Key('pixel-show')))
            .onPressed,
        isNull,
      );
      api.failRead = false;
      await tester.tap(refresh);
      await tester.pumpAndSettle();
      expect(
        tester
            .widget<FilledButton>(find.byKey(const Key('pixel-show')))
            .onPressed,
        isNotNull,
      );
      expect(
        find.text('Fresh board state received. No pixel command was sent.'),
        findsOneWidget,
      );
      expect(workspace.position, '480');
      expect(api.reads, 2);
      expect(api.sent, isEmpty);
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );
  testWidgets('Disconnect fences a pending diagnostic refresh without replay', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(800, 1200);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final api = DiagnosticApi()..heldRead = Completer<DeviceStatus>();
    addTearDown(api.close);
    var parentUpdates = 0;
    api.current = diagnosticFixture();
    api.observedAt = DateTime.now();
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: PixelDiagnosticPage(
            api: api,
            observed: diagnosticFixture,
            busyChanged: (_) {},
            onObserved: (_) => parentUpdates++,
          ),
        ),
      ),
    );
    final refresh = find.byKey(const Key('pixel-refresh-state'));
    final staleShow = tester
        .widget<FilledButton>(find.byKey(const Key('pixel-show')))
        .onPressed!;
    await tester.ensureVisible(refresh);
    await tester.tap(refresh);
    staleShow(); // A queued callback from the pre-refresh frame is fenced too.
    await tester.pump();
    expect(api.reads, 1);
    expect(tester.widget<TextButton>(refresh).onPressed, isNull);
    api.disconnect();
    api.heldRead!.complete(diagnosticFixture());
    await tester.pumpAndSettle();
    expect(parentUpdates, 0);
    expect(api.sent, isEmpty);
    expect(
      tester
          .widget<FilledButton>(find.byKey(const Key('pixel-show')))
          .onPressed,
      isNull,
    );
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets(
    'Unchanged fresh status restores diagnostics after expiry even when parent observation stays absent',
    (tester) async {
      tester.view.physicalSize = const Size(600, 1200);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = DiagnosticApi();
      addTearDown(api.close);
      DeviceStatus? parent = diagnosticFixture();
      final workspace = PixelDiagnosticWorkspace()..position = '200';
      await tester.pumpWidget(
        diagnosticPage(api, observed: () => parent, workspace: workspace),
      );
      await tester.pump();
      final show = find.byKey(const Key('pixel-show'));
      expect(tester.widget<FilledButton>(show).onPressed, isNotNull);
      parent = null;
      api.observedAt = DateTime.now().subtract(const Duration(seconds: 6));
      await tester.pump(const Duration(seconds: 6));
      expect(tester.widget<FilledButton>(show).onPressed, isNull);
      expect(
        find.textContaining('Waiting for fresh board state'),
        findsOneWidget,
      );
      api.pushes.add(diagnosticFixture());
      await tester.pump();
      expect(parent, isNull);
      expect(tester.widget<FilledButton>(show).onPressed, isNotNull);
      expect(workspace.position, '200');
      expect(api.sent, isEmpty);
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );
  testWidgets('Reopening an all-off observation keeps colour edits as drafts', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(600, 1200);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final api = DiagnosticApi();
    addTearDown(api.close);
    final workspace = PixelDiagnosticWorkspace();
    addTearDown(workspace.dispose);
    final off = DeviceStatus({
      ...diagnosticFixture().json,
      'led_output': {
        'enabled': true,
        'mode': 'effects',
        'diagnostic': {
          'revision': 3,
          'active': true,
          'index': 0,
          'count': 1,
          'rgb': 0xffffff,
          'off': true,
        },
      },
    });
    workspace.observe(off);
    await tester.pumpWidget(
      diagnosticPage(api, workspace: workspace, observed: () => off),
    );
    await tester.ensureVisible(find.text('Red'));
    await tester.tap(find.text('Red'));
    await tester.pumpAndSettle();
    expect(api.sent, isEmpty);
    expect(workspace.probe.rgb, 0xff0000);
    expect(find.text('Board reports: all 480 pixels off'), findsOneWidget);
    await tester.ensureVisible(find.byKey(const Key('pixel-show')));
    await tester.tap(find.byKey(const Key('pixel-show')));
    await tester.pumpAndSettle();
    expect(api.sent, hasLength(1));
    expect(api.sent.single.$1.rgb, 0xff0000);
    await tester.pumpWidget(const SizedBox());
  });

  test('Changed boot retires old diagnostic observation even without replacement metadata', () {
    final workspace = PixelDiagnosticWorkspace();
    final previous = DeviceStatus({
      ...diagnosticFixture().json,
      'led_output': {
        'diagnostic': {
          'revision': 8,
          'active': true,
          'index': 474,
          'count': 6,
          'rgb': 0xff0000,
          'off': false,
        },
      },
    });
    expect(workspace.observe(previous), true);
    workspace.position = '200';
    workspace.vertical = 120;
    var notifications = 0;
    workspace.addListener(() => notifications++);
    final current = DeviceStatus({
      ...diagnosticFixture().json,
      'boot_id': 'b' * 32,
    });
    expect(workspace.observe(current), true);
    expect(workspace.observation, isNull);
    expect(workspace.position, '200');
    expect(workspace.probe.first, 474);
    expect(workspace.vertical, 120);
    expect(notifications, 1);
    expect(workspace.observe(current), false);
    expect(notifications, 1);
    workspace.dispose();
  });
  testWidgets('Reconnecting can explicitly rebind without replaying a pixel', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(600, 1200);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final api = DiagnosticApi();
    addTearDown(api.close);
    DeviceStatus? peer = diagnosticFixture();
    await tester.pumpWidget(diagnosticPage(api, observed: () => peer));
    api.disconnect();
    await tester.pump();
    api.connect();
    api.pushes.add(peer);
    await tester.pumpAndSettle();
    expect(api.sent, isEmpty);
    final useCurrent = find.byKey(const Key('pixel-use-current-connection'));
    await tester.ensureVisible(useCurrent);
    await tester.pumpAndSettle();
    await tester.tap(useCurrent);
    await tester.pumpAndSettle();
    expect(api.sent, isEmpty);
    await tester.ensureVisible(find.byKey(const Key('pixel-show')));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const Key('pixel-show')));
    await tester.pumpAndSettle();
    expect(api.sent.length, 1);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets(
    'Other-client WS observations update controls without replay or polling',
    (tester) async {
      tester.view.physicalSize = const Size(600, 1200);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = DiagnosticApi();
      addTearDown(api.close);
      final workspace = PixelDiagnosticWorkspace();
      DeviceStatus pushed(int revision, {bool off = false, String? board}) =>
          DeviceStatus({
            ...diagnosticFixture().json,
            'board_id': board ?? 'fixture',
            'led_output': {
              'enabled': true,
              'mode': 'effects',
              'diagnostic': {
                'revision': revision,
                'active': true,
                'index': 474,
                'count': 6,
                'rgb': 0x1234ab,
                'off': off,
              },
            },
          });
      await tester.pumpWidget(diagnosticPage(api, workspace: workspace));
      api.pushes.add(pushed(3));
      await tester.pump();
      expect(find.text('Pixels 475–480'), findsOneWidget);
      expect(
        find.textContaining('Board reports: Pixels 475–480'),
        findsOneWidget,
      );
      expect(workspace.probe.rgb, 0x1234ab);
      expect(api.sent, isEmpty);
      api.pushes.add(pushed(2, off: true));
      api.pushes.add(pushed(4, board: 'foreign'));
      await tester.pump();
      expect(workspace.observation!.revision, 3);
      api.pushes.add(pushed(4, off: true));
      await tester.pump();
      expect(find.text('Board reports: all 480 pixels off'), findsOneWidget);
      expect(workspace.probe.first, 474);
      expect(api.sent, isEmpty);
      api.appForeground = false;
      await tester.pump();
      expect(
        tester
            .widget<FilledButton>(find.byKey(const Key('pixel-next')))
            .onPressed,
        isNull,
      );
      await tester.pumpWidget(const SizedBox());
      expect(api.pushes.hasListener, false);
    },
  );
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
      RepaintBoundary(
        key: boundary,
        child: diagnosticPage(
          api,
          observed: () => DeviceStatus({
            ...diagnosticFixture().json,
            'led_output': {
              'enabled': true,
              'mode': 'effects',
              'diagnostic': {
                'revision': 1,
                'active': true,
                'index': 474,
                'count': 6,
                'rgb': 0x1234ab,
                'off': false,
              },
            },
          }),
        ),
      ),
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
