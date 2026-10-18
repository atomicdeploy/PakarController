import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/discovery.dart';
import 'package:pakarcontroller/api.dart';

const description =
    '<?xml version="1.0"?><root xmlns="urn:schemas-upnp-org:device-1-0"><device>'
    '<friendlyName>Studio &amp; table</friendlyName><modelName>Pakar Table</modelName>'
    '<manufacturer>AtomicDeploy</manufacturer><modelURL>https://github.com/atomicdeploy/PakarController</modelURL>'
    '<UDN>uuid:38323636-4558-4dda-9188-cda0e6112233</UDN></device></root>';
void main() {
  test(
    'DNS-SD binds saved hostname to resolved IP before an ordinary DNS lookup',
    () {
      final peer = parseMdns({
        'name': 'ledboard',
        'endpoint': 'http://192.0.2.4:80',
        'address': '192.0.2.4',
      })!;
      const saved = DiscoveredDevice('My table', 'http://ledboard.local', '');
      const ssdp = DiscoveredDevice('Table', 'http://192.0.2.4', 'SSDP');
      expect(coalesceDevices([saved, peer, ssdp]), hasLength(1));
      expect(
        peer.verified,
        false,
        reason: 'A DNS-SD alias alone never verifies compatibility',
      );
      expect(
        parseMdns({'name': 'Friendly name', 'endpoint': 'http://192.0.2.4'})!
            .aliases,
        isEmpty,
      );
    },
  );
  test(
    'Authenticated reads cannot forward credentials through a redirect',
    () async {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      final requests = <String>[];
      final listener = server.listen((request) async {
        requests.add(request.uri.path);
        request.response.statusCode = 302;
        request.response.headers.set('location', '/must-not-follow');
        await request.response.close();
      });
      final api = PakarApi(
        baseUrl: 'http://127.0.0.1:${server.port}',
        password: 'fixture-only',
      );
      try {
        await expectLater(api.status(), throwsFormatException);
        expect(requests, ['/api/status']);
      } finally {
        await api.close();
        await listener.cancel();
        await server.close(force: true);
      }
    },
  );
  test(
    'Bounded XML verifies Pakar identity, not router/DTD/duplicate tags',
    () {
      expect(pakarXmlIdentity(description)?.$1, 'Studio & table');
      expect(
        pakarXmlIdentity(
          description.replaceAll(
            '<modelURL>https://github.com/atomicdeploy/PakarController</modelURL>',
            '',
          ),
        ),
        isNotNull,
      );
      expect(
        pakarXmlIdentity(description.replaceAll('Pakar Table', 'Router')),
        isNull,
      );
      expect(pakarXmlIdentity('<!DOCTYPE root>$description'), isNull);
      expect(
        pakarXmlIdentity(
          description.replaceAll(
            '</device>',
            '<modelName>Pakar Table</modelName></device>',
          ),
        ),
        isNull,
      );
      expect(pakarXmlIdentity('x' * 17000), isNull);
    },
  );
  test(
    'SSDP source pinning and duplicate headers reject unrelated targets',
    () {
      Uint8List packet(String value) => Uint8List.fromList(utf8.encode(value));
      const reply =
          'HTTP/1.1 200 OK\r\nLOCATION: http://192.0.2.5/description.xml\r\n';
      expect(parseSsdp(packet(reply), source: '192.0.2.5'), isNotNull);
      expect(parseSsdp(packet(reply), source: '192.0.2.6'), isNull);
      expect(
        parseSsdp(packet('${reply}LOCATION: http://192.0.2.6/\r\n')),
        isNull,
      );
    },
  );
  test(
    'Real read-only XML/status probes: no credentials, redirects or mutations',
    () async {
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      final requests = <String>[];
      var response = 'pakar';
      final listener = server.listen((request) async {
        requests.add(request.uri.path);
        expect(request.method, 'GET');
        expect(request.headers.value('authorization'), isNull);
        if (request.uri.path == '/description.xml') {
          if (response == 'redirect') {
            request.response.statusCode = 302;
            request.response.headers.set('location', '/must-not-follow');
          } else {
            request.response.write(
              response == 'pakar' ? description : '<root>Router</root>',
            );
          }
        } else {
          request.response.statusCode = 401;
          request.response.write('{}');
        }
        await request.response.close();
      });
      try {
        final candidate = DiscoveredDevice(
          'Advertisement',
          'http://127.0.0.1:${server.port}',
          'SSDP',
        );
        final peer = await verifyDiscoveredDevice(candidate);
        expect(peer?.verified, true);
        expect(peer?.authRequired, true);
        expect(peer?.ips, contains('127.0.0.1'));
        response = 'router';
        expect(await verifyDiscoveredDevice(candidate), isNull);
        response = 'redirect';
        expect(await verifyDiscoveredDevice(candidate), isNull);
        expect(requests, isNot(contains('/must-not-follow')));
      } finally {
        await listener.cancel();
        await server.close(force: true);
      }
    },
  );
}
