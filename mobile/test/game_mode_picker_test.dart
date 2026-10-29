import 'dart:io';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/game_mode_picker.dart';
import 'package:pakarcontroller/minigame.dart';

import 'minigame_test.dart' show miniStatus, fixture;

const catalog = <String, (String, IconData)>{
  'press-to-win': ('Press to win', Icons.emoji_events_outlined),
  'illuminated': ('Illuminated target', Icons.my_location),
  'color-match': ('Color pattern', Icons.palette_outlined),
  'simon': ('Repeat sequence', Icons.queue_music),
  'memory-pairs': ('Matching pairs', Icons.style_outlined),
  'clear-all': ('Clear all', Icons.checklist_rounded),
  'color-order': ('Color order', Icons.sort_rounded),
  'switch-pattern': ('Switch pattern', Icons.grid_view_rounded),
  'reaction-chase': ('Reaction chase', Icons.bolt_rounded),
  'mine-field': ('Mine Field', Icons.diamond_outlined),
  'toggle': ('Toggle', Icons.toggle_on_outlined),
};

MiniGameSnapshot game({String mode = 'press-to-win'}) => MiniGameSnapshot.read({
  ...miniStatus().json['minigame'] as Map,
  'mode': mode,
  'available_modes': [
    for (final entry in catalog.entries)
      {'id': entry.key, 'label': entry.value.$1},
  ],
})!;

Widget page(
  MiniGameSnapshot snapshot,
  ValueChanged<String>? changed, {
  double scale = 1,
  TextDirection direction = TextDirection.ltr,
  bool dark = true,
  bool bold = false,
  GlobalKey? boundary,
}) => MaterialApp(
  theme: dark
      ? studioTheme(palette: const {'primary': 0x38d27a})
      : ThemeData(
          useMaterial3: true,
          fontFamily: 'Vazirmatn',
          colorScheme: ColorScheme.fromSeed(seedColor: const Color(0xff008c58)),
        ),
  builder: (context, child) => Directionality(
    textDirection: direction,
    child: MediaQuery(
      data: MediaQuery.of(context)
          .copyWith(textScaler: TextScaler.linear(scale), boldText: bold),
      child: RepaintBoundary(key: boundary, child: child!),
    ),
  ),
  home: Scaffold(
    appBar: AppBar(title: const Text('Game studio')),
    body: Align(
      alignment: Alignment.topCenter,
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: GameModePicker(game: snapshot, onChanged: changed),
      ),
    ),
  ),
);

void size(WidgetTester tester, Size value) {
  tester.view.physicalSize = value;
  tester.view.devicePixelRatio = 1;
}

