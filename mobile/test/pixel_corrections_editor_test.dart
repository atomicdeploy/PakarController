import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_correction_preview.dart';
import 'package:pakarcontroller/pixel_corrections_editor.dart';
import 'package:pakarcontroller/pixel_corrections_page.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

import 'peer_fixture.dart';

Map<String, dynamic> rule(String kind, int pixel, [int offset = 0]) => {
  'kind': kind,
  'pixel': pixel,
  'offset': offset,
};

class EditorApi extends PakarApi {
  EditorApi({this.previewSupported = true, this.toggleSupported = true});
  final bool previewSupported, toggleSupported;
  String boot = 'a' * 32;
  final requests = <Map<String, dynamic>>[];
  int saves = 0, nextLease = 40;
  Completer<void>? hold;
  bool wrongReadback = false;
  String? frameRgb;
  Map<String, dynamic>? activePreview;
  Map<String, dynamic> value = {
    'enabled': true,
    'revision': 3,
    'pixels': 480,
    'rules': <dynamic>[],
    'max_rules': 32,
  };
  @override
  DeviceStatus get freshStatus => DeviceStatus({
    ...peerFixture(
      features: [
        'leds',
        'table',
        'pixel-corrections',
        'pixel-diagnostic',
        'command-ack',
        if (previewSupported) 'pixel-corrections-preview',
        if (toggleSupported) 'pixel-correction-rule-toggle',
      ],
    ).json,
    'board_id': 'correction-fixture',
    'boot_id': boot,
    if (frameRgb != null) 'pixel_frame': {'rgb': frameRgb, 'pending': false},
  });
  List<int> sources(Map<String, dynamic> config) => pixelCorrectionSources(
    480,
    config['enabled'] == true,
    (config['rules'] as List)
        .map((r) => Map<String, dynamic>.from(r as Map))
        .toList(),
  );
  @override
  Future<CellMap> mapping() async => CellMap.defaults();
  @override
  Future<Map<String, dynamic>> getJson(String path) async => {
    ...jsonDecode(jsonEncode(value)) as Map<String, dynamic>,
    'sources': sources(value),
    'active_sources': wrongReadback
        ? List.filled(480, -1)
        : sources(activePreview ?? value),
    'preview': {
      'active': activePreview != null,
      'lease_id': activePreview?['lease_id'] ?? 0,
      'base_revision': value['revision'],
      'remaining_ms': activePreview == null ? 0 : 10000,
    },
  };
  @override
  Future<Map<String, dynamic>> postJson(
    String path,
    Map<String, String> values, {
    bool Function()? authorize,
  }) async {
    if (authorize?.call() != true) throw const StaleConnection();
    final config = jsonDecode(values['configuration']!) as Map<String, dynamic>;
    requests.add({'path': path, ...config});
    if (path.endsWith('/preview')) {
      if (config['clear'] == true) {
        if (activePreview?['lease_id'] == config['lease_id']) {
          activePreview = null;
        }
      } else {
        if (activePreview != null &&
            activePreview!['lease_id'] != config['lease_id']) {
          throw StateError('owned');
        }
        activePreview = {
          ...config,
          'lease_id': config['lease_id'] ?? ++nextLease,
        };
      }
    } else {
      saves++;
      value = {...value, ...config, 'revision': (value['revision'] as int) + 1};
      activePreview = null;
    }
    // Model a command applied on the fixture before its response is delayed.
    final reply = await getJson('/api/pixels/corrections');
    await hold?.future;
    return reply;
  }
}

Widget page(
  EditorApi api, {
  PixelDiagnosticWorkspace? workspace,
  bool active = true,
  bool livePreview = false,
}) => MaterialApp(
  debugShowCheckedModeBanner: false,
  theme: studioTheme(),
  home: Scaffold(
    body: PixelCorrectionsPage(
      api: api,
      busyChanged: (_) {},
      workspace: workspace,
      active: active,
      livePreviewInitiallyEnabled: livePreview,
    ),
  ),
);

