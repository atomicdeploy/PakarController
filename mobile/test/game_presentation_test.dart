import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/game_settings_command.dart';

void main() {
  test('presentation is typed and bounds reject before dispatch', () {
    expect(
      gameSettingsPatch({
        'presentation': jsonEncode({'flashes': 0, 'fade_ms': 10000}),
      }),
      {
        'presentation': {'flashes': 0, 'fade_ms': 10000},
      },
    );
    for (final value in [-1, 11, 2.5, '2']) {
      expect(
        () => presentationPatch({'flashes': value}),
        throwsFormatException,
      );
    }
    expect(
      () => presentationPatch({
        'groups': [
          {'id': 1},
          {'id': 1},
        ],
      }),
      throwsFormatException,
    );
    expect(
      () => presentationPatch({
        'groups': [
          {'id': 19},
        ],
      }),
      throwsFormatException,
    );
    expect(
      () => presentationPatch({
        'groups': [
          {'id': 2, 'win_motion': 4},
        ],
      }),
      throwsFormatException,
    );
  });
  test(
    'partial presentation readback matches by group identity, not position',
    () {
      final actual = {
        'flashes': 2,
        'groups': [
          {'id': 2, 'win_rgb': 123, 'lose_motion': 3},
          {'id': 1, 'win_rgb': 456},
        ],
      };
      expect(
        presentationMatches(actual, {
          'groups': [
            {'id': 1, 'win_rgb': 456},
          ],
        }),
        isTrue,
      );
      expect(
        presentationMatches(actual, {
          'groups': [
            {'id': 2, 'win_rgb': 456},
          ],
        }),
        isFalse,
      );
      expect(presentationMatches(actual, {'flashes': 3}), isFalse);
    },
  );
}
