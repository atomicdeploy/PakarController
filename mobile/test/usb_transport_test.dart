import 'dart:async';
import 'dart:convert';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/testing.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/usb_transport.dart';
import 'package:pakarcontroller/lan_resolver.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

class UsbFixture {
  UsbFixture() {
    transport.events.stream.listen((_) {}, onError: (Object _) {});
  }
  final channel = const MethodChannel('test/pakar-usb');
  final stream = StreamController<dynamic>.broadcast(sync: true);
  late final transport = PakarUsbTransport(
    channel: channel,
    nativeEvents: stream.stream,
  );
  final writes = <UsbFrame>[];
  var session = 7, demand = 0, lease = 0, token = 1;
  bool partial = false,
      wrongAck = false,
      denied = false,
      corruptChunk = false,
      expiredLease = false;
  Uint8List capture = Uint8List(0);
  Map<String, dynamic>? pixels;
  final status = <String, dynamic>{
    'version': '0.2.8',
    'compatibility': {
      'enforced': true,
      'minimum_app': '0.2.8',
      'minimum_board': '0.2.8',
    },
    'hostname': 'ledboard',
    'board_id': 'esp32s3-fixture',
    'boot_id': List.filled(32, 'a').join(),
    'pixels': 480,
    'mode': 'cell',
    'brightness': 255,
    'effect': 'none',
    'red_lock': false,
    'http_peer_fencing': true,
    'cells': List.filled(80, 'white'),
    'avr_online': false,
    'led_output': {'enabled': true, 'mode': 'effects'},
    'capabilities': {
      'board_kind': 'table',
      'role': 'table-selection',
      'name': 'Table',
      'hostname': 'ledboard',
      'table': {'columns': 10, 'rows': 8, 'pixels_per_cell': 6},
      'modes': [
        {'id': 'cell', 'code': 1, 'kind': 'presentation', 'label': 'Table'},
      ],
      'acknowledged_features': [
        'table',
        'leds',
        'cell-batch',
        'cell-assignments',
        'cell-lock',
        'command-ack',
        'pixel-diagnostic',
        'usb-demand',
        'usb-pixel-diagnostic',
      ],
    },
  };
  static List<int> le(int n, int count) =>
      List.generate(count, (i) => n >> (i * 8) & 255);
  static int read(List<int> p, int at, int count) => List.generate(
    count,
    (i) => p[at + i] << (i * 8),
  ).fold(0, (a, b) => a | b);
  void reply(UsbFrame request, int op, List<int> p) {
    scheduleMicrotask(
      () => stream.add({
        'session': session,
        'bytes': UsbFrame(op, request.sequence, p).encode(),
      }),
    );
  }

  Future<dynamic> invoke(MethodCall call) async {
    if (call.method == 'devices') {
      return [
        {
          'device_id': 12,
          'interface': 0,
          'name': 'PakarController',
          'permission': true,
        },
      ];
    }
    if (call.method == 'connect') {
      if (denied) {
        throw PlatformException(code: 'usb_permission', message: 'Denied');
      }
      return {'session': session};
    }
    if (call.method == 'disconnect') return null;
    if (call.method != 'write') throw PlatformException(code: 'unexpected');
    final bytes = (call.arguments['bytes'] as Uint8List);
    final f = UsbFrame.decode(bytes.sublist(0, bytes.length - 1))!;
    writes.add(f);
    if (partial) return bytes.length - 1;
    switch (f.opcode) {
      case 1:
        reply(f, 0x81, [0, 2, 0, 0, 0, 0, 0, 2, 8]);
      case 0x61:
        reply(f, 0x80, [0x61, 0]);
      case 0x64:
        demand = read(f.payload, 0, 2);
        lease = read(f.payload, 2, 2);
        reply(f, 0xba, [...le(demand, 2), ...le(expiredLease ? 0 : lease, 2)]);
      case 0x5d:
        final offset = read(f.payload, 1, 2);
        if (offset == 0) {
          token++;
          capture = Uint8List.fromList(
            utf8.encode(
              jsonEncode({
                'status': status,
                'physical': List.generate(80, (i) => i + 1),
                'names': List.generate(80, (i) => '${i + 1}'),
                'pixels': demand & 8 != 0 ? pixels : null,
              }),
            ),
          );
        }
        reply(f, 0xb6, [
          f.payload[0],
          ...le(token, 4),
          ...le(capture.length, 2),
          ...le(corruptChunk ? offset + 1 : offset, 2),
          ...capture.sublist(offset, (offset + 39).clamp(0, capture.length)),
        ]);
      case 0x5e:
        final op = f.payload[16], p = f.payload.sublist(17);
        if (op == 0x5f) {
          for (var i = 0; i < 80; i++) {
            if (p[2 + i ~/ 8] & (1 << (i % 8)) != 0) {
              (status['cells'] as List)[i] = ['white', 'red', 'off'][p[0]];
            }
          }
        } else if (op == 0x65) {
          final probe = PixelProbe(
            index: read(p, 0, 2),
            group: p[2] == 6,
            rgb: p[3] << 16 | p[4] << 8 | p[5],
          );
          pixels = {
            'revision': 1,
            'pending': false,
            'rgb': probe
                .frame(off: p[6] == 1)
                .map((b) => b.toRadixString(16).padLeft(2, '0'))
                .join(),
            'physical': List.generate(80, (i) => i + 1),
          };
        }
        reply(f, 0x80, [wrongAck ? 0x51 : op, 0]);
      default:
        throw StateError('Unexpected opcode ${f.opcode}');
    }
    return bytes.length;
  }

