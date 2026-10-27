import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/pixel_corrections_editor.dart';
import 'package:pakarcontroller/pixel_corrections_page.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

import 'pixel_corrections_editor_test.dart'
    show EditorApi, rule, tabletViewport;

class UsbEditorApi extends EditorApi {
  @override
  bool get isUsb => true;
}

Widget editor(
  EditorApi api,
  GlobalKey<PixelCorrectionsPageState> key, {
  PixelCorrectionRuleIntent? intent,
}) => MaterialApp(
  home: Scaffold(
    body: PixelCorrectionsPage(
      key: key,
      api: api,
      busyChanged: (_) {},
      incomingRule: intent,
    ),
  ),
);

void main() {
  testWidgets(
    'USB correction editor loads and previews using selected USB route',
    (tester) async {
      tabletViewport(tester);
      final api = UsbEditorApi();
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await tester.pumpWidget(editor(api, key));
      await tester.pumpAndSettle();
      expect(key.currentState!.saved, isNotNull);
      expect(key.currentState!.error, isNull);
      await tester.tap(find.text('Add rule'));
      await tester.pumpAndSettle();
      expect(api.activePreview, isNotNull);
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(api.saves, 1);
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'revision-bound handoff cannot reinterpret an existing dirty rule sequence',
    (tester) async {
      tabletViewport(tester);
      final api = EditorApi();
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await tester.pumpWidget(editor(api, key));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Add rule'));
      await tester.pumpAndSettle();
      final count = api.requests.length;
      await tester.pumpWidget(
        editor(
          api,
          key,
          intent: PixelCorrectionRuleIntent(
            kind: 'black',
            pixel: 334,
            revision: 3,
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(key.currentState!.rules, hasLength(1));
      expect(key.currentState!.error, contains('mapping changed'));
      expect(api.requests.length, count);
      await tester.pumpWidget(const SizedBox());
    },
  );
  test('preview preference roundtrips through the existing per-board workspace without persisting a lease', () {
    final workspace = PixelDiagnosticWorkspace();
    expect(workspace.correctionLivePreview, true);
    var changes = 0;
    workspace.addListener(() => changes++);
    workspace.correctionLivePreview = false;
    final restored = PixelDiagnosticWorkspace(saved: workspace.snapshot());
    expect(restored.correctionLivePreview, false);
    expect(changes, 1);
    expect(workspace.snapshot().containsKey('lease_id'), false);
    expect(restored.observation, isNull);
  });
  testWidgets(
    'default preview stays read-only on open then edits RAM only; Save persists once',
    (tester) async {
      tabletViewport(tester);
      final api = EditorApi();
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await tester.pumpWidget(editor(api, key));
      await tester.pumpAndSettle();
      expect(key.currentState!.previewOnEdit, true);
      expect(api.requests, isEmpty);
      await tester.tap(find.text('Add rule'));
      await tester.pumpAndSettle();
      expect(api.saves, 0);
      expect(api.activePreview, isNotNull);
      expect(api.requests.single['path'], '/api/pixels/corrections/preview');
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(api.saves, 1);
      expect(api.value['rules'], [rule('black', 0)]);
      expect(api.activePreview, isNull);
      expect(key.currentState!.dirty, false);
      expect(key.currentState!.previewOnEdit, true);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'unchecked preview leaves edits local and never silently reacquires',
    (tester) async {
      tabletViewport(tester);
      final api = EditorApi();
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await tester.pumpWidget(editor(api, key));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const ValueKey('correction-live-preview')));
      await tester.tap(find.text('Add rule'));
      await tester.pumpAndSettle();
      expect(key.currentState!.previewOnEdit, false);
      expect(key.currentState!.dirty, true);
      expect(api.requests, isEmpty);
      await tester.pump(const Duration(seconds: 11));
      expect(api.requests, isEmpty);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'revision-bound diagnostic menu intent previews exact wire and never migrates saved rules',
    (tester) async {
      tabletViewport(tester);
      final api = EditorApi()..value['rules'] = [rule('remove', 0)];
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      final intent = PixelCorrectionRuleIntent(
        kind: 'black',
        pixel: 334,
        revision: 3,
      );
      await tester.pumpWidget(editor(api, key, intent: intent));
      await tester.pumpAndSettle();
      expect(key.currentState!.rules, [rule('remove', 0), intent.toRule()]);
      expect(api.value['rules'], [rule('remove', 0)]);
      expect(api.saves, 0);
      expect(api.activePreview!['rules'], [rule('remove', 0), intent.toRule()]);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'changed mapping revision rejects handoff without preview or saved writes',
    (tester) async {
      tabletViewport(tester);
      final api = EditorApi();
      addTearDown(api.close);
      final key = GlobalKey<PixelCorrectionsPageState>();
      await tester.pumpWidget(
        editor(
          api,
          key,
          intent: PixelCorrectionRuleIntent(
            kind: 'black',
            pixel: 334,
            revision: 2,
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(key.currentState!.rules, isEmpty);
      expect(key.currentState!.error, contains('mapping changed'));
      expect(api.requests, isEmpty);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets('existing rule cursor translates through prior draft rows only', (
    tester,
  ) async {
    tabletViewport(tester);
    final api = EditorApi()
      ..value['rules'] = [rule('remove', 0), rule('black', 8)];
    addTearDown(api.close);
    final key = GlobalKey<PixelCorrectionsPageState>();
    await tester.pumpWidget(editor(api, key));
    await tester.pumpAndSettle();
    key.currentState!.selectedRule = key.currentState!.rules.last;
    key.currentState!.importPixel(335);
    await tester.pumpAndSettle();
    expect(key.currentState!.rules.last['pixel'], 334);
    expect(api.saves, 0);
    expect(api.value['rules'], [rule('remove', 0), rule('black', 8)]);
    await tester.pumpWidget(const SizedBox());
  });
}
