import 'dart:convert';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/usb_transport.dart';

import 'usb_transport_test.dart' show UsbFixture;

class CorrectionUsbFixture extends UsbFixture {
  CorrectionUsbFixture() {
    (status['capabilities']['acknowledged_features'] as List).addAll(<String>[
      'usb-api',
      'pixel-corrections',
      'pixel-corrections-preview',
    ]);
  }
  final request = <int>[];
  List<int> response = [];
  final paths = <String>[];
  final bodies = <Map>[];
  bool reject = false;
  @override
  Future<dynamic> invoke(MethodCall call) async {
    if (call.method != 'write') return super.invoke(call);
    final bytes = call.arguments['bytes'] as Uint8List;
    final frame = UsbFrame.decode(bytes.sublist(0, bytes.length - 1))!;
    if (frame.opcode != 0x6a) return super.invoke(call);
    writes.add(frame);
    final p = frame.payload, action = p.first;
    var offset = 0;
    if (action == 0) {
      request.clear();
      response = [];
    }
    if (action == 1) request.addAll(p.skip(7));
    if (action == 2) {
      final envelope = jsonDecode(utf8.decode(request)) as Map;
      expect(envelope['board_id'], status['board_id']);
      expect(envelope['boot_id'], status['boot_id']);
      paths.add(envelope['path'] as String);
      bodies.add(envelope['body'] as Map);
      response = utf8.encode(
        jsonEncode(
          reject
              ? {'accepted': false, 'error': 'revision_conflict'}
              : {
                  'revision': 3,
                  'pixels': 480,
                  'enabled': true,
                  'rules': bodies.last['rules'] ?? [],
                  'preview': {
                    'active': paths.last.endsWith('/preview'),
                    'lease_id': 41,
                  },
                },
        ),
      );
    }
    if (action == 3) offset = UsbFixture.read(p, 5, 2);
    if (action == 4) {
      reply(frame, 0x80, [0x6a, 0]);
      return bytes.length;
    }
    reply(frame, 0xbd, [
      action,
      ...UsbFixture.le(73, 4),
      ...UsbFixture.le(response.length, 2),
      ...UsbFixture.le(offset, 2),
      if (action == 3) ...response.skip(offset).take(39),
    ]);
    return bytes.length;
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  test('USB correction read, preview and Save retain raw stored addresses and explicit paths', () async {
    final fixture = CorrectionUsbFixture();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(fixture.channel, fixture.invoke);
    try {
      await fixture.transport.connect('usb://12/0');
      await fixture.transport.pixelCorrectionsApi(authorize: () => true);
      final config = {
        'revision': 3,
        'enabled': true,
        'rules': [
          {'kind': 'black', 'pixel': 334, 'offset': 0},
        ],
      };
      await fixture.transport.pixelCorrectionsApi(
        configuration: {...config, 'ttl_ms': 10000},
        preview: true,
        authorize: () => true,
      );
      await fixture.transport.pixelCorrectionsApi(
        configuration: config,
        authorize: () => true,
      );
      expect(fixture.paths, [
        '/api/pixels/corrections',
        '/api/pixels/corrections/preview',
        '/api/pixels/corrections',
      ]);
      expect((fixture.bodies[1]['rules'] as List).single['pixel'], 334);
      expect(fixture.bodies[2].containsKey('ttl_ms'), false);
    } finally {
      await fixture.close();
    }
  });
  test(
    'USB correction rejection and retired authorization never replay',
    () async {
      final fixture = CorrectionUsbFixture()..reject = true;
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(fixture.channel, fixture.invoke);
      try {
        await fixture.transport.connect('usb://12/0');
        await expectLater(
          fixture.transport.pixelCorrectionsApi(
            configuration: {'revision': 3, 'enabled': true, 'rules': []},
            preview: true,
            authorize: () => true,
          ),
          throwsFormatException,
        );
        expect(fixture.paths, hasLength(1));
        await expectLater(
          fixture.transport.pixelCorrectionsApi(authorize: () => false),
          throwsFormatException,
        );
        expect(fixture.paths, hasLength(1));
      } finally {
        await fixture.close();
      }
    },
  );
}
