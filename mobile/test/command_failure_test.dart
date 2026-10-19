import 'dart:async';
import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/command_failure.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

import 'peer_fixture.dart';

Map<String, dynamic> snapshot() => {
  ...peerFixture(features: ['command-ack', 'pixel-diagnostic', 'leds', 'table'])
      .json,
  'board_id': 'fixture-board',
  'boot_id': 'a' * 32,
  'command_clock_ms': 1000,
  'pixels': 480,
  'effect': 'none',
  'led_output': {'enabled': true, 'mode': 'effects'},
};

void main() {
  for (final jsonMethod in [false, true]) {
    for (final status in [200, 202, 409, 503]) {
      test(
        'REST ${jsonMethod ? 'postJson' : 'post'} HTTP $status rejects a negative receipt exactly once',
        () async {
          var writes = 0;
          final api = PakarApi(
            client: MockClient((request) async {
              if (request.method == 'GET') {
                return http.Response(jsonEncode(snapshot()), 200);
              }
              writes++;
              return http.Response(
                jsonEncode({
                  'accepted': false,
                  'error': 'capability_not_acknowledged',
                }),
                status,
              );
            }),
          );
          try {
            final action = jsonMethod
                ? api.postJson('/api/audio', {'cue': 'welcome'})
                : api.post('/api/audio', {'cue': 'welcome'});
            await expectLater(
              action,
              throwsA(
                isA<DeviceCommandRejected>()
                    .having(
                      (e) => e.message,
                      'reason',
                      contains('not acknowledged'),
                    )
                    .having(
                      (e) => e.message,
                      'HTTP status',
                      contains('HTTP $status'),
                    ),
              ),
            );
            expect(writes, 1);
          } finally {
            await api.close();
          }
        },
      );
    }
  }
  test(
    'Write timeout remains uncertain, detailed and is not replayed',
    () async {
      var writes = 0;
      final api = PakarApi(
        client: MockClient((request) async {
          if (request.method == 'GET') {
            return http.Response(jsonEncode(snapshot()), 200);
          }
          writes++;
          throw TimeoutException('fixture lost receipt');
        }),
      );
      try {
        await expectLater(
          api.post('/api/led/diagnostic'),
          throwsA(
            isA<OperationUncertain>().having(
              (e) => e.toString(),
              'detail',
              contains('write receipt'),
            ),
          ),
        );
        expect(writes, 1);
      } finally {
        await api.close();
      }
    },
  );
  test(
    'Error formatting does not echo arbitrary server text or credentials',
    () {
      final error = DeviceCommandRejected(
        error: 'secret=never-show-this',
        httpStatus: 409,
      );
      expect(error.message, contains('HTTP 409'));
      expect(error.message, isNot(contains('secret')));
    },
  );
  for (final issue in [
    'http-rejected',
    'command-rejected',
    'malformed',
    'missing-frame',
    'pending-frame',
    'wrong-frame',
  ]) {
    test(
      'Atomic pixel $issue propagates a specific failure and never completes or replays',
      () async {
        var writes = 0;
        const probe = PixelProbe(index: 479, rgb: 0x1234ab);
        final api = PakarApi(
          client: MockClient((request) async {
            if (request.method == 'GET') {
              return http.Response(jsonEncode(snapshot()), 200);
            }
            writes++;
            expect(request.url.path, '/api/command');
            if (issue == 'http-rejected') {
              return http.Response('{"error":"pixel_diagnostic_busy"}', 409);
            }
            if (issue == 'malformed') {
              return http.Response('not a command receipt', 200);
            }
            final packet = jsonDecode(request.bodyFields['packet']!) as Map;
            return http.Response(
              jsonEncode({
                'type': 'command-result',
                'request_id': packet['request_id'],
                'board_id': packet['board_id'],
                'boot_id': packet['boot_id'],
                'accepted': issue != 'command-rejected',
                'error': 'pixel_diagnostic_busy',
                'status': {
                  ...snapshot(),
                  if (issue != 'missing-frame')
                    'pixel_frame': {
                      'revision': 1,
                      'pending': issue == 'pending-frame',
                      'rgb':
                          (issue == 'wrong-frame'
                                  ? List<int>.filled(1440, 0)
                                  : probe.frame())
                              .map((v) => v.toRadixString(16).padLeft(2, '0'))
                              .join(),
                    },
                },
              }),
              200,
            );
          }),
        );
        try {
          await api.status();
          final expected = switch (issue) {
            'http-rejected' || 'command-rejected' => 'LED driver is busy',
            'malformed' => 'malformed command receipt',
            'missing-frame' => 'returned no pixel frame',
            'pending-frame' => 'still pending',
            _ => 'differs from the requested frame',
          };
          await expectLater(
            api.pixelDiagnostic(probe, authorize: () => true),
            throwsA(
              predicate<Object>(
                (error) => error.toString().contains(expected),
                expected,
              ),
            ),
          );
          expect(writes, 1);
        } finally {
          await api.close();
        }
      },
    );
  }
}
