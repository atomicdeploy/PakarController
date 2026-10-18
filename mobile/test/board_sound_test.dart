import 'dart:convert';
import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/board_sound.dart';
import 'package:pakarcontroller/board_sound_preferences.dart';
import 'package:pakarcontroller/models.dart';

class SoundFixture extends PakarApi {
  bool acknowledged = true;
  bool muted = false;
  int reads = 0;
  Completer<void>? hold;
  @override
  Future<DeviceStatus> status() async {
    reads++;
    return DeviceStatus({
      'hostname': 'ledboard',
      'avr_online': true,
      'capabilities': {
        'hostname': 'ledboard',
        'board_kind': 'controller',
        'acknowledged_features': acknowledged ? ['settings', 'buzzer'] : [],
      },
    });
  }

  @override
  Future<BoardSound> boardSound({bool? silent}) async {
    if (silent != null) {
      await hold?.future;
      muted = silent;
    }
    return BoardSound.parse(
      BoardSound.encode([
        0x50,
        muted ? 3 : 2,
        5,
        0,
        0,
        0,
        2,
        0,
        63,
        0x10,
        0x32,
        0x54,
        0,
        ...List.filled(8, 0),
        1,
      ]),
    );
  }
}

void main() {
  testWidgets(
    'A backgrounded preferences timer cannot read before the next UI frame',
    (tester) async {
      final api = SoundFixture();
      addTearDown(api.close);
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(body: BoardSoundPreferences(api: api)),
        ),
      );
      await tester.pumpAndSettle();
      final reads = api.reads;
      api.appForeground = false; // Deliberately no didUpdateWidget/rebuild yet.
      await tester.pump(const Duration(seconds: 16));
      expect(api.reads, reads);
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'Board Preferences stay contextual and do not optimistically switch before readback',
    (tester) async {
      final api = SoundFixture();
      addTearDown(api.close);
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(body: BoardSoundPreferences(api: api)),
        ),
      );
      await tester.pumpAndSettle();
      expect(find.text('Silent mode'), findsOneWidget);
      expect(
        tester.widget<SwitchListTile>(find.byType(SwitchListTile)).value,
        false,
      );
      api.hold = Completer<void>();
      await tester.tap(find.byType(SwitchListTile));
      await tester.pump();
      expect(
        tester.widget<SwitchListTile>(find.byType(SwitchListTile)).value,
        false,
      );
      expect(
        tester.widget<SwitchListTile>(find.byType(SwitchListTile)).onChanged,
        isNull,
      );
      api.hold!.complete();
      await tester.pumpAndSettle();
      expect(
        tester.widget<SwitchListTile>(find.byType(SwitchListTile)).value,
        true,
      );
      api.disconnect();
      await tester.pump();
      // Force a parent rebuild, as the real workspace does on Disconnect.
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: BoardSoundPreferences(
              key: const ValueKey('disconnected'),
              api: api,
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(find.byType(SwitchListTile), findsNothing);
      await tester.pumpWidget(const SizedBox.shrink());
      final unavailable = SoundFixture()..acknowledged = false;
      addTearDown(unavailable.close);
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(body: BoardSoundPreferences(api: unavailable)),
        ),
      );
      await tester.pumpAndSettle();
      expect(find.text('Board sound'), findsNothing);
      await tester.pumpWidget(const SizedBox.shrink());
    },
  );
  String raw(int flags, [int persisted = 1]) => BoardSound.encode([
    0x50,
    flags,
    5,
    0,
    0,
    0,
    2,
    0,
    63,
    0x10,
    0x32,
    0x54,
    0,
    ...List.filled(8, 0),
    persisted,
  ]);
  test(
    'Silent preserves other board bytes and rejects malformed/unknown records',
    () {
      final value = BoardSound.parse(raw(6));
      expect(value.silent, false);
      expect(value.persisted, true);
      final next = value.withSilent(true);
      expect(next.substring(4), raw(6).substring(4, 42));
      expect(BoardSound.parse('${next}01').silent, true);
      for (final bad in ['', raw(8), raw(2, 2), 'ff' * 22]) {
        expect(() => BoardSound.parse(bad), throwsFormatException);
      }
    },
  );
  for (final failure in [false, true]) {
    test(
      'Silent API uses actual AVR ACK/readback; mismatch=$failure',
      () async {
        var settings = raw(2), writes = 0, id = 0;
        Map<String, dynamic>? receipt;
        final client = MockClient((request) async {
          final path = request.url.path;
          Map<String, dynamic> json;
          if (path == '/api/status') {
            json = {
              'hostname': 'ledboard',
              'board_id': 'esp32s3-fixture',
              'boot_id': 'a' * 32,
              'http_peer_fencing': true,
              'avr_online': true,
              'capabilities': {
                'hostname': 'ledboard',
                'board_kind': 'controller',
                'acknowledged_features': ['settings', 'buzzer'],
              },
            };
          } else if (request.method == 'POST') {
            expect(request.headers['X-Pakar-Board'], 'esp32s3-fixture');
            final opcode = int.parse(request.bodyFields['opcode']!);
            id++;
            if (opcode == 5) {
              writes++;
              expect(
                request.bodyFields['payload_hex'],
                raw(3).substring(0, 42),
              );
              settings = raw(3, failure ? 0 : 1);
            }
            receipt = {
              'request_id': id,
              'boot_id': 'a' * 32,
              'request_opcode': opcode,
              'state': 'responded',
              'response_opcode': opcode == 4 ? 0x91 : 0x80,
              'payload_hex': opcode == 4 ? settings : '0500',
            };
            json = {'request_id': id, 'boot_id': 'a' * 32};
          } else if (request.method == 'DELETE') {
            expect(request.url.queryParameters['id'], '$id');
            json = {'consumed': true};
          } else {
            expect(request.url.queryParameters['id'], '$id');
            json = receipt!;
          }
          return http.Response(
            jsonEncode(json),
            200,
            headers: {'content-type': 'application/json'},
          );
        });
        final api = PakarApi(client: client);
        if (failure) {
          await expectLater(
            api.boardSound(silent: true),
            throwsFormatException,
          );
        } else {
          expect((await api.boardSound(silent: true)).silent, true);
          await api.boardSound(silent: true);
        }
        expect(
          writes,
          1,
          reason: 'Do not replay a write or rewrite identical EEPROM settings',
        );
        await api.close();
      },
    );
  }
}
