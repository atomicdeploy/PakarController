import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/discovery.dart';

import 'peer_fixture.dart';

const instance = '38729421-5ade-4a55-a241-523085beea18';
Map<String, dynamic> bridged() => {
  ...peerFixture().json,
  'hostname': 'ledboard',
  'board_id': 'physical-board',
  'boot_id': 'a' * 32,
  'bridge': {
    'product': 'PakarController',
    'hostname': 'controller-host',
    'instance_id': instance,
    'path': [instance],
  },
};
Uint8List mndp({int port = 8789, bool portField = true}) {
  final out = BytesBuilder()..add([0, 0, 0, 1]);
  void tlv(int type, List<int> bytes) {
    final header = ByteData(4)
      ..setUint16(0, type)
      ..setUint16(2, bytes.length);
    out
      ..add(header.buffer.asUint8List())
      ..add(bytes);
  }

  tlv(1, [2, 3, 4, 5, 6, 7]);
  tlv(5, utf8.encode('controller-host'));
  tlv(11, utf8.encode(instance));
  tlv(12, utf8.encode('Application endpoint'));
  tlv(0xff01, utf8.encode('pakarcontroller'));
  if (portField) tlv(0xff02, [(port >> 8) & 255, port & 255]);
  return out.toBytes();
}

void main() {
  test('Bridge hostname resolves to downstream board identity without relabeling it', () async {
    final api = PakarApi(
      baseUrl: 'http://controller-host.local:8789',
      client: MockClient(
        (_) async => http.Response(jsonEncode(bridged()), 200),
      ),
    );
    final status = await api.status();
    expect(status.hostname, 'ledboard');
    expect(status.json['board_id'], 'physical-board');
    await api.close();
  });
  test(
    'Bridge hostname exceptions require an unambiguous bounded instance path',
    () async {
      for (final mutation in <Map<String, dynamic>>[
        {'hostname': 'foreign-host'},
        {'instance_id': 'not-an-instance'},
        {'path': []},
        {
          'path': [instance, instance],
        },
        {
          'path': ['foreign'],
        },
        {'path': List.filled(5, instance)},
      ]) {
        final value = bridged();
        value['bridge'] = {...value['bridge'] as Map, ...mutation};
        final api = PakarApi(
          baseUrl: 'http://controller-host.local:8789',
          client: MockClient(
            (_) async => http.Response(jsonEncode(value), 200),
          ),
        );
        await expectLater(api.status(), throwsFormatException);
        await api.close();
      }
    },
  );
  test('MNDP application endpoints use vendor API port and never become a board ID', () {
    final peer = parseMndp(mndp(), '192.0.2.1')!;
    expect(peer.endpoint, 'http://192.0.2.1:8789');
    expect(peer.boardId, isEmpty);
    expect(peer.instanceId, instance);
    expect(peer.kind, 'bridge');
    expect(parseMndp(mndp(port: 0), '192.0.2.1'), isNull);
    expect(parseMndp(mndp(portField: false), '192.0.2.1'), isNull);
    final packet = mndp();
    expect(
      parseMndp(packet.sublist(0, packet.length - 1), '192.0.2.1'),
      isNull,
    );
  });
  test(
    'DNS-SD preserves actual bridge hostname, not the friendly service label',
    () {
      final peer = parseMdns({
        'name': 'PakarController on controller-host',
        'endpoint': 'http://192.0.2.1:8789',
        'address': '192.0.2.1',
        'hostname': 'controller-host',
        'kind': 'bridge',
        'instance_id': instance,
      })!;
      expect(peer.kind, 'bridge');
      expect(peer.aliases, contains('http://controller-host.local:8789'));
      final noHost = parseMdns({
        'name': 'Nice service',
        'endpoint': 'http://192.0.2.1:8789',
        'kind': 'bridge',
      })!;
      expect(noHost.aliases, isEmpty);
      final board = parseMdns({
        'name': 'ledboard',
        'hostname': '',
        'endpoint': 'http://192.0.2.1',
      })!;
      expect(board.aliases, contains('http://ledboard.local'));
    },
  );
  test('Bridge public XML is distinct from the physical firmware UDN contract', () {
    const xml =
        '<root><device><friendlyName>controller-host</friendlyName><manufacturer>AtomicDeploy</manufacturer>'
        '<modelName>PakarController</modelName><modelNumber>bridge</modelNumber>'
        '<modelURL>https://github.com/atomicdeploy/PakarController</modelURL><UDN>uuid:$instance</UDN></device></root>';
    expect(pakarXmlIdentity(xml), ('controller-host', 'uuid:$instance'));
    expect(
      pakarXmlIdentity(
        xml.replaceFirst('<modelNumber>bridge</modelNumber>', ''),
      ),
      isNull,
    );
    expect(
      pakarXmlIdentity(xml.replaceFirst('AtomicDeploy', 'Foreign')),
      isNull,
    );
  });
  test(
    'Direct board and repeater routes coalesce only after same board identity',
    () {
      final board = DiscoveredDevice(
        'Table',
        'http://ledboard.local',
        'SSDP',
        boardId: 'one',
        verified: true,
      );
      final relay = DiscoveredDevice(
        'Relay',
        'http://controller-host.local:8789',
        'MNDP',
        boardId: 'one',
        instanceId: instance,
        kind: 'bridge',
        verified: true,
      );
      final merged = coalesceDevices([board, relay]);
      expect(merged.length, 1);
      expect(
        merged.single.origins,
        containsAll([board.endpoint, relay.endpoint]),
      );
      final other = DiscoveredDevice(
        'Other',
        'http://ledboard.local',
        'SSDP',
        boardId: 'two',
      );
      expect(coalesceDevices([board, other]).length, 2);
    },
  );
}
