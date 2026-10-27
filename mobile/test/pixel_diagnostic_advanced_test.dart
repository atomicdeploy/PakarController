import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/command_wire.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';
import 'package:pakarcontroller/pixel_diagnostic_page.dart';

import 'pixel_diagnostic_advanced_fixture.dart';
import 'pixel_diagnostic_page_test.dart' show DiagnosticApi, diagnosticFixture;

void main() {
  test('Advanced probe is bounded, typed, capability gated and never raw animated RGB', () {
    const probe = PixelProbe(
      index: 479,
      group: true,
      effect: 'breathe',
      overlay: true,
      periodMs: 450,
      dutyPercent: 25,
      minBrightness: 12,
      maxBrightness: 180,
    );
    final status = advancedDiagnosticFixture();
    final fields = probe.commandFields(status);
    final packet =
        jsonDecode(commandPacket(status, '/api/led/diagnostic', fields)) as Map;
    expect(packet['effect'], 'breathe');
    expect(packet['composite'], 'overlay');
    expect(packet['period_ms'], 450);
    expect(packet['duty_percent'], 25);
    expect(packet['min_brightness'], 12);
    expect(packet['max_brightness'], 180);
    expect(packet['enabled'], true);
    expect(probe.first, 474);
    expect(probe.count, 6);
    expect(() => probe.frame(), throwsFormatException);
    expect(
      () => probe.commandFields(diagnosticFixture()),
      throwsFormatException,
    );
    expect(const PixelProbe().commandFields(diagnosticFixture()).keys, [
      'index',
      'count',
      'rgb',
      'off',
    ]);
    for (final invalid in [
      probe.copyWith(periodMs: 99),
      probe.copyWith(periodMs: 10001),
      probe.copyWith(dutyPercent: 0),
      probe.copyWith(dutyPercent: 100),
      probe.copyWith(minBrightness: 181),
      probe.copyWith(maxBrightness: 256),
      probe.copyWith(effect: 'other'),
    ]) {
      expect(invalid.valid, false);
    }
  });
  test('Advanced readback requires newer matching specification and completed publication', () {
    const probe = PixelProbe(index: 479, effect: 'rainbow', overlay: true);
    expect(
      matchesDiagnosticObservation(
        advancedDiagnosticFixture(probe: probe, revision: 2),
        probe,
        afterRevision: 1,
      ),
      true,
    );
    expect(
      matchesDiagnosticObservation(
        advancedDiagnosticFixture(probe: probe, revision: 1),
        probe,
        afterRevision: 1,
      ),
      false,
    );
    expect(
      matchesDiagnosticObservation(
        advancedDiagnosticFixture(probe: probe, revision: 2, pending: true),
        probe,
      ),
      false,
    );
    expect(
      matchesDiagnosticObservation(
        advancedDiagnosticFixture(
          probe: probe.copyWith(index: 478),
          revision: 2,
        ),
        probe,
      ),
      false,
    );
    final stop = probe.copyWith(enabled: false);
    expect(
      matchesDiagnosticObservation(
        advancedDiagnosticFixture(probe: stop, revision: 3),
        stop,
        afterRevision: 2,
      ),
      true,
    );
    final malformed = advancedDiagnosticFixture(probe: probe, revision: 2);
    (malformed.json['led_output']['diagnostic'] as Map).remove('enabled');
    expect(PixelDiagnosticObservation.read(malformed), isNull);
  });
  test('Effect drafts persist but never persist active command authority', () {
    final workspace = PixelDiagnosticWorkspace()
      ..probe = const PixelProbe(
        index: 22,
        effect: 'blink',
        overlay: true,
        periodMs: 600,
        dutyPercent: 30,
        minBrightness: 20,
      );
    final restored = PixelDiagnosticWorkspace(
      saved: {...workspace.snapshot(), 'active': true, 'enabled': false},
    );
    expect(restored.probe.sameAs(workspace.probe), true);
    expect(restored.probe.enabled, true);
    expect(restored.observation, isNull);
    final invalid = PixelDiagnosticWorkspace(
      saved: {
        'effect': 'bad',
        'period_ms': 5,
        'duty_percent': 500,
        'min_brightness': 200,
        'max_brightness': 100,
      },
    );
    expect(invalid.probe.valid, true);
    expect(invalid.probe.effect, 'solid');
    expect(invalid.probe.periodMs, 1000);
    expect(invalid.probe.minBrightness, 0);
    expect(
      PixelProbe.forPhysicalGroup(
        advancedDiagnosticFixture(),
        physicalGroup: 80,
      ).first,
      474,
    );
    expect(
      () => PixelProbe.forPhysicalGroup(
        advancedDiagnosticFixture(),
        physicalGroup: 81,
      ),
      throwsFormatException,
    );
    workspace.dispose();
    restored.dispose();
    invalid.dispose();
  });

  for (final size in [const Size(390, 1000), const Size(1100, 900)]) {
    testWidgets(
      'Advanced diagnostics $size opens at supplied group without writes and validates timing',
      (tester) async {
        tester.view.physicalSize = size;
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);
        final api = DiagnosticApi()
          ..current = advancedDiagnosticFixture()
          ..observedAt = DateTime.now();
        addTearDown(api.close);
        final workspace = PixelDiagnosticWorkspace();
        addTearDown(workspace.dispose);
        await tester.pumpWidget(
          MaterialApp(
            theme: studioTheme(),
            home: Scaffold(
              body: PixelDiagnosticPage(
                api: api,
                observed: () => api.current,
                workspace: workspace,
                busyChanged: (_) {},
                initialProbe: const PixelProbe(
                  index: 474,
                  group: true,
                  effect: 'blink',
                  overlay: true,
                ),
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        expect(find.text('Pixels 475–480'), findsOneWidget);
        expect(api.sent, isEmpty);
        await tester.scrollUntilVisible(
          find.byKey(const Key('diagnostic-timing')),
          200,
          scrollable: find.byType(Scrollable).first,
        );
        await tester.pumpAndSettle();
        await tester.tap(find.byKey(const Key('diagnostic-timing')));
        await tester.pumpAndSettle();
        await tester.enterText(
          find.byKey(const Key('diagnostic-period_ms')),
          '99',
        );
        api.pushes.add(advancedDiagnosticFixture());
        await tester.pump();
        expect(find.text('99'), findsOneWidget);
        await tester.tap(find.byKey(const Key('diagnostic-timing-apply')));
        await tester.pumpAndSettle();
        expect(find.text('Enter 100–10000'), findsOneWidget);
        expect(api.sent, isEmpty);
        await tester.enterText(
          find.byKey(const Key('diagnostic-period_ms')),
          '650',
        );
        await tester.tap(find.byKey(const Key('diagnostic-timing-apply')));
        await tester.pumpAndSettle();
        expect(workspace.probe.periodMs, 650);
        expect(api.sent, isEmpty);
        await tester.scrollUntilVisible(
          find.byKey(const Key('pixel-show')),
          -200,
          scrollable: find.byType(Scrollable).first,
        );
        await tester.pumpAndSettle();
        await tester.tap(find.byKey(const Key('pixel-show')));
        await tester.pumpAndSettle();
        expect(api.sent.single.$1.effect, 'blink');
        expect(api.sent.single.$1.overlay, true);
        expect(api.sent.single.$1.periodMs, 650);
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      },
    );
  }
  testWidgets(
    'Old-board saved effect explains unavailable and does not send unsupported fields',
    (tester) async {
      final api = DiagnosticApi()
        ..current = diagnosticFixture()
        ..observedAt = DateTime.now();
      addTearDown(api.close);
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: PixelDiagnosticPage(
              api: api,
              observed: () => api.current,
              busyChanged: (_) {},
              initialProbe: const PixelProbe(effect: 'rainbow', overlay: true),
            ),
          ),
        ),
      );
      await tester.tap(find.byKey(const Key('pixel-show')));
      await tester.pumpAndSettle();
      expect(api.sent, isEmpty);
      expect(find.textContaining('Nothing was sent.'), findsOneWidget);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );
}
