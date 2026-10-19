import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/cell_pixel_preview.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

DeviceStatus diagnostic({int count = 6}) => DeviceStatus({
  ...peerFixture(features: ['table', 'leds', 'pixel-diagnostic']).json,
  'pixels': 480,
  'board_id': 'test-board',
  'boot_id': 'test-boot',
  'effect': 'none',
  'led_output': {
    'diagnostic': {
      'revision': 17,
      'active': true,
      'index': 6,
      'count': count,
      'rgb': 0x00ff00,
      'off': false,
    },
  },
});

void main() {
  test(
    'Board diagnostic overrides stale white semantics using physical mapping',
    () {
      final physical = List.generate(80, (i) => i + 1);
      physical[14] = 2;
      physical[1] = 15;
      final mapping = CellMap(
        physical: physical,
        names: CellMap.defaults().names,
      );
      expect(
        cellPixelPreview(diagnostic(), mapping, 15)!.colors,
        List.filled(6, 0x00ff00),
      );
      expect(
        cellPixelPreview(diagnostic(), mapping, 2)!.colors,
        List.filled(6, 0),
      );
      expect(cellPixelPreview(diagnostic(count: 1), mapping, 15)!.colors, [
        0x00ff00,
        0,
        0,
        0,
        0,
        0,
      ]);
    },
  );

  test('Published frame uses its own map and animated mode never invents white RGB', () {
    final physical = List.generate(80, (i) => i + 1);
    physical[0] = 2;
    physical[1] = 1;
    final frame = DeviceStatus({
      ...diagnostic().json,
      'effect': 'rainbow',
      'led_output': {},
      'pixel_frame': {
        'revision': 1,
        'pending': false,
        'physical': physical,
        'rgb': '000000' * 6 + '00ff00' * 6 + '000000' * 468,
      },
    });
    expect(
      cellPixelPreview(frame, CellMap.defaults(), 1)!.colors,
      List.filled(6, 0x00ff00),
    );
    final noFrame = DeviceStatus({...frame.json}..remove('pixel_frame'));
    expect(cellPixelPreview(noFrame, CellMap.defaults(), 1), isNull);
  });

  testWidgets(
    'Cell shows all six actual diagnostic colors in fixed indicator slots',
    (tester) async {
      final sample = cellPixelPreview(diagnostic(), CellMap.defaults(), 2)!;
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 80,
              height: 80,
              child: CellButton(
                id: 2,
                name: '2',
                physical: 2,
                state: 'white',
                onPressed: () {},
                onLongPress: null,
                showIndicator: true,
                pixels: sample,
              ),
            ),
          ),
        ),
      );
      for (var pixel = 0; pixel < 6; ++pixel) {
        final slot = tester.widget<Container>(
          find.byKey(ValueKey('cell-2-pixel-$pixel')),
        );
        expect(
          (slot.decoration as BoxDecoration).color,
          const Color(0xff00ff00),
        );
      }
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'Recovery footer retains selected hostname and exposes actual reason',
    (tester) async {
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: StatusBar(
              status: null,
              retained: peerFixture(),
              endpoint: 'http://ledboard.local',
              message: 'Checking the board connection…',
            ),
          ),
        ),
      );
      expect(find.text('State unavailable'), findsNothing);
      expect(find.text('ledboard · awaiting fresh state'), findsOneWidget);
      expect(find.text('Checking the board connection…'), findsOneWidget);
    },
  );
}
