import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/cell_pixel_preview.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';
import 'package:pakarcontroller/pixel_groups.dart';
import 'package:pakarcontroller/pixel_groups_page.dart';

import 'peer_fixture.dart';

Map<String, dynamic> groupJson({
  Map<int, int> overrides = const {},
  int revision = 0,
  bool keys = false,
}) {
  final layout = PixelGroupLayout.defaults(keys: keys).draft(overrides);
  return {
    'revision': revision,
    'default_count': layout.defaultCount,
    'capacity': layout.capacity,
    'group_count': layout.groupCount,
    'index_base': 0,
    'used_pixels': layout.usedPixels,
    'counts': layout.counts,
    'starts': layout.starts,
    'overrides': layout.configuration()['overrides'],
  };
}

DeviceStatus groupBoard(Map<String, dynamic> groups) => DeviceStatus({
  ...peerFixture(
    features: [
      'table',
      'leds',
      'command-ack',
      'pixel-diagnostic',
      'pixel-group-layout',
    ],
  ).json,
  'board_id': 'groups-fixture',
  'boot_id': 'a' * 32,
  'pixels': 480,
  'mode': 'cell',
  'effect': 'none',
  'pixel_groups': groups,
  'cells': List.filled(80, 'white'),
});

class GroupsFixture extends PakarApi {
  Map<String, dynamic> groups = groupJson();
  final pushes = StreamController<DeviceStatus>.broadcast(sync: true);
  DeviceStatus? observed;
  int writes = 0;
  bool badReadback = false;
  bool canonicalNoOp = false;
  @override
  DeviceStatus get freshStatus => observed ?? groupBoard(groups);
  @override
  Stream<DeviceStatus> get statusUpdates => pushes.stream.map((status) {
    observed = status;
    return status;
  });
  @override
  Future<void> close() async {
    await pushes.close();
    await super.close();
  }

  @override
  Future<DeviceStatus> settledStatus({bool Function()? stillNeeded}) async =>
      freshStatus;
  @override
  Future<Map<String, dynamic>> getJson(String path) async => groups;
  @override
  Future<Map<String, dynamic>> postJson(
    String path,
    Map<String, String> values, {
    bool Function()? authorize,
  }) async {
    expect(authorize?.call(), true);
    final body = jsonDecode(values['configuration']!) as Map;
    expect(body['revision'], groups['revision']);
    writes++;
    if (!badReadback && !canonicalNoOp) {
      groups = groupJson(
        revision: (groups['revision'] as int) + 1,
        overrides: {
          for (final row in body['overrides'] as List)
            row['group'] as int: row['count'] as int,
        },
      );
    }
    return groups;
  }
}

