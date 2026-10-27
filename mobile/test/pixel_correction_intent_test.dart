import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_corrections_editor.dart';
import 'package:pakarcontroller/pixel_corrections_page.dart';

import 'pixel_corrections_editor_test.dart'
    show EditorApi, rule, tabletViewport;

class IntentApi extends EditorApi {
  Completer<void>? firstRead;
  int reads = 0;
  bool correctionCapability = true;
  @override
  DeviceStatus get freshStatus {
    final base = super.freshStatus;
    if (correctionCapability) return base;
    final cap = Map<String, dynamic>.from(base.json['capabilities'] as Map);
    return DeviceStatus({
      ...base.json,
      'capabilities': {
        ...cap,
        'acknowledged_features': (cap['acknowledged_features'] as List)
            .where((value) => value != 'pixel-corrections')
            .toList(),
      },
    });
  }

  @override
  Future<Map<String, dynamic>> getJson(String path) async {
    if (reads++ == 0) await firstRead?.future;
    return super.getJson(path);
  }
}

Widget page(
  IntentApi api,
  GlobalKey<PixelCorrectionsPageState> key,
  PixelCorrectionRuleIntent? intent, {
  bool active = true,
}) => MaterialApp(
  home: Scaffold(
    body: PixelCorrectionsPage(
      key: key,
      api: api,
      busyChanged: (_) {},
      incomingRule: intent,
      active: active,
    ),
  ),
);

void main() {
  testWidgets(
    'incoming rule waits for verified load and creates only a selected local draft',
    (tester) async {
      tabletViewport(tester);
      final api = IntentApi()..firstRead = Completer<void>();
      api.value['rules'] = [rule('black', 2)];
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      final intent = PixelCorrectionRuleIntent(
        kind: 'shift',
        pixel: 334,
        offset: -1,
      );
      await tester.pumpWidget(page(api, key, intent));
      await tester.pump();
      expect(key.currentState!.saved, isNull);
      expect(key.currentState!.rules, isEmpty);
      api.firstRead!.complete();
      await tester.pumpAndSettle();
      expect(key.currentState!.rules, [rule('black', 2), intent.toRule()]);
      expect(key.currentState!.selectedRule, intent.toRule());
      expect(key.currentState!.dirty, true);
      expect(api.requests, isEmpty);
      expect(api.saves, 0);
      await tester.pumpWidget(page(api, key, intent));
      await tester.pumpAndSettle();
      expect(key.currentState!.rules, hasLength(2));
      expect(api.reads, 1);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'repeat explicit intent appends while retaining existing dirty rules',
    (tester) async {
      tabletViewport(tester);
      final api = IntentApi();
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await tester.pumpWidget(
        page(api, key, PixelCorrectionRuleIntent(kind: 'black', pixel: 20)),
      );
      await tester.pumpAndSettle();
      final first = key.currentState!.rules.first;
      await tester.pumpWidget(
        page(api, key, PixelCorrectionRuleIntent(kind: 'remove', pixel: 21)),
      );
      await tester.pumpAndSettle();
      expect(key.currentState!.rules, hasLength(2));
      expect(identical(first, key.currentState!.rules.first), true);
      expect(key.currentState!.rules.last['kind'], 'remove');
      expect(key.currentState!.dirty, true);
      expect(api.reads, 1);
      expect(api.requests, isEmpty);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets('hidden tab keeps intent pending until active', (tester) async {
    tabletViewport(tester);
    final api = IntentApi();
    addTearDown(api.close);
    final key = GlobalKey<PixelCorrectionsPageState>();
    final intent = PixelCorrectionRuleIntent(kind: 'black', pixel: 0);
    await tester.pumpWidget(page(api, key, intent, active: false));
    await tester.pumpAndSettle();
    expect(key.currentState!.saved, isNotNull);
    expect(key.currentState!.rules, isEmpty);
    await tester.pumpWidget(page(api, key, intent));
    await tester.pumpAndSettle();
    expect(key.currentState!.rules, [intent.toRule()]);
    expect(api.requests, isEmpty);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets(
    'new intent stops existing live preview and never sends its draft',
    (tester) async {
      tabletViewport(tester);
      final api = IntentApi();
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await tester.pumpWidget(page(api, key, null));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const ValueKey('correction-live-preview')));
      await tester.pumpAndSettle();
      expect(api.activePreview, isNotNull);
      await tester.pumpWidget(
        page(api, key, PixelCorrectionRuleIntent(kind: 'black', pixel: 8)),
      );
      await tester.pumpAndSettle();
      expect(key.currentState!.rules, hasLength(1));
      expect(key.currentState!.preview.enabled, false);
      expect(api.activePreview, isNull);
      expect(api.requests, hasLength(2));
      expect(api.requests.last['clear'], true);
      expect(api.saves, 0);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'stale peer, missing capability, limits and invalid output slots cannot append',
    (tester) async {
      tabletViewport(tester);
      for (final reason in ['peer', 'capability', 'limit', 'range']) {
        final api = IntentApi();
        addTearDown(api.close);
        if (reason == 'limit') api.value['max_rules'] = 0;
        final key = GlobalKey<PixelCorrectionsPageState>();
        await tester.pumpWidget(page(api, key, null));
        await tester.pumpAndSettle();
        if (reason == 'peer') api.boot = 'b' * 32;
        if (reason == 'capability') api.correctionCapability = false;
        await tester.pumpWidget(
          page(
            api,
            key,
            PixelCorrectionRuleIntent(
              kind: 'black',
              pixel: reason == 'range' ? 480 : 0,
            ),
          ),
        );
        await tester.pumpAndSettle();
        expect(key.currentState!.rules, isEmpty, reason: reason);
        expect(key.currentState!.dirty, false, reason: reason);
        expect(key.currentState!.error, isNotNull, reason: reason);
        expect(api.requests, isEmpty, reason: reason);
        await tester.pumpWidget(const SizedBox());
      }
    },
  );
}
