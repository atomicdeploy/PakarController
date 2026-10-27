import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_correction_address.dart';
import 'package:pakarcontroller/pixel_corrections_page.dart';
import 'package:pakarcontroller/pixel_groups.dart';
import 'package:pakarcontroller/pixel_tools_navigation.dart';

import 'peer_fixture.dart';
import 'pixel_corrections_editor_test.dart'
    show EditorApi, rule, tabletViewport;

class ExitApi extends EditorApi {
  bool differentSavedRules = false;
  int reads = 0;
  @override
  Future<Map<String, dynamic>> getJson(String path) async {
    reads++;
    final value = await super.getJson(path);
    if (differentSavedRules && saves > 0) value['rules'] = <dynamic>[];
    return value;
  }
}

Widget workspace(ExitApi api, GlobalKey<PixelCorrectionsPageState> key) =>
    MaterialApp(
      theme: studioTheme(),
      home: Builder(
        builder: (context) => Scaffold(
          body: TextButton(
            onPressed: () {
              var locked = false;
              Navigator.of(context).push(
                MaterialPageRoute<void>(
                  builder: (context) => StatefulBuilder(
                    builder: (context, update) => PixelToolsNavigation(
                      length: 2,
                      initialIndex: 1,
                      blocked: locked,
                      beforeLeave: (_) async =>
                          await key.currentState?.requestLeave() ?? true,
                      builder: (context, index, controller, select, close) =>
                          Scaffold(
                            appBar: AppBar(
                              leading: BackButton(onPressed: close),
                              bottom: TabBar(
                                controller: controller,
                                onTap: select,
                                tabs: const [
                                  Tab(text: 'Diagnostics tab'),
                                  Tab(text: 'Corrections tab'),
                                ],
                              ),
                            ),
                            body: TabBarView(
                              controller: controller,
                              physics: const NeverScrollableScrollPhysics(),
                              children: [
                                const Center(
                                  child: Text('Diagnostic workspace'),
                                ),
                                PixelCorrectionsPage(
                                  key: key,
                                  api: api,
                                  active: index == 1,
                                  busyChanged: (value) =>
                                      update(() => locked = value),
                                ),
                              ],
                            ),
                          ),
                    ),
                  ),
                ),
              );
            },
            child: const Text('Open pixel tools'),
          ),
        ),
      ),
    );

Future<void> open(
  WidgetTester tester,
  ExitApi api,
  GlobalKey<PixelCorrectionsPageState> key,
) async {
  tabletViewport(tester);
  await tester.pumpWidget(workspace(api, key));
  await tester.tap(find.text('Open pixel tools'));
  await tester.pumpAndSettle();
}

Future<void> dirty(WidgetTester tester) async {
  await tester.tap(find.text('Add rule'));
  await tester.pumpAndSettle();
}

Finder dialogSave() => find.descendant(
  of: find.byType(Dialog),
  matching: find.text('Save to board'),
);

