import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

void main() {
  test('Only valid actual AVR-advertised cues are available, including future names', () {
    expect(DeviceStatus({}).audioCues, isEmpty);
    expect(
      DeviceStatus({'avr_online': true, 'audio_cues': 'bad'}).audioCues,
      isEmpty,
    );
    expect(
      DeviceStatus({'avr_online': false, 'audio_cues': audioCueNames})
          .audioCues,
      isEmpty,
    );
    expect(
      DeviceStatus({
        ...peerFixture(features: ['buzzer']).json,
        'avr_online': true,
        'audio_cues': ['welcome-40led', 'future-cue', 'bad\ncue', '', 7, 'welcome-40led'],
      }).audioCues,
      ['welcome-40led', 'future-cue'],
    );
    expect(
      DeviceStatus({
        ...peerFixture(features: ['buzzer']).json,
        'avr_online': true,
        'audio_cues': audioCueNames,
      }).audioCues,
      audioCueNames,
    );
  });
}
