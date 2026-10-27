import 'dart:convert';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';
import 'package:pakarcontroller/usb_transport.dart';

import 'pixel_groups_test.dart' show groupJson;
import 'pixel_diagnostic_usb_api_test.dart' show DiagnosticUsbApiFixture;
import 'usb_transport_test.dart' show UsbFixture;

class GroupsUsbFixture extends UsbFixture {
  GroupsUsbFixture() {
    status['pixel_groups'] = groupJson(overrides: {1: 5, 2: 7}, revision: 2);
    (status['capabilities']['acknowledged_features'] as List).addAll(<String>[
      'usb-api',
      'pixel-group-layout',
    ]);
  }
  final request = <int>[];
  List<int> response = [];
  int mutations = 0;
  bool foreign = false;
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
      if (envelope['path'] == '/api/pixels/groups') {
        if (envelope['method'] == 'POST') {
          final config = envelope['body'] as Map;
          expect(config['revision'], 2);
          mutations++;
          status['pixel_groups'] = groupJson(
            revision: 3,
            overrides: {
              for (final row in config['overrides'] as List)
                row['group'] as int: row['count'] as int,
            },
          );
        }
        response = utf8.encode(jsonEncode(status['pixel_groups']));
      } else {
        expect(envelope['path'], '/api/pixels/frame');
        expect(envelope['method'], 'GET');
        final layout = status['pixel_groups'] as Map;
        expect(
          (envelope['body'] as Map)['group_layout_revision'],
          layout['revision'],
        );
        response = utf8.encode(
          jsonEncode({
            'board_id': status['board_id'],
            'boot_id': foreign ? 'b' * 32 : status['boot_id'],
            'pixel_frame': {
              'revision': 2,
              'pending': false,
              'rgb': '00ff00' * 480,
              'physical': List.generate(80, (i) => i + 1),
              'group_layout_revision': layout['revision'],
              'group_counts': layout['counts'],
              'group_starts': layout['starts'],
            },
          }),
        );
      }
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
  test('Custom group diagnostics use fenced generic USB command with numeric layout revision, never legacy opcode', () async {
    final fixture = DiagnosticUsbApiFixture();
    fixture.status['pixel_groups'] = groupJson(
      overrides: {1: 5, 2: 7},
      revision: 2,
    );
    (fixture.status['capabilities']['acknowledged_features'] as List).add(
      'pixel-group-layout',
    );
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(fixture.channel, fixture.invoke);
    try {
      await fixture.transport.connect('usb://12/0');
      final probe = PixelProbe.forPhysicalGroup(
        DeviceStatus(fixture.status),
        physicalGroup: 2,
      ).copyWith(effect: 'breathe', overlay: true);
      final observed = await fixture.transport.command(
        '/api/led/diagnostic',
        probe.commandFields(fixture.transport.latest!),
        authorize: () => true,
      );
      expect(matchesDiagnosticObservation(observed, probe), true);
      expect(fixture.received!['group_layout_revision'], 2);
      expect(fixture.received!['index'], 5);
      expect(fixture.received!['count'], 7);
      expect(fixture.executes, 1);
      expect(fixture.writes.where((f) => f.opcode == 0x5e), isEmpty);
    } finally {
      await fixture.close();
    }
  });
  test('USB group API reads and saves one revisioned configuration without network fallback', () async {
    final fixture = GroupsUsbFixture();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(fixture.channel, fixture.invoke);
    try {
      await fixture.transport.connect('usb://12/0');
      final before = await fixture.transport.pixelGroupsApi(
        authorize: () => true,
      );
      expect(before['revision'], 2);
      expect(fixture.mutations, 0);
      final saved = await fixture.transport.pixelGroupsApi(
        configuration: {
          'revision': 2,
          'overrides': [
            {'group': 1, 'count': 5},
          ],
        },
        authorize: () => true,
      );
      expect(saved['revision'], 3);
      expect(saved['used_pixels'], 479);
      expect(fixture.mutations, 1);
    } finally {
      await fixture.close();
    }
  });
  for (final foreign in [false, true]) {
    test(
      'Compact USB RGB fetch is on demand and peer fenced foreign=$foreign',
      () async {
        final fixture = GroupsUsbFixture()
          ..foreign = foreign
          ..pixelFramePartial = true;
        final layout = fixture.status['pixel_groups'] as Map;
        fixture.pixels = {
          'revision': 2,
          'pending': false,
          'rgb_omitted': true,
          'group_layout_revision': 2,
          'group_counts': layout['counts'],
          'group_starts': layout['starts'],
        };
        TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
            .setMockMethodCallHandler(fixture.channel, fixture.invoke);
        try {
          await fixture.transport.connect('usb://12/0');
          expect(fixture.writes.where((f) => f.opcode == 0x6a), isEmpty);
          fixture.transport.demand(8, foreground: true);
          await Future<void>.delayed(const Duration(milliseconds: 10));
          if (foreign) {
            await expectLater(
              fixture.transport.status(),
              throwsFormatException,
            );
          } else {
            final actual = await fixture.transport.status();
            expect(actual.json['pixel_frame']['rgb'], '00ff00' * 480);
            expect(actual.frameGroups(actual.json['pixel_frame'])!.start(2), 5);
          }
          expect(fixture.mutations, 0);
          expect(fixture.writes.where((f) => f.opcode == 0x5e), isEmpty);
        } finally {
          await fixture.close();
        }
      },
    );
  }
}
