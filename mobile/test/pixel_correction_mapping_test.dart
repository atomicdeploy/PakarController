import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_corrections_page.dart';
import 'package:pakarcontroller/pixel_groups.dart';

import 'pixel_corrections_editor_test.dart'
    show EditorApi, rule, tabletViewport;

class AddressApi extends EditorApi {
  AddressApi() {
    value['rules'] = [rule('black', 334)];
  }
  final events = StreamController<DeviceStatus>.broadcast(sync: true);
  final configurations = StreamController<int>.broadcast(sync: true);
  PixelGroupLayout layout = PixelGroupLayout.defaults();
  CellMap? overrideMap;
  int mappingReads = 0, failures = 0;
  Completer<void>? mapHold;
  bool keys = false;
  @override
  Stream<DeviceStatus> get statusUpdates => events.stream;
  @override
  Stream<int> get configurationUpdates => configurations.stream;
  @override
  DeviceStatus get freshStatus {
    final base = super.freshStatus.json;
    final cap = Map<String, dynamic>.from(base['capabilities'] as Map);
    return DeviceStatus({
      ...base,
      'capabilities': {
        ...cap,
        'role': keys ? 'first-wins' : 'table',
        'acknowledged_features': [
          ...(cap['acknowledged_features'] as List),
          'pixel-group-layout',
        ],
      },
      'pixel_groups': {
        ...layout.configuration(),
        'default_count': layout.defaultCount,
        'capacity': layout.capacity,
        'group_count': layout.groupCount,
        'index_base': 0,
        'used_pixels': layout.usedPixels,
        'counts': layout.counts,
        'starts': layout.starts,
      },
    });
  }

  @override
  Future<CellMap> mapping() async {
    mappingReads++;
    final result =
        overrideMap ??
        CellMap(
          physical: [
            77,
            69,
            61,
            53,
            45,
            37,
            29,
            21,
            13,
            5,
            76,
            68,
            60,
            52,
            44,
            36,
            28,
            20,
            12,
            4,
            78,
            70,
            62,
            54,
            46,
            38,
            30,
            22,
            14,
            6,
            75,
            67,
            59,
            51,
            43,
            35,
            27,
            19,
            11,
            3,
            79,
            71,
            63,
            55,
            47,
            39,
            31,
            23,
            15,
            7,
            74,
            66,
            58,
            50,
            42,
            34,
            26,
            18,
            10,
            2,
            80,
            72,
            64,
            56,
            48,
            40,
            32,
            24,
            16,
            8,
            73,
            65,
            57,
            49,
            41,
            33,
            25,
            17,
            9,
            1,
          ],
          names: List.generate(80, (i) => '${i + 1}'),
          pixelGroups: layout,
        );
    await mapHold?.future;
    if (failures-- > 0) throw TimeoutException('Fixture map read timed out');
    return result;
  }

  @override
  Future<void> close() async {
    await events.close();
    await configurations.close();
    await super.close();
  }
}

Future<GlobalKey<PixelCorrectionsPageState>> open(
  WidgetTester tester,
  AddressApi api,
) async {
  tabletViewport(tester);
  final key = GlobalKey<PixelCorrectionsPageState>();
  await tester.pumpWidget(
    MaterialApp(
      home: Scaffold(
        body: PixelCorrectionsPage(
          key: key,
          api: api,
          busyChanged: (_) {},
          mapping: () => null,
          livePreviewInitiallyEnabled: false,
        ),
      ),
    ),
  );
  await tester.pumpAndSettle();
  return key;
}

