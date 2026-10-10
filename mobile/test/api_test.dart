import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';

void main() {
  test('Endpoint rejects credentials/path injection', () {
    expect(
      PakarApi.normalizeEndpoint(' ledboard.local '),
      'http://ledboard.local',
    );
    expect(
      PakarApi.normalizeEndpoint('https://renamed.local:8443/'),
      'https://renamed.local:8443',
    );
    for (final value in [
      'ftp://board.local',
      'http://user:pass@board.local',
      'http://board.local/path',
      'http://board.local?x=1',
      'http://board.local#frag',
    ]) {
      expect(() => PakarApi.normalizeEndpoint(value), throwsFormatException);
    }
  });
  test('Mutation with lost response is attempted exactly once', () async {
    var posts = 0, discoveries = 0;
    final api = PakarApi(
      client: MockClient((request) async {
        if (request.method == 'POST') {
          ++posts;
          throw const SocketException('lost response after apply');
        }
        return http.Response(jsonEncode({'hostname': 'ledboard'}), 200);
      }),
      discovery: () async {
        ++discoveries;
        return {'http://192.0.2.1'};
      },
    );
    await expectLater(
      api.post('/api/v1/game/reset'),
      throwsA(isA<OperationUncertain>()),
    );
    expect(posts, 1);
    expect(discoveries, 0);
    await api.close();
  });
  test('Renamed local label uses identity-checked read discovery', () async {
    final api = PakarApi(
      baseUrl: 'http://renamed.local',
      client: MockClient((request) async {
        if (request.url.host.endsWith('.local')) {
          throw const SocketException('no mDNS');
        }
        return http.Response(
          jsonEncode({
            'hostname': request.url.host == '192.0.2.1' ? 'other' : 'renamed',
          }),
          200,
        );
      }),
      discovery: () async => {'http://192.0.2.1', 'http://192.0.2.2'},
    );
    expect((await api.status()).hostname, 'renamed');
    expect(api.transportUrl, 'http://192.0.2.2');
    api.baseUrl = 'http://new.local';
    expect(api.transportUrl, 'http://new.local');
    await api.close();
  });
  test('Wrong responding hostname cannot authorize a mutation', () async {
    var posts = 0;
    final api = PakarApi(
      client: MockClient((request) async {
        if (request.method == 'POST') ++posts;
        return http.Response('{"hostname":"other"}', 200);
      }),
    );
    await expectLater(api.post('/api/v1/game/reset'), throwsFormatException);
    expect(posts, 0);
    await api.close();
  });
}
