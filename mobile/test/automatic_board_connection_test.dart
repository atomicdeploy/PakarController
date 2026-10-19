import 'dart:async';
import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/automatic_board_connection.dart';
import 'package:pakarcontroller/discovery.dart';

import 'peer_fixture.dart';

DiscoveredDevice candidate(
  String board,
  String origin, {
  String hostname = 'table',
  bool compatible = true,
  String kind = 'board',
  bool auth = false,
}) => DiscoveredDevice(
  'Table',
  origin,
  'MNDP',
  boardId: board,
  hostname: hostname,
  compatible: compatible,
  verified: true,
  authRequired: auth,
  kind: kind,
);

void main() {
  test('Candidate overflow never claims a unique board; duplicate origins do not cause overflow', () async {
    final first = candidate('first', 'http://192.0.2.1');
    final last = candidate('second', 'http://192.0.2.200');
    final filler = List.generate(
      127,
      (i) => candidate('unrelated-$i', 'http://198.51.100.${i + 1}'),
    );
    final discovery = DeviceDiscovery(
      catalog: DiscoveryCatalog(),
      advertisements: () async => [first, ...filler, last],
      verify: (p) async =>
          p.boardId == 'first' || p.boardId == 'second' ? p : null,
    );
    final api = PakarApi();
    try {
      expect(await AutomaticBoardConnector(api, discovery).discover(), isNull);
      expect(discovery.verificationComplete, false);
      final duplicates = DeviceDiscovery(
        catalog: DiscoveryCatalog(),
        advertisements: () async => List.filled(160, first),
        verify: (p) async => p,
      );
      expect(
        (await AutomaticBoardConnector(
          api,
          duplicates,
        ).discover())?.peer.boardId,
        'first',
      );
      expect(duplicates.verificationComplete, true);
      final aliases = DeviceDiscovery(
        catalog: DiscoveryCatalog(),
        advertisements: () async => [],
        verify: (p) async => p.endpoint == first.endpoint ? first : null,
      );
      await aliases.scan(
        seeds: [
          DiscoveredDevice(
            'Aliases',
            first.endpoint,
            'saved',
            aliases: {...filler.map((p) => p.endpoint), last.endpoint},
          ),
        ],
      );
      expect(aliases.verificationComplete, false);
      expect(AutomaticBoardConnector(api, aliases).currentChoice(), isNull);
    } finally {
      await api.close();
    }
  });
  test(
    'A bridge alias never masks a verified physical route for the same board',
    () {
      final catalog = DiscoveryCatalog()
        ..merge([
          candidate('one', 'http://192.0.2.1'),
          candidate('one', 'http://192.0.2.2:8789', kind: 'bridge'),
        ]);
      expect(uniqueAutomaticBoard(catalog)?.route, 'http://192.0.2.1');
      expect(uniqueAutomaticBoard(catalog)?.peer.kind, 'board');
      expect(
        catalog.merge([]),
        hasLength(1),
        reason: 'Ordinary list UI still unifies bridge aliases',
      );
    },
  );
  test('Local forward aliases retain their actual listener origin and forwarded port', () {
    for (final origin in [
      'http://127.0.0.1:18082',
      'http://localhost:18082',
      'http://[::1]:18082',
    ]) {
      final catalog = DiscoveryCatalog()
        ..merge([candidate('one', origin, hostname: 'ledboard')]);
      expect(uniqueAutomaticBoard(catalog)?.endpoint, origin);
    }
  });
  test(
    'Exactly one current compatible physical board; interfaces are not devices',
    () {
      final catalog = DiscoveryCatalog();
      catalog.merge([
        candidate('one', 'http://192.0.2.1'),
        candidate('one', 'http://192.0.2.2'),
      ]);
      final choice = uniqueAutomaticBoard(catalog)!;
      expect(choice.peer.boardId, 'one');
      expect(
        choice.peer.origins,
        containsAll(['http://192.0.2.1', 'http://192.0.2.2']),
      );
      expect(choice.endpoint, 'http://table.local');
      catalog.merge([
        candidate('one', 'http://192.0.2.1'),
        candidate('two', 'http://192.0.2.2'),
      ]);
      expect(uniqueAutomaticBoard(catalog), isNull);
      catalog.merge([]);
      expect(
        uniqueAutomaticBoard(catalog),
        isNull,
        reason: 'Retained history is not availability',
      );
      for (final peer in [
        candidate('one', 'http://192.0.2.1', compatible: false),
        candidate('one', 'http://192.0.2.1', auth: true),
        candidate('one', 'http://192.0.2.1', kind: 'bridge'),
        candidate('', 'http://192.0.2.1'),
      ]) {
        catalog.merge([peer]);
        expect(uniqueAutomaticBoard(catalog), isNull);
      }
    },
  );

  test('Healthy selected board, explicit Disconnect, USB, BLE and manual policy suppress auto scans', () async {
    var scans = 0;
    final discovery = DeviceDiscovery(
      catalog: DiscoveryCatalog(),
      advertisements: () async {
        scans++;
        return [];
      },
    );
    final api = PakarApi(
      client: MockClient(
        (_) async => http.Response(
          jsonEncode({
            ...peerFixture().json,
            'board_id': 'selected',
            'boot_id': 'boot',
          }),
          200,
        ),
      ),
    );
    final automatic = AutomaticBoardConnector(api, discovery);
    try {
      await api.status();
      expect(await automatic.discover(), isNull);
      api.disconnect();
      expect(await automatic.discover(), isNull);
      api.connect();
      api.connectionMethod = ConnectionMethod.usb;
      expect(await automatic.discover(), isNull);
      api.connectionMethod = ConnectionMethod.bluetooth;
      expect(await automatic.discover(), isNull);
      api.connectionMethod = ConnectionMethod.network;
      api.automaticDiscovery = false;
      expect(await automatic.discover(), isNull);
      expect(scans, 0);
    } finally {
      await api.close();
    }
  });

  test('A complete verified sweep is required and a late scan cannot override Disconnect', () async {
    final pending = Completer<List<DiscoveredDevice>>();
    var scans = 0;
    final peer = candidate('one', 'http://192.0.2.1');
    final discovery = DeviceDiscovery(
      catalog: DiscoveryCatalog(),
      advertisements: () {
        scans++;
        return pending.future;
      },
      verify: (p) async => p,
    );
    final api = PakarApi();
    final automatic = AutomaticBoardConnector(api, discovery);
    try {
      final first = automatic.discover(), joined = automatic.discover();
      api.disconnect();
      pending.complete([peer]);
      expect(await first, isNull);
      expect(await joined, isNull);
      expect(scans, 1);
      api.connect();
      expect(automatic.currentChoice()?.peer.boardId, 'one');
      api.appForeground = false;
      expect(automatic.currentChoice(), isNull);
    } finally {
      await api.close();
    }
  });
}