void checkNumber(WidgetTester tester, Finder row, String value) {
  final number = find.descendant(of: row, matching: find.text(value)).last;
  final widget = tester.widget<Text>(number);
  final render = tester.renderObject<RenderParagraph>(number);
  expect(widget.maxLines, 1);
  expect(widget.textDirection, TextDirection.ltr);
  expect(
    render.getMaxIntrinsicWidth(double.infinity),
    lessThanOrEqualTo(render.size.width),
  );
  expect(render.didExceedMaxLines, isFalse);
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  setUp(() {
    final before = WidgetController.hitTestWarningShouldBeFatal;
    WidgetController.hitTestWarningShouldBeFatal = true;
    addTearDown(() => WidgetController.hitTestWarningShouldBeFatal = before);
  });
  setUpAll(() async {
    for (final font in ['Vazirmatn', 'MaterialIcons']) {
      await (FontLoader(font)..addFont(
            rootBundle.load(
              font == 'MaterialIcons'
                  ? 'fonts/MaterialIcons-Regular.otf'
                  : 'assets/fonts/Vazirmatn-Regular.ttf',
            ),
          ))
          .load();
    }
  });

  testWidgets(
    'Every advertised mode has its numbered icon and muted description',
    (tester) async {
      size(tester, const Size(390, 844));
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final changes = <String>[];
      final snapshot = game();
      await tester.pumpWidget(page(snapshot, changes.add));
      await tester.tap(find.byKey(const ValueKey('minigame-mode')));
      await tester.pumpAndSettle();
      for (final (index, entry) in catalog.entries.indexed) {
        final row = find.descendant(
          of: find.byType(Scrollable).last,
          matching: find.byKey(ValueKey('game-mode-choice-${entry.key}')),
        );
        await tester.scrollUntilVisible(
          row,
          120,
          scrollable: find.byType(Scrollable).last,
        );
        checkNumber(tester, row, '${index + 1}.');
        expect(
          find.descendant(of: row, matching: find.byIcon(entry.value.$2)),
          findsOneWidget,
        );
        final detail = find.descendant(
          of: row,
          matching: find.text(snapshot.description(entry.key)),
        );
        expect(tester.widget<Text>(detail).maxLines, 2);
        expect(
          tester.widget<Text>(detail).style!.color,
          Theme.of(tester.element(row)).colorScheme.onSurfaceVariant,
        );
        final semantics = tester.widget<Semantics>(
          find.ancestor(of: row, matching: find.byType(Semantics)).first,
        );
        expect(semantics.properties.label, contains(entry.value.$1));
        expect(
          semantics.properties.label,
          contains(snapshot.description(entry.key)),
        );
        final tooltip = tester.widget<Tooltip>(
          find.ancestor(of: row, matching: find.byType(Tooltip)).first,
        );
        expect(tooltip.excludeFromSemantics, isTrue);
      }
      expect(changes, isEmpty);
      await tester.tap(
        find.descendant(
          of: find.byType(Scrollable).last,
          matching: find.text('Toggle'),
        ),
      );
      await tester.pumpAndSettle();
      expect(changes, ['toggle']);
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'Phone tablet RTL large-text and bold numbering keep stable geometry',
    (tester) async {
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final changes = <String>[];
      for (final width in [320.0, 768.0, 1024.0]) {
        for (final scale in [1.0, 2.0, 3.0]) {
          for (final direction in TextDirection.values) {
            size(tester, Size(width, 1100));
            var snapshot = game(mode: 'mine-field');
            await tester.pumpWidget(
              page(
                snapshot,
                changes.add,
                scale: scale,
                direction: direction,
                dark: direction == TextDirection.ltr,
                bold: scale == 3,
              ),
            );
            await tester.pumpAndSettle();
            final picker = find.byKey(const ValueKey('minigame-mode'));
            final before = tester.getRect(picker);
            checkNumber(
              tester,
              find.byKey(const ValueKey('game-mode-choice-mine-field')),
              '10.',
            );
            await tester.tap(picker);
            await tester.pumpAndSettle();
            expect(tester.getRect(picker), before);
            checkNumber(
              tester,
              find.byKey(const ValueKey('game-mode-choice-mine-field')).last,
              '10.',
            );
            expect(
              tester.takeException(),
              isNull,
              reason: '$width / $scale / $direction',
            );
            await tester.sendKeyEvent(LogicalKeyboardKey.escape);
            await tester.pumpAndSettle();
            snapshot = game(mode: 'illuminated');
            await tester.pumpWidget(
              page(
                snapshot,
                changes.add,
                scale: scale,
                direction: direction,
                dark: direction == TextDirection.ltr,
                bold: scale == 3,
              ),
            );
            await tester.pumpAndSettle();
            expect(tester.getRect(picker), before);
            expect(tester.takeException(), isNull);
          }
        }
      }
      expect(changes, isEmpty);
    },
  );

  testWidgets(
    'Keyboard selection is explicit and disabled or unchanged choices do nothing',
    (tester) async {
      final changes = <String>[];
      await tester.pumpWidget(page(game(), changes.add));
      await tester.sendKeyEvent(LogicalKeyboardKey.tab);
      await tester.sendKeyEvent(LogicalKeyboardKey.enter);
      await tester.pumpAndSettle();
      expect(changes, isEmpty);
      await tester.sendKeyEvent(LogicalKeyboardKey.arrowDown);
      await tester.sendKeyEvent(LogicalKeyboardKey.enter);
      await tester.pumpAndSettle();
      expect(changes, ['illuminated']);
      changes.clear();
      await tester.tap(find.byKey(const ValueKey('minigame-mode')));
      await tester.pumpAndSettle();
      final current = find.descendant(
        of: find.byType(Scrollable).last,
        matching: find.text('Press to win'),
      );
      await tester.scrollUntilVisible(
        current,
        -120,
        scrollable: find.byType(Scrollable).last,
      );
      await tester.tap(current);
      await tester.pumpAndSettle();
      expect(changes, isEmpty);
      await tester.pumpWidget(page(game(), null));
      await tester.tap(find.byKey(const ValueKey('minigame-mode')));
      await tester.pumpAndSettle();
      expect(find.byType(Scrollable), findsNothing);
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets('Unknown board mode keeps supplied details and a generic icon', (
    tester,
  ) async {
    final snapshot = MiniGameSnapshot.read({
      ...game().json,
      'mode': 'custom',
      'available_modes': [
        {
          'id': 'custom',
          'label': 'Custom challenge',
          'description': 'Board-provided objective',
        },
      ],
    })!;
    await tester.pumpWidget(page(snapshot, (_) {}));
    expect(find.text('Custom challenge'), findsOneWidget);
    expect(find.text('Board-provided objective'), findsOneWidget);
    expect(find.byIcon(Icons.sports_esports_outlined), findsOneWidget);
    expect(find.text('Simon'), findsNothing);
  });

  testWidgets('Blank optional descriptions retain readable local fallbacks', (
    tester,
  ) async {
    for (final id in ['simon', 'custom']) {
      final snapshot = MiniGameSnapshot.read({
        ...game().json,
        'mode': id,
        'available_modes': [
          {'id': id, 'label': 'Challenge', 'description': '   '},
        ],
      })!;
      await tester.pumpWidget(page(snapshot, (_) {}));
      expect(
        find.text(minigameDescriptions[id] ?? 'Board-provided game'),
        findsOneWidget,
      );
      expect(tester.takeException(), isNull);
    }
  });

  testWidgets('Changing board closes its old mode menu without a command', (
    tester,
  ) async {
    final commands = <String>[];
    Future<bool> send(String path, Map<String, String> values) async {
      commands.add(path);
      return true;
    }

    await tester.pumpWidget(fixture(miniStatus(board: 'keys-A'), send));
    await tester.tap(find.byKey(const ValueKey('minigame-mode')));
    await tester.pumpAndSettle();
    expect(find.text('Switches'), findsOneWidget);
    await tester.pumpWidget(fixture(miniStatus(board: 'keys-B'), send));
    await tester.pumpAndSettle();
    expect(find.text('Switches'), findsNothing);
    expect(commands, isEmpty);
    expect(tester.takeException(), isNull);
  });

  testWidgets('Capture the actual expanded game picker', (tester) async {
    final path = Platform.environment['PAKAR_MOBILE_CAPTURE_PATH'];
    final view = Platform.environment['PAKAR_MOBILE_CAPTURE_VIEW'];
    if (path == null ||
        !const ['GameModePhone', 'GameModeTablet'].contains(view)) {
      return;
    }
    size(
      tester,
      view == 'GameModePhone' ? const Size(390, 844) : const Size(1024, 1200),
    );
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final boundary = GlobalKey();
    await tester.pumpWidget(page(game(), (_) {}, boundary: boundary));
    await tester.tap(find.byKey(const ValueKey('minigame-mode')));
    await tester.pumpAndSettle();
    expect(tester.takeException(), isNull);
    await tester.runAsync(() async {
      final file = File(path);
      expect(await file.exists(), isFalse);
      final render =
          boundary.currentContext!.findRenderObject() as RenderRepaintBoundary;
      final image = await render.toImage(pixelRatio: 1);
      final png = await image.toByteData(format: ui.ImageByteFormat.png);
      await file.writeAsBytes(png!.buffer.asUint8List(), flush: true);
      image.dispose();
    });
  });
}