  Future<void> close() async {
    await transport.close();
    await stream.close();
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  setUp(() => SharedPreferences.setMockInitialValues({}));
  test('USB COBS CRC matches fixed protocol and rejects corrupt/reserved/oversized frames', () {
    for (final payload in [
      <int>[],
      [0, 1, 0, 255],
      List.generate(48, (i) => i),
    ]) {
      final packet = UsbFrame(0x5e, 42, payload).encode();
      final frame = UsbFrame.decode(packet.sublist(0, packet.length - 1));
      expect(frame!.payload, payload);
      expect(frame.sequence, 42);
      expect(frame.opcode, 0x5e);
      packet[packet.length - 2] ^= 1;
      expect(UsbFrame.decode(packet.sublist(0, packet.length - 1)), isNull);
    }
    expect(UsbFrame.crc(ascii.encode('123456789')), 0xf4);
    expect(
      () => UsbFrame(1, 1, List.filled(49, 0)).encode(),
      throwsFormatException,
    );
  });
  test('Real channel session performs HELLO introduction snapshot and atomic batch without HTTP or DNS', () async {
    final fixture = UsbFixture();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(fixture.channel, fixture.invoke);
    var http = 0, dns = 0;
    final api = PakarApi(
      baseUrl: 'usb://12/0',
      usb: fixture.transport,
      client: MockClient((_) async {
        http++;
        throw StateError('Network must not be used');
      }),
      resolver: LanResolver(
        lookup: (_) async {
          dns++;
          throw StateError('DNS must not be used');
        },
      ),
    );
    try {
      final status = await api.status();
      expect(status.peer.tableSelection, isTrue);
      expect(status.json['compatibility'], isA<Map>());
      expect(api.connectionMethod, ConnectionMethod.usb);
      // A real board includes compatibility metadata; status publication must
      // not start the optional network introduction in an offline USB session.
      await Future<void>.delayed(Duration.zero);
      expect(http, 0);
      expect(dns, 0);
      final actual = await api.fastCommand('/api/cells', {
        'ids': '1,80',
        'state': 'red',
        'effect': 'immediate',
      }, authorize: () => true);
      expect(actual!.cells[0], 'red');
      expect(actual.cells[79], 'red');
      expect(actual.cells[1], 'white');
      expect(fixture.writes.where((f) => f.opcode == 0x5e), hasLength(1));
      expect((await api.mapping()).valid, isTrue);
      await expectLater(api.getJson('/api/wifi/scan'), throwsFormatException);
      await expectLater(
        api.maintenanceBytes('/api/isp/probe', authorize: () => true),
        throwsFormatException,
      );
      expect(http, 0);
      expect(dns, 0);
    } finally {
      await api.close();
      await fixture.stream.close();
    }
  });
  test('Pixel diagnostic requests a renewable RGB lease and verifies all480 published pixels', () async {
    final fixture = UsbFixture();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(fixture.channel, fixture.invoke);
    try {
      await fixture.transport.connect('usb://12/0');
      final actual = await fixture.transport.command('/api/led/diagnostic', {
        'index': '474',
        'count': '6',
        'rgb': '65280',
        'off': 'false',
      }, authorize: () => true);
      expect(
        matchesDiagnosticFrame(
          actual,
          const PixelProbe(index: 474, group: true, rgb: 65280).frame(),
        ),
        isTrue,
      );
      expect(fixture.demand & 8, 8);
      fixture.transport.demand(1, foreground: false);
      await Future<void>.delayed(const Duration(milliseconds: 20));
      expect(fixture.demand, 0);
      expect(fixture.lease, 0);
      expect(fixture.writes.where((f) => f.opcode == 0x5e), hasLength(1));
    } finally {
      await fixture.close();
    }
  });
  test(
    'Partial write, wrong ACK and malformed capture never replay an intent',
    () async {
      final fixture = UsbFixture();
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(fixture.channel, fixture.invoke);
      try {
        await fixture.transport.connect('usb://12/0');
        fixture.wrongAck = true;
        await expectLater(
          fixture.transport.command('/api/cells', {
            'ids': '1',
            'state': 'red',
          }, authorize: () => true),
          throwsFormatException,
        );
        expect(fixture.writes.where((f) => f.opcode == 0x5e), hasLength(1));
        fixture.corruptChunk = true;
        await expectLater(fixture.transport.status(), throwsFormatException);
        fixture.corruptChunk = false;
        fixture.partial = true;
        await expectLater(fixture.transport.status(), throwsFormatException);
      } finally {
        await fixture.close();
      }
    },
  );
  test('Nonzero demand requires a live lease, permission denial never sends HELLO, detach clears authority', () async {
    final fixture = UsbFixture();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(fixture.channel, fixture.invoke);
    final errors = fixture.transport.events.stream.listen(
      (_) {},
      onError: (Object _) {},
    );
    try {
      fixture.denied = true;
      await expectLater(
        fixture.transport.connect('usb://12/0'),
        throwsA(isA<PlatformException>()),
      );
      expect(fixture.writes, isEmpty);
      fixture.denied = false;
      await fixture.transport.connect('usb://12/0');
      fixture.expiredLease = true;
      fixture.transport.demand(1, foreground: true);
      await Future<void>.delayed(const Duration(milliseconds: 20));
      expect(fixture.transport.ready, isFalse);
      expect(fixture.transport.latest, isNull);
      fixture.expiredLease = false;
      await fixture.transport.connect('usb://12/0');
      fixture.stream.add({'session': fixture.session, 'type': 'disconnected'});
      expect(fixture.transport.ready, isFalse);
      expect(fixture.transport.table, isNull);
    } finally {
      await errors.cancel();
      await fixture.close();
    }
  });
  test('Semantic panel event uses its own sequence and preserves requested-only presentation', () async {
    final fixture = UsbFixture();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(fixture.channel, fixture.invoke);
    try {
      await fixture.transport.connect('usb://12/0');
      final next = fixture.transport.events.stream.firstWhere(
        (e) => e['type'] == 'panel-event',
      );
      final text = ascii.encode('Cell 80 red');
      fixture.stream.add({
        'session': fixture.session,
        'bytes': UsbFrame(0xb8, 37, [
          ...UsbFixture.le(37, 4),
          ...UsbFixture.le(1000, 4),
          1,
          0,
          2,
          ...UsbFixture.le(2000, 2),
          80,
          0,
          text.length,
          ...text,
        ]).encode(),
      });
      final event = await next.timeout(const Duration(seconds: 1));
      expect(event['event_id'], 37);
      expect(event['board_id'], 'esp32s3-fixture');
      expect(event['text'], 'Cell 80 red');
      expect(event['hardware_confirmed'], isFalse);
      expect(event['presentation']['marquee'], 'once');
    } finally {
      await fixture.close();
    }
  });
}
