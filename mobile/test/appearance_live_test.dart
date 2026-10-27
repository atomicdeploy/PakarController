import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/state_appearance_page.dart';
import 'package:pakarcontroller/color_picker.dart';

DeviceStatus appearance({int motion = 0, String boot = 'boot'}) =>
    DeviceStatus({
      'board_id': 'board',
      'boot_id': boot,
      'state_presentation': {
        'enabled': true,
        'states': {
          'won': {'rgb': 0x00ff00, 'motion': motion},
        },
        'fade_ms': 350,
        'pulse_ms': 1200,
        'ring_ms': 800,
        'chase_ms': 1200,
      },
    });
void main() {
  testWidgets(
    'Open effects page follows peer events and retires replaced boot',
    (tester) async {
      final updates = StreamController<DeviceStatus>.broadcast();
      var actual = appearance(), sends = 0;
      await tester.pumpWidget(
        MaterialApp(
          home: StateAppearancePage(
            status: actual,
            statusUpdates: updates.stream,
            ids: '1',
            readStatus: () async => actual,
            command: (path, values) async {
              sends++;
              actual = appearance(motion: 2);
              return true;
            },
          ),
        ),
      );
      expect(
        tester
            .widget<DropdownButton<int>>(find.byType(DropdownButton<int>))
            .value,
        0,
      );
      updates.add(actual = appearance(motion: 1));
      await tester.pump();
      expect(
        tester
            .widget<DropdownButton<int>>(find.byType(DropdownButton<int>))
            .value,
        1,
      );
      final dropdown = tester.widget<DropdownButton<int>>(
        find.byType(DropdownButton<int>),
      );
      dropdown.onChanged!(2);
      await tester.pumpAndSettle();
      expect(sends, 1);
      expect(
        tester
            .widget<DropdownButton<int>>(find.byType(DropdownButton<int>))
            .value,
        2,
      );
      updates.add(appearance(boot: 'replacement'));
      await tester.pumpAndSettle();
      expect(
        tester
            .widget<DropdownButton<int>>(find.byType(DropdownButton<int>))
            .onChanged,
        isNull,
      );
      await tester.pumpWidget(const SizedBox());
      await updates.close();
    },
  );
  testWidgets(
    'Color picker offers named swatches custom channels and explicit apply',
    (tester) async {
      int? selected;
      await tester.pumpWidget(
        MaterialApp(
          home: Builder(
            builder: (context) => Scaffold(
              body: TextButton(
                onPressed: () async =>
                    selected = await showLedColorPicker(context, initial: 0),
                child: const Text('Pick'),
              ),
            ),
          ),
        ),
      );
      await tester.tap(find.text('Pick'));
      await tester.pumpAndSettle();
      expect(ledColorPalette.length, 16);
      await tester.tap(find.byTooltip('Amber · #FFBF00'));
      await tester.pump();
      expect(find.text('Amber'), findsOneWidget);
      expect(find.byType(Slider), findsNWidgets(3));
      expect(selected, isNull);
      await tester.tap(find.text('Apply'));
      await tester.pumpAndSettle();
      expect(selected, 0xffbf00);
    },
  );
}