void main() {
  testWidgets(
    'configuration edge reloads names/order without replacing dirty rules',
    (tester) async {
      final api = AddressApi();
      addTearDown(api.close);
      final key = await open(tester, api);
      expect(find.textContaining('≈ Cell 64'), findsOneWidget);
      await tester.tap(find.text('Add rule'));
      await tester.pumpAndSettle();
      api.overrideMap = CellMap(
        physical: List.generate(80, (i) => i + 1),
        names: List.generate(80, (i) => '${i + 1}'),
        pixelGroups: api.layout,
      );
      api.configurations.add(1);
      await tester.pumpAndSettle();
      expect(find.textContaining('≈ Cell 56'), findsOneWidget);
      expect(find.textContaining('≈ Cell 64'), findsNothing);
      expect(api.mappingReads, 2);
      expect(key.currentState!.dirty, true);
      expect(key.currentState!.rules, hasLength(2));
      expect(api.requests, isEmpty);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets('configuration edge fences an older pending map receipt', (
    tester,
  ) async {
    final api = AddressApi()..mapHold = Completer<void>();
    addTearDown(api.close);
    await open(tester, api);
    api.overrideMap = CellMap(
      physical: List.generate(80, (i) => i + 1),
      names: List.generate(80, (i) => '${i + 1}'),
      pixelGroups: api.layout,
    );
    api.configurations.add(1);
    api.mapHold!.complete();
    await tester.pumpAndSettle();
    expect(find.textContaining('≈ Cell 56'), findsOneWidget);
    expect(find.textContaining('≈ Cell 64'), findsNothing);
    expect(api.mappingReads, 2);
    expect(api.requests, isEmpty);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets(
    'editor hydrates actual logical mapping without parent map or writes',
    (tester) async {
      final api = AddressApi();
      addTearDown(api.close);
      final key = await open(tester, api);
      expect(
        find.textContaining(
          'Wire 335 · raw 335 · ≈ Cell 64 · pixel 5/6 · group 56',
        ),
        findsOneWidget,
      );
      expect(key.currentState!.addressMapping, isNotNull);
      expect(api.mappingReads, 1);
      expect(api.requests, isEmpty);
      expect(api.saves, 0);
      for (var i = 0; i < 20; i++) {
        api.events.add(api.freshStatus);
      }
      await tester.pumpAndSettle();
      expect(api.mappingReads, 1);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets('bounded read recovery retains local rule edits and selection', (
    tester,
  ) async {
    final api = AddressApi()..failures = 1;
    addTearDown(api.close);
    final key = await open(tester, api);
    await tester.tap(find.text('Add rule'));
    await tester.pumpAndSettle();
    final selected = key.currentState!.selectedRule;
    expect(key.currentState!.dirty, true);
    await tester.pump(const Duration(seconds: 2));
    await tester.pumpAndSettle();
    expect(find.textContaining('≈ Cell 64'), findsOneWidget);
    expect(key.currentState!.rules, hasLength(2));
    expect(identical(key.currentState!.selectedRule, selected), true);
    expect(key.currentState!.dirty, true);
    expect(api.mappingReads, 2);
    expect(api.requests, isEmpty);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('late map from retired boot never labels replacement peer', (
    tester,
  ) async {
    final api = AddressApi()..mapHold = Completer<void>();
    addTearDown(api.close);
    final key = await open(tester, api);
    api.boot = 'b' * 32;
    api.events.add(api.freshStatus);
    api.mapHold!.complete();
    await tester.pumpAndSettle();
    expect(key.currentState!.addressMapping, isNull);
    expect(find.textContaining('≈ Cell'), findsNothing);
    expect(find.textContaining('logical address not current'), findsOneWidget);
    expect(api.requests, isEmpty);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('stale group spans never fall back and retry is bounded', (
    tester,
  ) async {
    final api = AddressApi()
      ..layout = PixelGroupLayout.defaults().draft({1: 5, 2: 7});
    api.overrideMap = CellMap(
      physical: List.generate(80, (i) => i + 1),
      names: List.generate(80, (i) => '${i + 1}'),
      pixelGroups: PixelGroupLayout.defaults(),
    );
    addTearDown(api.close);
    final key = await open(tester, api);
    for (var i = 0; i < 4; i++) {
      await tester.pump(const Duration(seconds: 2));
      await tester.pumpAndSettle();
    }
    expect(api.mappingReads, 3);
    expect(key.currentState!.addressMapping, isNull);
    expect(find.textContaining('logical cell unavailable'), findsOneWidget);
    api.overrideMap = null;
    await tester.tap(find.byKey(const ValueKey('correction-map-retry')));
    await tester.pumpAndSettle();
    expect(key.currentState!.addressMapping, isNotNull);
    expect(api.mappingReads, 4);
    expect(api.requests, isEmpty);
    await tester.pumpWidget(const SizedBox());
  });
}