void main() {
  test('Strict layout validates starts, count bounds and capacity; explicit partial derives starts only', () {
    final json = groupJson(overrides: {1: 5, 2: 7});
    final layout = PixelGroupLayout.fromJson(json, keys: false);
    expect(layout.start(2), 5);
    expect(layout.start(3), 12);
    expect(layout.usedPixels, 480);
    final wrong = {
      ...json,
      'starts': [0, 6, ...layout.starts.skip(2)],
    };
    expect(
      () => PixelGroupLayout.fromJson(wrong, keys: false),
      throwsFormatException,
    );
    expect(
      () => PixelGroupLayout.fromJson({...json, 'capacity': 481}, keys: false),
      throwsFormatException,
    );
    expect(
      () => PixelGroupLayout.fromJson({
        ...json,
        'default_count': 6.0,
      }, keys: false),
      throwsFormatException,
    );
    expect(
      () => PixelGroupLayout.defaults().draft({1: 7}),
      throwsFormatException,
    );
    final partial = {...json, 'partial': true}
      ..remove('starts')
      ..remove('overrides');
    final compact = PixelGroupLayout.fromJson(partial, keys: false);
    expect(compact.sameAs(layout), true);
    for (final invalid in [null, 'invalid']) {
      expect(
        () => PixelGroupLayout.fromJson({
          ...partial,
          'overrides': invalid,
        }, keys: false),
        throwsFormatException,
      );
    }
    expect(
      compact.overrides,
      isEmpty,
    ); // Editable override intent must come from GET.
    expect(groupBoard({...json, 'counts': []}).pixelGroups, isNull);
    expect(supportsPixelDiagnostic(groupBoard({...json, 'counts': []})), false);
  });
  test('Variable group stepping including one-pixel group preserves exact physical spans', () {
    final status = groupBoard(
      groupJson(overrides: {1: 5, 2: 1, 3: 7}, revision: 4),
    );
    final probe = PixelProbe.forPhysicalGroup(status, physicalGroup: 1);
    expect(probe.count, 5);
    expect(probe.move(1).first, 5);
    expect(probe.move(1).count, 1);
    expect(probe.move(1).move(1).first, 6);
    expect(probe.move(1).move(1).count, 7);
    expect(probe.move(1).move(-1).first, 0);
    expect(probe.commandFields(status)['group_layout_revision'], '4');
    final changed = groupBoard(
      groupJson(overrides: {1: 5, 2: 1, 3: 7}, revision: 5),
    );
    expect(probe.unavailableReason(changed), isNotNull);
    expect(
      () => PixelProbe.fromCommandFields(status, {
        'index': '5',
        'count': '7',
        'rgb': '1',
        'group_layout_revision': '4',
      }),
      throwsFormatException,
    );
    expect(
      () => PixelProbe.fromCommandFields(status, {
        'index': '0',
        'count': '5',
        'rgb': '1',
      }),
      throwsFormatException,
    );
    final bytes = probe.frame();
    expect(bytes.skip(probe.limit * 3).every((byte) => byte == 0), true);
    final workspace = PixelDiagnosticWorkspace();
    var notifications = 0;
    workspace.addListener(() => notifications++);
    workspace.observe(groupBoard(groupJson()));
    expect(notifications, 1);
    expect(
      workspace.probe.layout?.revision,
      0,
    ); // Revision zero is a real observation.
    workspace.probe = probe.move(1);
    workspace.observe(
      DeviceStatus({
        ...status.json,
        'led_output': {
          'diagnostic': {
            'revision': 1,
            'active': true,
            'off': false,
            'index': 5,
            'count': 1,
            'rgb': 0xffffff,
            'group_layout_revision': 4,
          },
        },
      }),
    );
    expect(workspace.probe.group, true);
    expect(workspace.probe.move(1).count, 7);
  });
  test(
    'Published RGB owns captured group spans and does not borrow fresh layout',
    () {
      final old = groupJson(overrides: {1: 5, 2: 7}, revision: 3);
      final status = groupBoard(
        groupJson(overrides: {1: 4, 2: 8}, revision: 4),
      );
      final frame = {
        'revision': 9,
        'pending': false,
        'physical': List.generate(80, (i) => i + 1),
        'rgb': '00ff00' * 480,
        'group_layout_revision': 3,
        'group_counts': old['counts'],
        'group_starts': old['starts'],
      };
      final observed = DeviceStatus({...status.json, 'pixel_frame': frame});
      expect(
        cellPixelPreview(observed, CellMap.defaults(), 1)!.colors.length,
        5,
      );
      expect(
        cellPixelPreview(observed, CellMap.defaults(), 2)!.colors.length,
        7,
      );
      final event = {
        'type': 'pixel-frame',
        'pixel_preview_requested': true,
        'board_id': status.json['board_id'],
        'boot_id': status.json['boot_id'],
        'pixel_frame': {
          ...frame,
          'group_starts': [0],
        },
      };
      expect(status.withPixelFrameEvent(event), isNull);
    },
  );
  test(
    'Diagnostic observation requires its layout revision and exact span',
    () {
      final status = groupBoard(
        groupJson(overrides: {1: 5, 2: 7}, revision: 2),
      );
      final diagnostic = {
        'revision': 1,
        'active': true,
        'off': false,
        'index': 5,
        'count': 7,
        'rgb': 255,
        'group_layout_revision': 2,
      };
      DeviceStatus observed(Map<String, dynamic> d) => DeviceStatus({
        ...status.json,
        'led_output': {'diagnostic': d},
      });
      expect(
        PixelDiagnosticObservation.read(observed(diagnostic))?.probe.count,
        7,
      );
      expect(
        PixelDiagnosticObservation.read(
          observed({...diagnostic, 'group_layout_revision': 1}),
        ),
        isNull,
      );
      expect(
        PixelDiagnosticObservation.read(observed({...diagnostic, 'index': 6})),
        isNull,
      );
    },
  );
  testWidgets(
    'Group editor is local until save; exact post-ACK readback required',
    (tester) async {
      tester.view.physicalSize = const Size(900, 1200);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = GroupsFixture();
      await tester.pumpWidget(
        MaterialApp(
          theme: studioTheme(),
          home: Scaffold(
            body: PixelGroupsPage(api: api, busyChanged: (_) {}),
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(api.writes, 0);
      await tester.tap(find.byKey(const ValueKey('pixel-group-1')));
      await tester.pumpAndSettle();
      await tester.enterText(find.byType(TextFormField), '5');
      await tester.tap(find.text('Use in draft'));
      await tester.pumpAndSettle();
      expect(api.writes, 0);
      expect(find.text('479 / 480 pixels'), findsOneWidget);
      await tester.tap(find.byKey(const Key('pixel-groups-save')));
      await tester.pumpAndSettle();
      expect(api.writes, 1);
      expect(find.text('Saved and verified on this board'), findsOneWidget);
      await tester.tap(find.byKey(const ValueKey('pixel-group-1')));
      await tester.pumpAndSettle();
      await tester.enterText(find.byType(TextFormField), '4');
      await tester.tap(find.text('Use in draft'));
      await tester.pumpAndSettle();
      api.badReadback = true;
      await tester.tap(find.byKey(const Key('pixel-groups-save')));
      await tester.pumpAndSettle();
      expect(api.writes, 2);
      expect(find.textContaining('readback differs'), findsOneWidget);
      expect(find.text('Unsaved draft'), findsOneWidget);
      await tester.pumpWidget(const SizedBox.shrink());
      api.close();
    },
  );
  testWidgets(
    'Older telemetry does not invalidate saved layout; newer or missing layout does',
    (tester) async {
      tester.view.physicalSize = const Size(900, 1200);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = GroupsFixture();
      await tester.pumpWidget(
        MaterialApp(
          theme: studioTheme(),
          home: Scaffold(
            body: PixelGroupsPage(api: api, busyChanged: (_) {}),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const ValueKey('pixel-group-1')));
      await tester.pumpAndSettle();
      await tester.enterText(find.byType(TextFormField), '5');
      await tester.tap(find.text('Use in draft'));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('pixel-groups-save')));
      await tester.pumpAndSettle();
      expect(api.groups['revision'], 1);
      expect(find.text('Saved and verified on this board'), findsOneWidget);
      api.pushes.add(groupBoard(groupJson()));
      await tester.pumpAndSettle();
      expect(find.textContaining('Layout changed'), findsNothing);
      api.pushes.add(groupBoard(groupJson(revision: 2)));
      await tester.pumpAndSettle();
      expect(find.textContaining('Layout changed'), findsOneWidget);
      expect(api.writes, 1);
      await tester.tap(find.text('Reload'));
      await tester.pumpAndSettle();
      expect(find.textContaining('Layout changed'), findsNothing);
      api.pushes.add(
        DeviceStatus({...groupBoard(groupJson()).json}..remove('pixel_groups')),
      );
      await tester.pumpAndSettle();
      expect(find.textContaining('Layout changed'), findsOneWidget);
      expect(api.writes, 1);
      await tester.pumpWidget(const SizedBox.shrink());
      api.close();
    },
  );
  testWidgets(
    'No-op save accepts unchanged revision and normalized default overrides',
    (tester) async {
      tester.view.physicalSize = const Size(900, 1200);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = GroupsFixture()..canonicalNoOp = true;
      await tester.pumpWidget(
        MaterialApp(
          theme: studioTheme(),
          home: Scaffold(
            body: PixelGroupsPage(api: api, busyChanged: (_) {}),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const ValueKey('pixel-group-1')));
      await tester.pumpAndSettle();
      await tester.enterText(find.byType(TextFormField), '6');
      await tester.tap(find.text('Use in draft'));
      await tester.pumpAndSettle();
      expect(find.text('Unsaved draft'), findsOneWidget);
      await tester.tap(find.byKey(const Key('pixel-groups-save')));
      await tester.pumpAndSettle();
      expect(api.writes, 1);
      expect(api.groups['revision'], 0);
      expect(api.groups['overrides'], isEmpty);
      expect(find.text('Saved and verified on this board'), findsOneWidget);
      expect(find.text('Unsaved draft'), findsNothing);
      expect(find.textContaining('readback differs'), findsNothing);
      await tester.pumpWidget(const SizedBox.shrink());
      api.close();
    },
  );
  testWidgets(
    'Phone group list and numeric dialog wrap without overflow or writes',
    (tester) async {
      tester.view.physicalSize = const Size(360, 740);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = GroupsFixture();
      await tester.pumpWidget(
        MaterialApp(
          theme: studioTheme(),
          home: Scaffold(
            body: PixelGroupsPage(api: api, busyChanged: (_) {}),
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(tester.takeException(), isNull);
      await tester.ensureVisible(find.byKey(const ValueKey('pixel-group-1')));
      await tester.tap(find.byKey(const ValueKey('pixel-group-1')));
      await tester.pumpAndSettle();
      expect(tester.takeException(), isNull);
      await tester.enterText(find.byType(TextFormField), '256');
      await tester.pumpAndSettle();
      expect(
        tester
            .widget<FilledButton>(
              find.widgetWithText(FilledButton, 'Use in draft'),
            )
            .onPressed,
        isNull,
      );
      expect(api.writes, 0);
      await tester.pumpWidget(const SizedBox.shrink());
      api.close();
    },
  );
}
