import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_corrections_editor.dart';
import 'package:pakarcontroller/pixel_corrections_page.dart';

import 'peer_fixture.dart';

class CorrectionsFixture extends PakarApi {
  int writes = 0;
  Map<String, dynamic> value = {
    'enabled': true,
    'revision': 0,
    'pixels': 480,
    'rules': <dynamic>[],
    'sources': List.generate(480, (index) => index),
  };
  @override
  DeviceStatus get freshStatus => DeviceStatus({
    ...peerFixture(
      features: [
        'leds',
        'table',
        'pixel-corrections',
        'pixel-correction-rule-toggle',
      ],
    ).json,
    'board_id': 'corrections-fixture',
    'boot_id': 'a' * 32,
  });
  @override
  Future<Map<String, dynamic>> getJson(String path) async =>
      jsonDecode(jsonEncode(value)) as Map<String, dynamic>;
  @override
  Future<Map<String, dynamic>> postJson(
    String path,
    Map<String, String> values, {
    bool Function()? authorize,
  }) async {
    expect(authorize?.call(), true);
    final next = jsonDecode(values['configuration']!) as Map<String, dynamic>;
    expect(next['revision'], value['revision']);
    writes++;
    final rules = (next['rules'] as List)
        .map(
          (rule) => {
            ...Map<String, dynamic>.from(rule as Map),
            'enabled': rule['enabled'] ?? true,
          },
        )
        .toList();
    value = {
      ...value,
      ...next,
      'rules': rules,
      'revision': (value['revision'] as int) + 1,
      'sources': pixelCorrectionSources(480, next['enabled'] == true, rules),
    };
    return value;
  }
}

void main() {
  testWidgets(
    'read does not write; explicit save verifies persistent configuration',
    (tester) async {
      tester.view.physicalSize = const Size(1100, 1400);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = CorrectionsFixture();
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: PixelCorrectionsPage(api: api, busyChanged: (_) {}),
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(api.writes, 0);
      await tester.ensureVisible(find.text('Add rule'));
      await tester.tap(find.text('Add rule'));
      await tester.pumpAndSettle();
      expect(api.writes, 0);
      await tester.ensureVisible(find.text('Save to board'));
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(api.writes, 1);
      expect((api.value['rules'] as List).single, {
        'kind': 'black',
        'pixel': 0,
        'offset': 0,
        'enabled': true,
      });
      await tester.tap(find.byTooltip('Wire mapping'));
      await tester.pumpAndSettle();
      expect(find.textContaining('revision 1'), findsOneWidget);
      await tester.pumpWidget(const SizedBox.shrink());
      api.close();
    },
  );
  testWidgets('invalid pixel is not silently saved as the previous number', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(1100, 1400);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final api = CorrectionsFixture();
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: PixelCorrectionsPage(api: api, busyChanged: (_) {}),
        ),
      ),
    );
    await tester.pumpAndSettle();
    await tester.ensureVisible(find.text('Add rule'));
    await tester.tap(find.text('Add rule'));
    await tester.pumpAndSettle();
    await tester.ensureVisible(find.byType(TextFormField).first);
    await tester.enterText(find.byType(TextFormField).first, '481');
    await tester.ensureVisible(find.text('Save to board'));
    await tester.tap(find.text('Save to board'));
    await tester.pumpAndSettle();
    expect(api.writes, 0);
    expect(find.text('1–480'), findsOneWidget);
    await tester.pumpWidget(const SizedBox.shrink());
    api.close();
  });
}