void main() {
  testWidgets('peer capture precedes the first deferred configuration read', (
    tester,
  ) async {
    final api = ExitApi();
    addTearDown(api.close);
    final key = GlobalKey<PixelCorrectionsPageState>();
    await tester.pumpWidget(
      MaterialApp(
        home: Builder(
          builder: (_) {
            WidgetsBinding.instance.addPostFrameCallback(
              (_) => api.boot = 'b' * 32,
            );
            return Scaffold(
              body: PixelCorrectionsPage(
                key: key,
                api: api,
                busyChanged: (_) {},
              ),
            );
          },
        ),
      ),
    );
    await tester.pumpAndSettle();
    expect(key.currentState!.current, false);
    expect(key.currentState!.saved, isNull);
    expect(api.reads, 0);
    expect(api.saves, 0);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('delayed tab construction retains its original route peer', (
    tester,
  ) async {
    final api = ExitApi();
    addTearDown(api.close);
    final original = (
      connection: api.connectionRevision,
      board: api.freshStatus.json['board_id'],
      boot: api.freshStatus.json['boot_id'],
    );
    api.boot = 'b' * 32;
    final key = GlobalKey<PixelCorrectionsPageState>();
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: PixelCorrectionsPage(
            key: key,
            api: api,
            expectedPeer: original,
            busyChanged: (_) {},
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();
    expect(key.currentState!.current, false);
    expect(api.reads, 0);
    expect(api.saves, 0);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets(
    'Back and close serialize one prompt; Cancel retains draft and preview',
    (tester) async {
      final api = ExitApi();
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await open(tester, api, key);
      await tester.tap(find.byKey(const ValueKey('correction-live-preview')));
      await tester.pumpAndSettle();
      await dirty(tester);
      await tester.pump(const Duration(milliseconds: 300));
      await tester.pumpAndSettle();
      final lease = api.activePreview!['lease_id'];
      await tester.tap(find.byType(BackButton));
      await tester.pumpAndSettle();
      final repeated = key.currentState!.requestLeave();
      await tester.pump();
      expect(find.text('Unsaved corrections'), findsOneWidget);
      expect(api.activePreview?['lease_id'], lease);
      await tester.tap(find.text('Cancel'));
      await tester.pumpAndSettle();
      expect(await repeated, false);
      expect(key.currentState!.dirty, true);
      expect(key.currentState!.widget.active, true);
      expect(api.activePreview?['lease_id'], lease);
      expect(api.saves, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );

  testWidgets(
    'tab switch prompts before leaving; Discard restores draft without saving',
    (tester) async {
      final api = ExitApi()..value['rules'] = [rule('black', 5)];
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await open(tester, api, key);
      await dirty(tester);
      await tester.tap(find.text('Diagnostics tab'));
      await tester.pumpAndSettle();
      expect(key.currentState!.widget.active, true);
      expect(find.text('Unsaved corrections'), findsOneWidget);
      await tester.tap(find.text('Discard'));
      await tester.pumpAndSettle();
      expect(find.text('Diagnostic workspace'), findsOneWidget);
      expect(key.currentState!.widget.active, false);
      expect(key.currentState!.rules, [rule('black', 5)]);
      expect(key.currentState!.dirty, false);
      expect(api.saves, 0);
      expect(api.requests, isEmpty);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'Save on exit awaits persistent readback and blocks repeated Back',
    (tester) async {
      final api = ExitApi()..hold = Completer<void>();
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await open(tester, api, key);
      await dirty(tester);
      await tester.pageBack();
      await tester.pumpAndSettle();
      await tester.tap(dialogSave());
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 400));
      expect(api.saves, 1);
      expect(key.currentState!.busy, true);
      await tester.tap(find.byType(BackButton));
      await tester.pump();
      expect(key.currentState, isNotNull);
      expect(find.text('Unsaved corrections'), findsNothing);
      api.hold!.complete();
      await tester.pumpAndSettle();
      expect(find.text('Open pixel tools'), findsOneWidget);
      expect(key.currentState, isNull);
      expect(api.saves, 1);
    },
  );

  testWidgets('readback mismatch stays open and retains dirty edits', (
    tester,
  ) async {
    final api = ExitApi()..differentSavedRules = true;
    addTearDown(api.close);
    final key = GlobalKey<PixelCorrectionsPageState>();
    await open(tester, api, key);
    await dirty(tester);
    await tester.pageBack();
    await tester.pumpAndSettle();
    await tester.tap(dialogSave());
    await tester.pumpAndSettle();
    expect(api.saves, 1);
    expect(key.currentState!.dirty, true);
    expect(find.textContaining('readback differs'), findsOneWidget);
    expect(key.currentState!.widget.active, true);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets(
    'changed boot cannot save or leave through stale Save confirmation',
    (tester) async {
      final api = ExitApi();
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await open(tester, api, key);
      await dirty(tester);
      await tester.pageBack();
      await tester.pumpAndSettle();
      api.boot = 'b' * 32;
      await tester.tap(dialogSave());
      await tester.pumpAndSettle();
      expect(api.saves, 0);
      expect(key.currentState!.dirty, true);
      expect(key.currentState!.widget.active, true);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'pending preview blocks exit; confirmed Discard clears only its lease',
    (tester) async {
      final api = ExitApi();
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await open(tester, api, key);
      await dirty(tester);
      api.hold = Completer<void>();
      await tester.tap(find.byKey(const ValueKey('correction-live-preview')));
      await tester.pump();
      await tester.pageBack();
      await tester.pump();
      expect(find.text('Unsaved corrections'), findsNothing);
      expect(key.currentState, isNotNull);
      api.hold!.complete();
      await tester.pumpAndSettle();
      await tester.pageBack();
      await tester.pumpAndSettle();
      await tester.tap(find.text('Discard'));
      await tester.pumpAndSettle();
      expect(key.currentState, isNull);
      expect(api.activePreview, isNull);
      expect(api.saves, 0);
      expect(
        api.requests.where((request) => request['clear'] == true),
        hasLength(1),
      );
    },
  );

  test('row labels distinguish wire addresses from prior-rule raw sources and custom spans', () {
    final layout = PixelGroupLayout.defaults().draft({1: 5, 2: 7});
    final status = DeviceStatus({
      ...peerFixture(features: ['table', 'pixel-group-layout']).json,
      'pixel_groups': {
        ...layout.configuration(),
        'default_count': 6,
        'capacity': 480,
        'group_count': 80,
        'index_base': 0,
        'used_pixels': 480,
        'counts': layout.counts,
        'starts': layout.starts,
      },
    });
    final map = CellMap(
      physical: List.generate(80, (index) => 80 - index),
      names: List.generate(80, (index) => '${index + 1}'),
      pixelGroups: layout,
    );
    String label(
      List<Map<String, dynamic>> preceding, {
      CellMap? mapping,
      bool current = true,
      bool enabled = true,
    }) => pixelCorrectionAddress(
      wire: 4,
      pixels: 480,
      precedingRules: preceding,
      enabled: enabled,
      current: current,
      status: status,
      mapping: mapping ?? map,
    );
    expect(label([]), 'Wire 5 · raw 5 · ≈ Cell 80 · pixel 5/5 · group 1');
    expect(
      label([rule('remove', 0)]),
      'Wire 5 · raw 6 · ≈ Cell 79 · pixel 1/7 · group 2',
    );
    expect(
      label([rule('black', 4)]),
      contains('black from prior rule · no logical source'),
    );
    expect(
      label([rule('remove', 0)], enabled: false),
      contains('Cell 80 · pixel 5/5'),
    );
    expect(
      label([], mapping: CellMap.defaults()),
      contains('logical cell unavailable'),
    );
    expect(label([], current: false), contains('logical address not current'));
    expect(
      label([rule('shift', 0, 2)]),
      contains('raw 3 · ≈ Cell 80 · pixel 3/5'),
    );
  });

  test(
    'keycontroller labels use box and button/indicator instead of table cells',
    () {
      final base = peerFixture(kind: 'keys', features: ['leds']);
      final status = DeviceStatus({
        ...base.json,
        'capabilities': {
          ...base.json['capabilities'] as Map<String, dynamic>,
          'role': 'first-wins',
          'keys': {'count': 9, 'groups_per_key': 2, 'pixels_per_group': 12},
        },
      });
      final label = pixelCorrectionAddress(
        wire: 12,
        pixels: 216,
        precedingRules: const [],
        enabled: true,
        current: true,
        status: status,
        mapping: null,
      );
      expect(
        label,
        'Wire 13 · raw 13 · ≈ Box 1 indicator · pixel 1/12 · group 2',
      );
      expect(label, isNot(contains('Cell')));
    },
  );
}
