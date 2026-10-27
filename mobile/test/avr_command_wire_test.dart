import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/command_readback.dart';
import 'package:pakarcontroller/command_wire.dart';
import 'package:pakarcontroller/command_failure.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

void main() {
  for (final command in [
    ('/api/audio', {'cue': 'welcome'}, 'audio'),
    ('/api/buzzer', {'frequency': '880', 'duration_ms': '70'}, 'buzzer'),
    ('/api/outputs', {'mask': '3'}, 'outputs'),
  ]) {
    test('AVR ${command.$3} sends typed values and requires actual ACK', () {
      final status = DeviceStatus({
        ...peerFixture(
          features: ['buzzer', 'outputs', 'command-ack', 'avr-command-ack'],
        ).json,
        'board_id': 'avr-fixture',
        'boot_id': 'a' * 32,
        'command_clock_ms': 1000,
        'programming_latch': false,
        'active_outputs': 3,
        'buzzer': {'active': false},
      });
      final packet = commandPacket(status, command.$1, command.$2);
      final request = jsonDecode(packet) as Map;
      expect(request['command'], command.$3);
      if (command.$3 == 'buzzer') {
        expect(request['frequency'], 880);
        expect(request['duration_ms'], 70);
      }
      if (command.$3 == 'outputs') expect(request['mask'], 3);
      final reply = {
        'type': 'command-result',
        'request_id': request['request_id'],
        'board_id': 'avr-fixture',
        'boot_id': 'a' * 32,
        'accepted': true,
        'state': 'acknowledged',
        'physical_verified': false,
        'status': status.json,
      };
      expect(
        () => verifyCommandReply(reply, packet, command.$1, command.$2),
        throwsFormatException,
      );
      final actual = verifyCommandReply(
        {...reply, 'avr_acknowledged': true},
        packet,
        command.$1,
        command.$2,
      );
      expect(
        commandReadbackMatches(actual, command.$1, command.$2),
        true,
        reason: 'a completed short tone does not need to still be playing',
      );
    });
  }
  test('correction refusal explains known reason and never echoes arbitrary server text', () {
    expect(
      commandFailureMessage(error: 'corrections_preview_owned'),
      contains('Another editor'),
    );
    expect(
      commandFailureMessage(error: 'secret raw server text'),
      isNot(contains('secret')),
    );
    expect(
      commandFailureMessage(error: 'avr_outcome_unknown'),
      contains('did not acknowledge'),
    );
  });
}
