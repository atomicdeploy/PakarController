import 'dart:async';
import 'dart:convert';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/ble_codec.dart';
import 'package:pakarcontroller/ble_transport.dart';
import 'package:pakarcontroller/connection_preferences.dart';
import 'package:pakarcontroller/device_registry.dart';
import 'package:pakarcontroller/discovery.dart';
import 'package:pakarcontroller/mirroring.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'connection_preferences_test.dart' show TestSecrets;
import 'peer_fixture.dart';
import 'mirroring_test.dart' show SilentOutput, mirroredPeer, toneEvent;

// Actual Dart codec/transport/API; the fixture replaces only Android GATT.
// It is not radio, Android permission, installed-phone or hardware proof.
class GattFixture {
  final channel = const MethodChannel('pakar-test-ble');
  final events = StreamController<dynamic>.broadcast(sync: true);
  final requests = <Map<String, dynamic>>[];
  final packets = <Uint8List>[];
  final assembler = BleAssembler();
  int session = 1, message = 1, offset = 0, tick = 0;
  int mtu = 23, disconnects = 0;
  bool hold = false, badOwner = false, authenticated = false;
  Uint8List? outgoing;
  String get board => 'board-fixture';
  String get boot => 'boot-fixture';
  Map<String, dynamic> get snapshot => {
    ...peerFixture(features: ['table', 'cell-batch', 'leds', 'command-ack'])
        .json,
    'board_id': board,
    'boot_id': boot,
    'command_clock_ms': 100000,
    'ble': {
      'routes': ['GET /api/status', 'GET /api/ble/clock', 'POST /api/command'],
      'push': ['status'],
    },
  };
  void install() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, invoke);
  }

  Future<dynamic> invoke(MethodCall call) async {
    final args = call.arguments as Map? ?? {};
    if (call.method == 'connect') {
      assembler.reset();
      return {
        'session': session,
        'mtu': mtu,
        'info': jsonEncode({
          'product': 'PakarController',
          'board_id': board,
          'boot_id': boot,
          'auth_required': authenticated,
        }),
      };
    }
    if (call.method == 'disconnect') {
      if (args['session'] == session) {
        disconnects++;
      }
      return null;
    }
    if (call.method != 'write') {
      throw StateError('Unexpected native call ${call.method}');
    }
    final packet = Uint8List.fromList((args['value'] as Uint8List));
    packets.add(packet);
    final frame = BleFrame.decode(packet);
    if (frame.kind == 1) {
      if (outgoing != null && frame.id == message && frame.offset == offset) {
        scheduleMicrotask(sendPiece);
      }
      return null;
    }
    final assembled = assembler.receive(frame, tick++);
    emit(assembled.ack);
    if (assembled.message != null) {
      final request =
          jsonDecode(utf8.decode(assembled.message!)) as Map<String, dynamic>;
      requests.add(request);
      if (!hold) {
        final form = request['form'] as Map;
        Map<String, dynamic> body = snapshot;
        if (request['path'] == '/api/ble/clock') {
          body = {
            'board_id': board,
            'boot_id': boot,
            'command_clock_ms': 100500,
          };
        }
        if (request['path'] == '/api/command') {
          final command = jsonDecode(form['packet'] as String) as Map;
          final actual = snapshot;
          (actual['cells'] as List)[0] = command['state'];
          body = {
            'type': 'command-result',
            'request_id': command['request_id'],
            'board_id': board,
            'boot_id': boot,
            'accepted': true,
            'count': 1,
            'status': actual,
          };
        }
        outgoing = Uint8List.fromList(
          utf8.encode(
            jsonEncode({
              'type': 'ble-response',
              'request_id': request['request_id'],
              'board_id': badOwner ? 'another-board' : board,
              'boot_id': boot,
              'status': 200,
              'body': body,
            }),
          ),
        );
        offset = 0;
        message++;
        scheduleMicrotask(sendPiece);
      }
    }
    return null;
  }

  void emit(Uint8List value, {int? owner}) {
    scheduleMicrotask(() {
      if (!events.isClosed) {
        events.add({
          'type': 'data',
          'session': owner ?? session,
          'value': value,
        });
      }
    });
  }

  void sendPiece() {
    final bytes = outgoing;
    if (bytes == null) {
      return;
    }
    if (offset == bytes.length) {
      outgoing = null;
      return;
    }
    final end = (offset + blePayloadBytes(mtu)).clamp(0, bytes.length);
    final value = BleFrame(
      0,
      message,
      offset,
      bytes.length,
      Uint8List.sublistView(bytes, offset, end),
    ).encode();
    offset = end;
    emit(value);
  }

  PakarBleTransport transport() =>
      PakarBleTransport(channel: channel, nativeEvents: events.stream);
  Future<void> close() async {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
    await events.close();
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  test(
    'A mirroring sequence gap silences immediately and needs a new snapshot',
    () async {
      final output = SilentOutput(),
          mirror = PeripheralMirror(output: SilentOutput());
      mirror.dispose();
      final actual = PeripheralMirror(output: output);
      actual.observe(mirroredPeer(), 1);
      await actual.setBuzzer(true);
      actual.event(toneEvent(25, 440, 100));
      actual.event(toneEvent(27, 880, 100));
      expect(actual.buzzerEnabled, isFalse);
      expect(actual.status, isNull);
      expect(actual.segments, isEmpty);
      expect(output.stops, 1);
      actual.dispose();
    },
  );
  test('MTU23 and Android14 MTU517 respect ATT512 and header7', () {
    expect(blePayloadBytes(23), 13);
    expect(blePayloadBytes(517), 505);
    final packet = BleFrame(
      0,
      0x1234,
      2,
      4,
      Uint8List.fromList([7, 8]),
    ).encode();
    expect(packet.take(7), [0, 0x34, 0x12, 2, 0, 4, 0]);
    expect(BleFrame.decode(packet).payload, [7, 8]);
    expect(() => BleFrame.decode(Uint8List(513)), throwsFormatException);
  });
  test(
    'Ordered duplicate fragments ACK without redispatch or altered bytes',
    () {
      final assembler = BleAssembler();
      final first = BleFrame(0, 1, 0, 4, Uint8List.fromList([1, 2]));
      expect(assembler.receive(first, 0).message, isNull);
      expect(assembler.receive(first, 1).message, isNull);
      final last = BleFrame(0, 1, 2, 4, Uint8List.fromList([3, 4]));
      expect(assembler.receive(last, 2).message, [1, 2, 3, 4]);
      expect(assembler.receive(last, 3).message, isNull);
      expect(
        () => assembler.receive(
          BleFrame(0, 1, 2, 4, Uint8List.fromList([9, 4])),
          4,
        ),
        throwsFormatException,
      );
      assembler.reset();
      assembler.receive(first, 0);
      expect(() => assembler.receive(last, 5001), throwsFormatException);
    },
  );
  test(
    'Offline BLE status and command use GATT only, real inner receipt/readback',
    () async {
      final peer = GattFixture()..install();
      var network = 0;
      final api = PakarApi(
        baseUrl: 'ble://aabbccddeeff',
        bluetooth: peer.transport(),
        discovery: () async {
          network++;
          throw StateError('No board discovery');
        },
        client: MockClient((_) async {
          network++;
          throw StateError('No board HTTP');
        }),
      );
      try {
        expect(api.connectionMethod, ConnectionMethod.bluetooth);
        final status = await api.status();
        expect(status.peer.known, isTrue);
        final actual = await api.fastCommand('/api/cells', {
          'ids': '1',
          'state': 'red',
          'effect': 'immediate',
        }, authorize: () => true);
        expect(actual!.json['cells'][0], 'red');
        final request = peer.requests.singleWhere(
          (value) => value['method'] == 'POST',
        );
        final command = jsonDecode(request['form']['packet'] as String) as Map;
        expect(request['path'], '/api/command');
        expect(request['request_id'], isNot(command['request_id']));
        expect(command['expires_at_ms'], 108500);
        expect(request['expires_at_ms'], 108500);
        expect(network, 0);
        expect(api.supportsBinaryFirmware, isFalse);
        api.appForeground = false;
        await expectLater(
          api.post('/api/cells', {'ids': '1', 'state': 'white'}),
          throwsA(isA<ConnectionStopped>()),
        );
        expect(
          peer.requests.where((value) => value['method'] == 'POST').length,
          1,
        );
      } finally {
        await api.close();
        await peer.close();
      }
    },
  );
  test(
    'Foreign response retires session; wrong cached identity never reused',
    () async {
      final peer = GattFixture()..install();
      final transport = peer.transport();
      try {
        await transport.connect('AA:BB:CC:DD:EE:FF', expectedBoard: peer.board);
        expect(
          () => transport.connect('AA:BB:CC:DD:EE:FF', expectedBoard: 'other'),
          throwsFormatException,
        );
        peer.badOwner = true;
        await expectLater(
          transport.request(
            'GET',
            '/api/status',
            {},
            board: '',
            boot: '',
            expires: 0,
          ),
          throwsFormatException,
        );
        expect(transport.ready, isFalse);
        expect(peer.disconnects, 1);
      } finally {
        await transport.close();
        await peer.close();
      }
    },
  );
  test('Shared notifications are session-owned; closing old probe preserves new owner', () async {
    final peer = GattFixture()..install();
    final old = peer.transport(), current = peer.transport();
    try {
      await old.connect('AA:BB:CC:DD:EE:FF');
      peer.session++;
      await current.connect('AA:BB:CC:DD:EE:FF');
      await old.close();
      expect(peer.disconnects, 0);
      expect(
        (await current.request(
          'GET',
          '/api/status',
          {},
          board: '',
          boot: '',
          expires: 0,
        )).status,
        200,
      );
      expect(current.ready, isTrue);
    } finally {
      await old.close();
      await current.close();
      await peer.close();
    }
  });
  test(
    'Authenticated board refuses BLE without transmitting credentials',
    () async {
      final peer = GattFixture()..install();
      peer.authenticated = true;
      final transport = peer.transport();
      try {
        await expectLater(
          transport.connect('AA:BB:CC:DD:EE:FF'),
          throwsFormatException,
        );
        expect(transport.ready, isFalse);
        expect(peer.requests, isEmpty);
        expect(peer.packets, isEmpty);
      } finally {
        await transport.close();
        await peer.close();
      }
    },
  );
  test(
    'Lost session cannot complete later intent; busy is not dispatched',
    () async {
      final peer = GattFixture()..install();
      final transport = peer.transport();
      final errors = transport.events.stream.listen(
        (_) {},
        onError: (Object _) {},
      );
      try {
        await transport.connect('AA:BB:CC:DD:EE:FF');
        peer.hold = true;
        final first = transport.request(
          'GET',
          '/api/status',
          {},
          board: '',
          boot: '',
          expires: 0,
        );
        final rejected = expectLater(first, throwsStateError);
        await Future<void>.delayed(Duration.zero);
        await expectLater(
          transport.request(
            'GET',
            '/api/status',
            {},
            board: '',
            boot: '',
            expires: 0,
          ),
          throwsA(isA<BleBusy>()),
        );
        peer.events.add({'type': 'disconnected', 'session': peer.session});
        await rejected;
        peer.session++;
        peer.hold = false;
        await transport.connect('AA:BB:CC:DD:EE:FF');
        peer.emit(
          BleFrame(0, 99, 0, 2, Uint8List.fromList([123, 125])).encode(),
          owner: peer.session - 1,
        );
        expect(
          (await transport.request(
            'GET',
            '/api/status',
            {},
            board: '',
            boot: '',
            expires: 0,
          )).status,
          200,
        );
        expect(peer.requests.length, 2);
      } finally {
        await errors.cancel();
        await transport.close();
        await peer.close();
      }
    },
  );
  test(
    'Selected Bluetooth profile persists; verified board merges network alias',
    () async {
      SharedPreferences.setMockInitialValues({});
      final preferences = await SharedPreferences.getInstance();
      final settings = ConnectionPreferences(
        preferences,
        secrets: TestSecrets(),
      );
      await settings.save('ledboard.local', 'admin', '');
      await settings.save(
        'ble://aabbccddeeff',
        'admin',
        '',
        bluetoothBoard: 'same-board',
      );
      final api = PakarApi();
      try {
        await settings.restore(api);
        expect(api.connectionMethod, ConnectionMethod.bluetooth);
        expect(api.expectedBluetoothBoard, 'same-board');
        await settings.save('other.local', 'admin', '');
        expect(preferences.getString('bluetooth_board_id'), 'same-board');
        api.selectTransport(
          ConnectionMethod.bluetooth,
          preferences.getString('last_bluetooth_endpoint')!,
          board: preferences.getString('bluetooth_board_id')!,
        );
        expect(api.expectedBluetoothBoard, 'same-board');
        expect(api.baseUrl, 'ble://aabbccddeeff');
        expect(
          preferences.getString('last_network_endpoint'),
          'http://other.local',
        );
        final records = coalesceDevices([
          DiscoveredDevice(
            'LAN',
            'http://ledboard.local',
            'mDNS',
            boardId: 'same-board',
            verified: true,
          ),
          DiscoveredDevice(
            'Bluetooth',
            api.baseUrl,
            'Bluetooth LE',
            boardId: 'same-board',
            verified: true,
          ),
        ]);
        expect(records, hasLength(1));
        expect(
          records.single.origins,
          containsAll(['http://ledboard.local', 'ble://aabbccddeeff']),
        );
        final registry = DeviceRegistry();
        final saved = await registry.save(
          SavedDevice(
            id: 'board',
            name: 'Table',
            endpoint: api.baseUrl,
            location: DeviceLocation.local,
            boardId: 'same-board',
          ),
          null,
        );
        expect(saved.endpoint, api.baseUrl);
      } finally {
        await api.close();
      }
    },
  );
}
