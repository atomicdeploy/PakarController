import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/peer_context.dart';

void main() {
  test('LCD preview requires its own complete initialization/frame ACK', () {
    final lcd = <String, dynamic>{
      'configured': true,
      'started': true,
      'detection_known': true,
      'backpack_ack': true,
      'initialized': true,
      'available': true,
      'columns': 16,
      'rows': 2,
      'address': 0x27,
      'lines': ['ledboard        ', 'Cell 1 white    '],
    };
    final status = <String, dynamic>{
      'hostname': 'ledboard',
      'lcd': lcd,
      'capabilities': {
        'board_kind': 'controller',
        'hostname': 'ledboard',
        'acknowledged_features': ['lcd'],
      },
    };
    expect(DeviceStatus(status).lcdLines, hasLength(2));
    expect(DeviceStatus(status).peer.frontPanel, isTrue);
    for (final key in [
      'configured',
      'started',
      'detection_known',
      'backpack_ack',
      'initialized',
      'available',
    ]) {
      final invalid = {
        ...status,
        'lcd': {...lcd, key: false},
      };
      expect(lcdObservationReady(invalid), isFalse);
      expect(DeviceStatus(invalid).peer.has('lcd'), isFalse);
      expect(DeviceStatus(invalid).lcdLines, isEmpty);
    }
    expect(
      DeviceStatus({
        ...status,
        'lcd': {
          ...lcd,
          'lines': ['partial'],
        },
      }).lcdLines,
      isEmpty,
    );
    expect(
      DeviceStatus({
        ...status,
        'lcd': {...lcd, 'address': 0x40},
      }).lcdLines,
      isEmpty,
    );
    expect(DeviceStatus({...status, 'lcd': null}).lcdLines, isEmpty);
  });
}
