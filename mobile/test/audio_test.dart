import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';

void main() {
  test('Only known actual AVR-advertised cues are available', () {
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
        'avr_online': true,
        'audio_cues': ['welcome-40led', 'bad', 7, 'welcome-40led'],
      }).audioCues,
      ['welcome-40led'],
    );
    expect(
      DeviceStatus({'avr_online': true, 'audio_cues': audioCueNames}).audioCues,
      audioCueNames,
    );
  });
}
