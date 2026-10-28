import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/pixel_correction_menu.dart';
import 'package:pakarcontroller/pixel_correction_address.dart';
import 'package:pakarcontroller/pixel_corrections_editor.dart';
import 'package:pakarcontroller/pixel_corrections_page.dart';

import 'pixel_tools_navigation_test.dart' show PixelMenuApi, pixelPeer;

class RemappedMenuApi extends PixelMenuApi {
  List<int> sources = List.generate(480, (index) => index);
  @override
  Future<Map<String, dynamic>> getJson(String path) async => {
    ...await super.getJson(path),
    'sources': sources,
  };
}

void main() {
  test('diagnostic raw addresses invert saved output without changing persisted indexes', () {
    final identity = List.generate(480, (index) => index);
    expect(pixelCorrectionOutputSlots(raw: 0, pixels: 480, sources: identity), [
      0,
    ]);
    expect(
      pixelCorrectionOutputSlots(raw: 479, pixels: 480, sources: identity),
      [479],
    );
    final removed = pixelCorrectionSources(480, true, [
      {'kind': 'remove', 'pixel': 0, 'offset': 0},
    ]);
    expect(
      pixelCorrectionOutputSlots(raw: 335, pixels: 480, sources: removed),
      [334],
    );
    expect(
      pixelCorrectionOutputSlots(raw: 0, pixels: 480, sources: removed),
      isEmpty,
    );
    expect(
      pixelCorrectionOutputSlots(raw: 2, pixels: 4, sources: [0, 2, 2, -1]),
      [1, 2],
    );
    expect(
      () => pixelCorrectionOutputSlots(raw: 0, pixels: 4, sources: [0, 1]),
      throwsFormatException,
    );
    expect(
      () =>
          pixelCorrectionOutputSlots(raw: 0, pixels: 4, sources: [0, 1, 2, 4]),
      throwsFormatException,
    );
  });
  testWidgets(
    'Actual diagnostic long press opens exact correction draft without sending',
    (tester) async {
      SharedPreferences.setMockInitialValues({});
      FlutterSecureStorage.setMockInitialValues({});
      tester.view.physicalSize = const Size(1200, 1000);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = PixelMenuApi()
        ..healthyWs = true
        ..observed = pixelPeer(corrections: true);
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.byTooltip('Device context menu'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Pixel diagnostics…'));
      await tester.pumpAndSettle();
      await tester.longPress(find.byKey(const Key('pixel-sector-4')));
      await tester.pumpAndSettle();
      expect(find.text('Correct output slot 5'), findsOneWidget);
      await tester.tap(find.byKey(const ValueKey('correction-menu-shift--1')));
      await tester.pumpAndSettle();
      expect(tester.widget<TabBar>(find.byType(TabBar)).controller!.index, 1);
      final editor = tester.state<PixelCorrectionsPageState>(
        find.byType(PixelCorrectionsPage),
      );
      expect(editor.dirty, true);
      expect(editor.rules.single['pixel'], 4);
      expect(editor.rules.single['kind'], 'shift');
      expect(editor.rules.single['offset'], -1);
      expect(api.writes, 0);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  for (final width in [360.0, 1200.0]) {
    for (final operation in [
      ('black', 0),
      ('remove', 0),
      ('shift', 1),
      ('shift', -1),
    ]) {
      testWidgets(
        'Correction menu $operation fits $width and targets exact output slot',
        (tester) async {
          tester.view.physicalSize = Size(width, 760);
          tester.view.devicePixelRatio = 1;
          addTearDown(tester.view.resetPhysicalSize);
          addTearDown(tester.view.resetDevicePixelRatio);
          PixelCorrectionRuleIntent? selected;
          final api = PixelMenuApi()..observed = pixelPeer(corrections: true);
          addTearDown(api.close);
          await tester.pumpWidget(
            MaterialApp(
              home: Builder(
                builder: (context) => Scaffold(
                  body: TextButton(
                    onPressed: () async {
                      selected = await showPixelCorrectionMenu(
                        context,
                        335,
                        api: api,
                        authorize: () => true,
                      );
                    },
                    child: const Text('Open'),
                  ),
                ),
              ),
            ),
          );
          await tester.tap(find.text('Open'));
          await tester.pumpAndSettle();
          expect(find.text('Correct output slot 336'), findsOneWidget);
          expect(selected, isNull);
          await tester.tap(
            find.byKey(
              ValueKey('correction-menu-${operation.$1}-${operation.$2}'),
            ),
          );
          await tester.pumpAndSettle();
          expect(selected?.pixel, 335);
          expect(selected?.kind, operation.$1);
          expect(selected?.offset, operation.$2);
          expect(selected?.revision, 1);
          expect(tester.takeException(), isNull);
        },
      );
    }
  }
  for (final ambiguous in [false, true]) {
    testWidgets(
      'remapped diagnostic chooses correct wire, ambiguous=$ambiguous',
      (tester) async {
        final api = RemappedMenuApi()..observed = pixelPeer(corrections: true);
        addTearDown(api.close);
        api.sources = pixelCorrectionSources(480, true, [
          {'kind': 'remove', 'pixel': 0, 'offset': 0},
        ]);
        if (ambiguous) api.sources[333] = 335;
        PixelCorrectionRuleIntent? selected;
        await tester.pumpWidget(
          MaterialApp(
            home: Builder(
              builder: (context) => Scaffold(
                body: TextButton(
                  onPressed: () async =>
                      selected = await showPixelCorrectionMenu(
                        context,
                        335,
                        api: api,
                        authorize: () => true,
                      ),
                  child: const Text('Open'),
                ),
              ),
            ),
          ),
        );
        await tester.tap(find.text('Open'));
        await tester.pumpAndSettle();
        if (ambiguous) {
          expect(
            tester
                .widget<ListTile>(
                  find.byKey(const ValueKey('correction-menu-black-0')),
                )
                .enabled,
            false,
          );
          await tester.tap(find.byKey(const Key('correction-output-choice')));
          await tester.pumpAndSettle();
          await tester.tap(find.text('Output slot 335').last);
          await tester.pumpAndSettle();
        }
        expect(find.text('Correct output slot 335'), findsOneWidget);
        await tester.tap(find.byKey(const ValueKey('correction-menu-black-0')));
        await tester.pumpAndSettle();
        expect(selected?.pixel, 334);
        expect(selected?.revision, 1);
        expect(api.writes, 0);
      },
    );
  }
}
