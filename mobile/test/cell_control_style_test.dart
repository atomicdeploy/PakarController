import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/cell_controls.dart';

void main() {
  test(
    'Semantic actions have no idle or disabled outline, including White',
    () {
      for (final brightness in Brightness.values) {
        final colors = ColorScheme.fromSeed(
          seedColor: const Color(0xff5688d9),
          brightness: brightness,
        );
        for (final tone in [
          Colors.white,
          const Color(0xffff0000),
          const Color(0xff00ff00),
          const Color(0xff142851),
          Colors.black,
        ]) {
          final style = cellColorActionStyle(colors, tone);
          expect(style.side!.resolve({})!.color, Colors.transparent);
          expect(
            style.side!.resolve({WidgetState.disabled})!.color,
            Colors.transparent,
          );
          final idle = style.backgroundColor!.resolve({})!;
          final ink = style.foregroundColor!.resolve({})!;
          final high = idle.computeLuminance() > ink.computeLuminance()
              ? idle.computeLuminance()
              : ink.computeLuminance();
          final low = idle.computeLuminance() < ink.computeLuminance()
              ? idle.computeLuminance()
              : ink.computeLuminance();
          expect((high + .05) / (low + .05), greaterThanOrEqualTo(4.5));
          expect(
            idle.a,
            1,
            reason: 'Enabled actions use a solid semantic fill.',
          );
          expect(
            style.backgroundColor!.resolve({WidgetState.disabled}),
            isNot(idle),
          );
        }
      }
    },
  );

  test(
    'Hover, focus and press are distinct without changing border geometry',
    () {
      final style = cellColorActionStyle(
        ColorScheme.fromSeed(
          seedColor: Colors.blue,
          brightness: Brightness.dark,
        ),
        Colors.white,
      );
      final normal = style.side!.resolve({})!;
      for (final state in [
        WidgetState.hovered,
        WidgetState.focused,
        WidgetState.pressed,
        WidgetState.disabled,
      ]) {
        expect(style.side!.resolve({state})!.width, normal.width);
      }
      expect(
        style.overlayColor!.resolve({WidgetState.hovered}),
        isNot(style.overlayColor!.resolve({WidgetState.pressed})),
      );
      expect(
        style.side!.resolve({WidgetState.focused})!.color,
        isNot(Colors.transparent),
      );
      expect(
        style.overlayColor!.resolve({
          WidgetState.disabled,
          WidgetState.hovered,
        }),
        Colors.transparent,
      );
    },
  );

  testWidgets(
    'Compact faces preserve accessible hit areas and disabled guard',
    (tester) async {
      var commands = 0;
      await tester.pumpWidget(
        MaterialApp(
          theme: ThemeData.dark(),
          home: Scaffold(
            body: Wrap(
              children: [
                CellColorAction(
                  value: 'white',
                  label: 'White',
                  onPressed: () => commands++,
                ),
                const CellColorAction(value: 'red', label: 'Red'),
              ],
            ),
          ),
        ),
      );
      final controls = find.byType(FilledButton);
      expect(controls, findsNWidgets(2));
      final control = tester.widget<FilledButton>(controls.first);
      expect(control.style!.minimumSize!.resolve({})!.height, 36);
      expect(tester.getSize(controls.first).height, greaterThanOrEqualTo(48));
      expect(tester.getSize(controls.first).width, lessThan(120));
      await tester.tap(find.text('White'));
      await tester.tap(find.text('Red'));
      expect(commands, 1);
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets('Selection options use quiet tonal surfaces and colored edges', (
    tester,
  ) async {
    final theme = ThemeData.dark();
    var selected = false;
    await tester.pumpWidget(
      MaterialApp(
        theme: theme,
        home: Scaffold(
          body: StatefulBuilder(
            builder: (context, setState) => CellOption(
              label: 'Multiple selection',
              icon: Icons.checklist,
              selected: selected,
              onSelected: (value) => setState(() => selected = value),
            ),
          ),
        ),
      ),
    );
    var chip = tester.widget<FilterChip>(find.byType(FilterChip));
    expect(chip.side!.color, Colors.transparent);
    await tester.tap(find.text('Multiple selection'));
    await tester.pumpAndSettle();
    chip = tester.widget<FilterChip>(find.byType(FilterChip));
    expect(chip.selected, isTrue);
    expect(chip.side!.color, theme.colorScheme.primary.withValues(alpha: .20));
    expect(chip.labelStyle!.color, theme.colorScheme.primary);
    expect(chip.elevation, 0);
    expect(chip.pressElevation, 0);
  });

  testWidgets('Semantic action labels retain the inherited application font', (
    tester,
  ) async {
    await tester.pumpWidget(
      MaterialApp(
        theme: ThemeData(fontFamily: 'Vazirmatn'),
        home: Scaffold(
          body: CellColorAction(
            value: 'white',
            label: 'White',
            onPressed: () {},
          ),
        ),
      ),
    );
    final action = tester.widget<FilledButton>(find.byType(FilledButton));
    final style = action.style!.textStyle!.resolve({})!;
    expect(style.fontFamily, 'Vazirmatn');
    expect(style.fontSize, 13);
    expect(style.height, 1.2);
    expect(style.fontWeight, FontWeight.w600);
    expect(tester.takeException(), isNull);
  });
}
