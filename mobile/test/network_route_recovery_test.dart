import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';

import 'peer_fixture.dart';

void main() {
  test('Unknown-board hostname recovery refuses two physical IDs, but accepts two interfaces of one ID', () async {
    for (final sameBoard in [false, true]) {
      final api = PakarApi(
        client: MockClient((request) async {
          if (request.url.host.endsWith('.local')) {
            throw const SocketException('Fixture DNS unavailable');
          }
          return http.Response(
            jsonEncode({
              ...peerFixture().json,
              'board_id': sameBoard ? 'one' : request.url.host,
              'boot_id': 'boot',
            }),
            200,
          );
        }),
        discovery: () async => {'http://192.0.2.1', 'http://192.0.2.2'},
      );
      try {
        if (sameBoard) {
          expect((await api.status()).json['board_id'], 'one');
        } else {
          await expectLater(api.status(), throwsA(isA<SocketException>()));
          expect(api.transportUrl, 'http://ledboard.local');
        }
      } finally {
        await api.close();
      }
    }
  });
  test(
    'Board hostname updates never replace a loopback-forward listener URI',
    () async {
      var hostname = 'ledboard';
      final api = PakarApi(
        baseUrl: 'http://127.0.0.1:18082',
        client: MockClient(
          (request) async => http.Response(
            jsonEncode({
              ...peerFixture().json,
              'hostname': hostname,
              'board_id': 'same-board',
              'boot_id': 'boot',
            }),
            200,
          ),
        ),
      );
      try {
        await api.status();
        hostname = 'renamed';
        expect((await api.status()).hostname, 'renamed');
        expect(api.baseUrl, 'http://127.0.0.1:18082');
        expect(api.transportUrl, 'http://127.0.0.1:18082');
        expect(api.webSocketUri.host, '127.0.0.1');
        expect(api.webSocketUri.port, 18082);
      } finally {
        await api.close();
      }
    },
  );
  test('Saved hostname first, verified last IP fallback; DHCP recovery retains board pin and adopts actual rename', () async {
    final reads = <String>[];
    var discovery = 0, phase = 'last-ip';
    final api = PakarApi(
      client: MockClient((request) async {
        expect(request.method, 'GET');
        reads.add(request.url.host);
        if (request.url.host.endsWith('.local') ||
            phase == 'changed' && request.url.host == '192.0.2.2') {
          throw const SocketException('unreachable fixture route');
        }
        return http.Response(
          jsonEncode({
            ...peerFixture().json,
            'hostname': phase == 'changed' ? 'renamed' : 'ledboard',
            'board_id': phase == 'foreign' ? 'other-board' : 'same-board',
            'boot_id': 'boot',
          }),
          200,
        );
      }),
      discovery: () async {
        discovery++;
        return {'http://192.0.2.3'};
      },
    );
    api.restoreNetworkRecovery({
      'endpoint': 'http://ledboard.local',
      'board_id': 'same-board',
      'origin': 'http://192.0.2.2',
    });
    try {
      final owner = api.connectionRevision;
      expect((await api.status()).json['board_id'], 'same-board');
      expect(reads, ['ledboard.local', '192.0.2.2', '192.0.2.2']);
      expect(discovery, 0);
      expect(api.baseUrl, 'http://ledboard.local');
      expect(api.transportUrl, 'http://192.0.2.2');
      expect(api.webSocketUri.host, '192.0.2.2');
      phase = 'changed';
      expect((await api.status()).hostname, 'renamed');
      expect(discovery, 1);
      expect(api.baseUrl, 'http://renamed.local');
      expect(api.transportUrl, 'http://192.0.2.3');
      expect(api.webSocketUri.host, '192.0.2.3');
      expect(
        api.connectionRevision,
        owner,
        reason: 'Same-board rename does not destroy workspace owner',
      );
      expect(api.networkRecovery, {
        'endpoint': 'http://renamed.local',
        'board_id': 'same-board',
        'origin': 'http://192.0.2.3',
      });
      phase = 'foreign';
      await expectLater(api.status(), throwsFormatException);
      expect(
        discovery,
        1,
        reason: 'Responsive wrong identity never authorizes fallback',
      );
    } finally {
      await api.close();
    }
  });

  test(
    'Saved route cannot change scheme/port/owner or authorize another board',
    () async {
      var requests = 0;
      final api = PakarApi(
        client: MockClient((request) async {
          requests++;
          return http.Response(
            jsonEncode({
              ...peerFixture().json,
              'board_id': 'other',
              'boot_id': 'boot',
            }),
            200,
          );
        }),
      );
      try {
        for (final bad in [
          {
            'endpoint': 'http://other.local',
            'board_id': 'one',
            'origin': 'http://192.0.2.2',
          },
          {
            'endpoint': 'http://ledboard.local',
            'board_id': 'one',
            'origin': 'https://192.0.2.2',
          },
          {
            'endpoint': 'http://ledboard.local',
            'board_id': 'one',
            'origin': 'http://192.0.2.2:81',
          },
        ]) {
          api.restoreNetworkRecovery(bad);
          expect(api.networkRecovery, isNull);
        }
        api.restoreNetworkRecovery({
          'endpoint': 'http://ledboard.local',
          'board_id': 'one',
          'origin': 'http://192.0.2.2',
        });
        await expectLater(api.status(), throwsFormatException);
        expect(requests, 1);
      } finally {
        await api.close();
      }
    },
  );
}