void tabletViewport(WidgetTester tester) {
  tester.view.physicalSize = const Size(1100, 1600);
  tester.view.devicePixelRatio = 1;
  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);
}

void main() {
  test(
    'mapping projection matches ordered BoardCore black/remove/shift semantics',
    () {
      expect(pixelCorrectionSources(6, true, [rule('black', 2)]), [
        0,
        1,
        -1,
        3,
        4,
        5,
      ]);
      expect(pixelCorrectionSources(6, true, [rule('remove', 2)]), [
        0,
        1,
        3,
        4,
        5,
        -1,
      ]);
      expect(pixelCorrectionSources(6, true, [rule('shift', 2, 2)]), [
        0,
        1,
        -1,
        -1,
        2,
        3,
      ]);
      expect(pixelCorrectionSources(6, true, [rule('shift', 2, -1)]), [
        0,
        2,
        3,
        4,
        5,
        -1,
      ]);
      expect(
        pixelCorrectionSources(6, true, [rule('black', 2), rule('remove', 1)]),
        [0, -1, 3, 4, 5, -1],
      );
      expect(
        pixelCorrectionSources(6, true, [
          {...rule('remove', 1), 'enabled': false},
          rule('black', 2),
        ]),
        [0, 1, -1, 3, 4, 5],
      );
      expect(pixelCorrectionSources(6, false, [rule('remove', 1)]), [
        0,
        1,
        2,
        3,
        4,
        5,
      ]);
      expect(
        () => pixelCorrectionSources(6, true, [rule('shift', 6)]),
        throwsFormatException,
      );
      expect(
        () => pixelCorrectionSources(6, true, [
          {...rule('black', 0), 'enabled': 'false'},
        ]),
        throwsFormatException,
      );
    },
  );
  testWidgets(
    'both reorder controls retain rule values and save only on explicit action',
    (tester) async {
      tabletViewport(tester);
      final api = EditorApi()
        ..value['rules'] = [rule('black', 3), rule('shift', 6, 1)];
      addTearDown(api.close);
      await tester.pumpWidget(page(api));
      await tester.pumpAndSettle();
      expect(find.byTooltip('Move up'), findsNWidgets(2));
      expect(find.byTooltip('Move down'), findsNWidgets(2));
      expect(
        tester
            .widget<IconButton>(
              find.ancestor(
                of: find.byTooltip('Move up').first,
                matching: find.byType(IconButton),
              ),
            )
            .onPressed,
        isNull,
      );
      expect(
        tester
            .widget<IconButton>(
              find.ancestor(
                of: find.byTooltip('Move down').last,
                matching: find.byType(IconButton),
              ),
            )
            .onPressed,
        isNull,
      );
      await tester.ensureVisible(find.byTooltip('Move down').first);
      await tester.tap(find.byTooltip('Move down').first);
      await tester.pumpAndSettle();
      expect(api.requests, isEmpty);
      await tester.ensureVisible(find.text('Save to board'));
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(api.saves, 1);
      expect(api.value['rules'], [rule('shift', 6, 1), rule('black', 3)]);
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'one selected cursor action imports locally and invalid numbers cannot save',
    (tester) async {
      tabletViewport(tester);
      final api = EditorApi();
      addTearDown(api.close);
      final workspace = PixelDiagnosticWorkspace(saved: {'position': '24'});
      workspace.observation = PixelDiagnosticObservation(
        'correction-fixture',
        api.boot,
        2,
        true,
        const PixelProbe(index: 30, rgb: 0xff0000),
        false,
      );
      await tester.pumpWidget(page(api, workspace: workspace));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Use selected cursor (24)'));
      await tester.pumpAndSettle();
      expect(api.requests, isEmpty);
      expect(find.text('From diagnostics'), findsNothing);
      expect(find.textContaining('Use lit'), findsNothing);
      workspace.position = '31';
      await tester.pumpAndSettle();
      await tester.tap(find.text('Use selected cursor (31)'));
      await tester.pumpAndSettle();
      await tester.ensureVisible(find.byType(TextFormField).first);
      expect(
        tester
            .widget<TextFormField>(find.byType(TextFormField).first)
            .controller!
            .text,
        '31',
      );
      await tester.enterText(find.byType(TextFormField).first, '481');
      await tester.ensureVisible(find.text('Save to board'));
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(api.saves, 0);
      expect(find.text('1–480'), findsOneWidget);
      // Import also replaces an invalid draft even when the prior valid value matches.
      await tester.tap(find.text('Use selected cursor (31)'));
      await tester.pumpAndSettle();
      expect(
        tester
            .widget<TextFormField>(find.byType(TextFormField).first)
            .controller!
            .text,
        '31',
      );
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'disabled row preserves values and old firmware cannot silently save it',
    (tester) async {
      tabletViewport(tester);
      final api = EditorApi(toggleSupported: false)
        ..value['rules'] = [rule('shift', 5, -2)];
      addTearDown(api.close);
      await tester.pumpWidget(page(api));
      await tester.pumpAndSettle();
      await tester.ensureVisible(find.byType(Checkbox));
      await tester.tap(find.byType(Checkbox));
      await tester.pumpAndSettle();
      await tester.ensureVisible(find.text('Save to board'));
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(api.saves, 0);
      expect(
        find.textContaining('before disabled rules can be saved'),
        findsOneWidget,
      );
      await tester.ensureVisible(find.byType(Checkbox));
      await tester.tap(find.byType(Checkbox));
      await tester.pumpAndSettle();
      await tester.ensureVisible(find.text('Save to board'));
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect((api.value['rules'] as List).single, {
        ...rule('shift', 5, -2),
        'enabled': true,
      });
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'preview is opt-in, coalesces edits, and relinquishes late acquisition after cancellation',
    (tester) async {
      final api = EditorApi();
      addTearDown(api.close);
      final config = {
        'revision': 3,
        'enabled': true,
        'rules': [rule('black', 2)],
      };
      final preview = PixelCorrectionPreview(
        api: api,
        allowed: () => true,
        configuration: () => config,
        pixels: () => 480,
      );
      expect(api.requests, isEmpty);
      await preview.start();
      expect(preview.enabled, true);
      expect(preview.lease, 41);
      api.hold = Completer<void>();
      (config['rules'] as List)[0] = rule('black', 3);
      preview.edited();
      await tester.pump(const Duration(milliseconds: 300));
      (config['rules'] as List)[0] = rule('black', 4);
      preview.edited();
      (config['rules'] as List)[0] = rule('black', 5);
      preview.edited();
      await tester.pump(const Duration(milliseconds: 300));
      expect(api.requests.length, 2);
      final held = api.hold!;
      api.hold = null;
      held.complete();
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 300));
      await tester.pump();
      expect(api.requests.length, 3);
      expect((api.requests.last['rules'] as List).single['pixel'], 5);
      await preview.stop();
      expect(api.activePreview, isNull);
      preview.dispose();
      final late = PixelCorrectionPreview(
        api: api,
        allowed: () => true,
        configuration: () => config,
        pixels: () => 480,
      );
      api.hold = Completer<void>();
      final response = late.start();
      await tester.pump();
      await late.stop();
      final pending = api.hold!;
      api.hold = null;
      pending.complete();
      await response;
      expect(late.enabled, false);
      expect(late.lease, isNull);
      expect(api.activePreview, isNull);
      late.dispose();
    },
  );
  testWidgets(
    'preview rejects mismatched output and never replays into a new boot',
    (tester) async {
      final api = EditorApi()..wrongReadback = true;
      addTearDown(api.close);
      final preview = PixelCorrectionPreview(
        api: api,
        allowed: () => true,
        configuration: () => {
          'revision': 3,
          'enabled': true,
          'rules': [rule('black', 2)],
        },
        pixels: () => 480,
      );
      await preview.start();
      expect(preview.enabled, false);
      expect(preview.error, contains('readback differs'));
      expect(api.activePreview, isNull);
      api.wrongReadback = false;
      await preview.start();
      final before = api.requests.length;
      api.boot = 'b' * 32;
      await preview.stop();
      expect(api.requests.length, before);
      preview.dispose();
    },
  );
  testWidgets(
    'malformed preview draft fails locally and releases an existing lease',
    (tester) async {
      final api = EditorApi();
      addTearDown(api.close);
      final config = <String, dynamic>{
        'revision': 3,
        'enabled': true,
        'rules': [rule('black', 480)],
      };
      final preview = PixelCorrectionPreview(
        api: api,
        allowed: () => true,
        configuration: () => config,
        pixels: () => 480,
      );
      await preview.start();
      expect(preview.enabled, false);
      expect(preview.pending, false);
      expect(preview.error, isNotNull);
      expect(api.requests, isEmpty);
      config['rules'] = [rule('black', 2)];
      await preview.start();
      expect(preview.lease, isNotNull);
      config['rules'] = null;
      preview.edited();
      await tester.pump(const Duration(milliseconds: 300));
      await tester.pump();
      expect(preview.enabled, false);
      expect(preview.pending, false);
      expect(preview.error, isNotNull);
      expect(api.activePreview, isNull);
      expect(api.requests.last['clear'], true);
      preview.dispose();
    },
  );
  testWidgets(
    'preview ends on tab hide and background without restarting on return',
    (tester) async {
      tabletViewport(tester);
      final api = EditorApi()..value['rules'] = [rule('black', 3)];
      addTearDown(api.close);
      await tester.pumpWidget(page(api));
      await tester.pumpAndSettle();
      expect(api.requests, isEmpty);
      await tester.ensureVisible(find.text('Live preview'));
      await tester.tap(find.text('Live preview'));
      await tester.pumpAndSettle();
      expect(api.activePreview, isNotNull);
      await tester.pumpWidget(page(api, active: false));
      await tester.pumpAndSettle();
      expect(api.activePreview, isNull);
      final count = api.requests.length;
      await tester.pumpWidget(page(api));
      await tester.pumpAndSettle();
      expect(api.requests.length, count);
      await tester.ensureVisible(find.text('Live preview'));
      await tester.tap(find.text('Live preview'));
      await tester.pumpAndSettle();
      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.inactive);
      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.hidden);
      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.paused);
      await tester.pumpAndSettle();
      expect(api.activePreview, isNull);
      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.hidden);
      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.inactive);
      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
      await tester.pumpAndSettle();
      expect(api.activePreview, isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );
  for (final size in [const Size(320, 1000), const Size(1100, 1000)]) {
    testWidgets(
      'correction rule editor at $size has no overflow or automatic writes',
      (tester) async {
        tester.view.physicalSize = size;
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);
        final api = EditorApi()..value['rules'] = [rule('shift', 4, -1)];
        addTearDown(api.close);
        await tester.pumpWidget(page(api));
        await tester.pumpAndSettle();
        await tester.scrollUntilVisible(
          find.byTooltip('Increase offset'),
          220,
          scrollable: find.byType(Scrollable).first,
        );
        await tester.pumpAndSettle();
        expect(tester.takeException(), isNull);
        expect(api.requests, isEmpty);
        await tester.ensureVisible(
          find.byType(DropdownButtonFormField<String>),
        );
        await tester.pumpAndSettle();
        await tester.tap(find.byType(DropdownButtonFormField<String>));
        await tester.pumpAndSettle();
        expect(
          find.text(correctionOperationDescription('remove')),
          findsOneWidget,
        );
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      },
    );
  }
  testWidgets(
    'actions remain pinned while only rules scroll and drafts survive',
    (tester) async {
      tester.view.physicalSize = const Size(1100, 750);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = EditorApi()
        ..value['rules'] = List.generate(
          12,
          (index) => rule('black', index + 1),
        );
      addTearDown(api.close);
      await tester.pumpWidget(
        page(
          api,
          workspace: PixelDiagnosticWorkspace(saved: {'position': '336'}),
        ),
      );
      await tester.pumpAndSettle();
      final actions = tester.getRect(
        find.byKey(const ValueKey('correction-fixed-actions')),
      );
      final cursor = tester.getRect(
        find.byKey(const ValueKey('correction-use-cursor')),
      );
      await tester.enterText(find.byType(TextFormField).first, '481');
      await tester.drag(
        find.byKey(const PageStorageKey('pixel-correction-rules')),
        const Offset(0, -600),
      );
      await tester.pumpAndSettle();
      expect(
        tester.getRect(find.byKey(const ValueKey('correction-fixed-actions'))),
        actions,
      );
      expect(
        tester.getRect(find.byKey(const ValueKey('correction-use-cursor'))),
        cursor,
      );
      expect(
        tester
            .widget<TextFormField>(find.byType(TextFormField).first)
            .controller!
            .text,
        '481',
      );
      expect(api.requests, isEmpty);
      expect(find.byKey(const ValueKey('compact-rule-0')), findsOneWidget);
      expect(tester.getSize(find.byType(Card).first).height, lessThan(180));
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets('operation menu uses current pixel and observed colors', (
    tester,
  ) async {
    tabletViewport(tester);
    final api = EditorApi()
      ..value['rules'] = [rule('remove', 335)]
      ..frameRgb = List.generate(
        480,
        (index) => index == 336 ? '00ff00' : 'ff0000',
      ).join();
    addTearDown(api.close);
    await tester.pumpWidget(page(api));
    await tester.pumpAndSettle();
    await tester.tap(find.byType(DropdownButtonFormField<String>));
    await tester.pumpAndSettle();
    final diagrams = tester.widgetList<PixelCorrectionDiagram>(
      find.byType(PixelCorrectionDiagram),
    );
    expect(diagrams, isNotEmpty);
    for (final diagram in diagrams) {
      expect(diagram.start, 333);
      expect(diagram.focus, 335);
      expect(diagram.rawRgb![336], 0x00ff00);
    }
    expect(tester.takeException(), isNull);
    expect(api.requests, isEmpty);
    await tester.pumpWidget(const SizedBox());
  });

  for (final width in [250.0, 1000.0]) {
    testWidgets(
      'pixel previews keep true circles and actual RGB at width $width',
      (tester) async {
        await tester.pumpWidget(
          MaterialApp(
            home: Scaffold(
              body: Align(
                alignment: Alignment.topLeft,
                child: SizedBox(
                  width: width,
                  child: const PixelCorrectionDiagram(
                    sources: [0, 1, -1, 2, 3, 4],
                    start: 330,
                    focus: 332,
                    rawRgb: [0xff0000, 0x00ff00, 0x0000ff, 0xffffff, 0],
                  ),
                ),
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        final circle = find.byKey(const ValueKey('wire-pixel-330'));
        expect(tester.getSize(circle), const Size(38, 38));
        final decoration =
            tester.widget<Container>(circle).decoration! as BoxDecoration;
        expect(decoration.shape, BoxShape.circle);
        expect(decoration.color, const Color(0xffff0000));
        expect(tester.takeException(), isNull);
      },
    );
  }
  testWidgets('explicit simulated corrections screenshot, no board writes', (
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
    final phone =
        Platform.environment['PAKAR_MOBILE_CAPTURE_VIEW']?.endsWith('Phone') ==
        true;
    tester.view.physicalSize = phone
        ? const Size(390, 1180)
        : const Size(1100, 1100);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final boundary = GlobalKey(),
        api = EditorApi()..value['rules'] = [rule('shift', 18, -1)];
    addTearDown(api.close);
    final workspace = PixelDiagnosticWorkspace(saved: {'position': '19'});
    await tester.pumpWidget(
      RepaintBoundary(
        key: boundary,
        child: page(api, workspace: workspace),
      ),
    );
    await tester.pumpAndSettle();
    expect(api.requests, isEmpty);
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
}
