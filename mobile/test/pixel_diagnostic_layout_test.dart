import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';
import 'package:pakarcontroller/pixel_diagnostic_page.dart';
import 'package:pakarcontroller/pixel_groups.dart';

import 'pixel_diagnostic_page_test.dart' show DiagnosticApi, diagnosticFixture;

DeviceStatus stripFixture(int count, {bool corrections = false}) {
  final base = diagnosticFixture().json;
  final cap = Map<String, dynamic>.from(base['capabilities'] as Map);
  final keys = count == 12;
  final groups = PixelGroupLayout.defaults(keys: keys).draft(
    count == 20
        ? {1: 20, 2: 1, 3: 1, 4: 2}
        : count != 6 && !keys
        ? {1: count}
        : {},
  );
  return DeviceStatus({
    ...base,
    'pixels': keys ? 216 : 480,
    'capabilities': {
      ...cap,
      'role': keys ? 'first-wins' : 'table-selection',
      'acknowledged_features': [
        ...(cap['acknowledged_features'] as List),
        'pixel-group-layout',
        if (corrections) 'pixel-corrections',
      ],
    },
    'pixel_groups': {
      ...groups.configuration(),
      'default_count': groups.defaultCount,
      'capacity': groups.capacity,
      'group_count': groups.groupCount,
      'used_pixels': groups.usedPixels,
      'index_base': 0,
      'counts': groups.counts,
      'starts': groups.starts,
    },
  });
}

Widget stripPage(
  DiagnosticApi api,
  DeviceStatus status, {
  double textScale = 1,
  TextDirection direction = TextDirection.ltr,
  PixelProbe? initialProbe,
  Future<void> Function(int)? longPress,
}) {
  api.current = status;
  api.observedAt = DateTime.now();
  return MaterialApp(
    theme: studioTheme(),
    home: MediaQuery(
      data: MediaQueryData(textScaler: TextScaler.linear(textScale)),
      child: Directionality(
        textDirection: direction,
        child: Scaffold(
          body: PixelDiagnosticPage(
            api: api,
            observed: () => status,
            busyChanged: (_) {},
            initialProbe: initialProbe,
            onPixelLongPress: longPress,
          ),
        ),
      ),
    ),
  );
}

void viewport(WidgetTester tester, Size size) {
  tester.view.physicalSize = size;
  tester.view.devicePixelRatio = 1;
  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);
}

void main() {
  for (final size in [const Size(390, 1100), const Size(1100, 900)]) {
    for (final count in [1, 5, 6, 12, 20]) {
      testWidgets('Physical $count-pixel strip uses available width at $size', (
        tester,
      ) async {
        viewport(tester, size);
        final api = DiagnosticApi();
        addTearDown(api.close);
        await tester.pumpWidget(stripPage(api, stripFixture(count)));
        await tester.pumpAndSettle();
        final strip = tester.getRect(
          find.byKey(const Key('pixel-sector-strip')),
        );
        final slots = [
          for (var index = 0; index < count; index++)
            tester.getRect(find.byKey(Key('pixel-sector-$index'))),
        ];
        final firstRow = slots.where(
          (slot) => (slot.top - slots.first.top).abs() < 1,
        );
        expect(firstRow.first.left, closeTo(strip.left, .1));
        expect(firstRow.last.right, closeTo(strip.right, .1));
        for (var index = 0; index < count; index++) {
          final circle = tester.getRect(
            find.byKey(Key('pixel-sector-circle-$index')),
          );
          expect(circle.width, closeTo(circle.height, .01));
          // Preserve the original circle size independent of slot width.
          expect(circle.width, closeTo(25, .01));
          expect(slots[index].width, greaterThanOrEqualTo(44));
          expect(slots[index].height, greaterThanOrEqualTo(48));
          expect(slots[index].height, lessThanOrEqualTo(68));
          expect(slots[index].left, greaterThanOrEqualTo(strip.left - .1));
          expect(slots[index].right, lessThanOrEqualTo(strip.right + .1));
          for (var other = 0; other < index; other++) {
            expect(slots[index].overlaps(slots[other]), false);
          }
        }
        if (size.width > 1000 && count == 6) {
          expect(firstRow.length, 6);
          expect(slots.first.width, greaterThan(150));
          expect(
            tester
                .getSize(find.byKey(const Key('pixel-sector-circle-0')))
                .width,
            25,
          );
        }
        expect(api.sent, isEmpty);
        expect(api.reads, 0);
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      });
    }
  }

  testWidgets('Large-text RTL retains round previews and physical wire order', (
    tester,
  ) async {
    viewport(tester, const Size(390, 1300));
    final api = DiagnosticApi();
    addTearDown(api.close);
    await tester.pumpWidget(
      stripPage(
        api,
        stripFixture(6),
        textScale: 1.6,
        direction: TextDirection.rtl,
      ),
    );
    await tester.pumpAndSettle();
    final first = tester.getRect(find.byKey(const Key('pixel-sector-0')));
    final second = tester.getRect(find.byKey(const Key('pixel-sector-1')));
    expect(first.left, lessThan(second.left));
    expect(first.top, second.top);
    expect(
      tester.getSize(find.byKey(const Key('pixel-sector-circle-0'))),
      const Size(25, 25),
    );
    expect(tester.takeException(), isNull);
    expect(api.sent, isEmpty);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets(
    'Long press opens supported physical-pixel actions without a probe',
    (tester) async {
      viewport(tester, const Size(1100, 1000));
      final api = DiagnosticApi();
      addTearDown(api.close);
      final status = stripFixture(6, corrections: true);
      final actions = <int>[];
      await tester.pumpWidget(
        stripPage(
          api,
          status,
          initialProbe: PixelProbe(index: 479, layout: status.pixelGroups),
          longPress: (index) async => actions.add(index),
        ),
      );
      await tester.pumpAndSettle();
      await tester.longPress(find.byKey(const Key('pixel-sector-5')));
      await tester.pumpAndSettle();
      expect(actions, [479]);
      expect(api.sent, isEmpty);
      expect(api.reads, 0);
      api.pushes.add(DeviceStatus({...status.json, 'boot_id': 'b' * 32}));
      await tester.pump();
      expect(
        tester
            .widget<InkWell>(find.byKey(const Key('pixel-sector-5')))
            .onLongPress,
        isNull,
      );
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets('Unsupported board has no correction long-press action', (
    tester,
  ) async {
    viewport(tester, const Size(1100, 1000));
    final api = DiagnosticApi();
    addTearDown(api.close);
    var actions = 0;
    await tester.pumpWidget(
      stripPage(api, stripFixture(6), longPress: (_) async => actions++),
    );
    await tester.pumpAndSettle();
    expect(
      tester
          .widget<InkWell>(find.byKey(const Key('pixel-sector-0')))
          .onLongPress,
      isNull,
    );
    expect(actions, 0);
    expect(api.sent, isEmpty);
    await tester.pumpWidget(const SizedBox());
  });
}
