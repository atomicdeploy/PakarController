import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/discovery.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

const available = DiscoveredDevice(
  'Only table',
  'http://192.0.2.3',
  'MNDP',
  boardId: 'only-board',
  hostname: 'onlytable',
  verified: true,
  compatible: true,
);

class StartupApi extends PakarApi {
  final updates = StreamController<DeviceStatus>.broadcast();
  DeviceStatus? connected;
  int writes = 0;
  int selectedStatusReads = 0;
  @override
  DeviceStatus? get freshStatus => connected;
  @override
  Future<DeviceStatus> status() async {
    if (baseUrl != 'http://onlytable.local') {
      throw const SocketException('Previously selected board unavailable');
    }
    selectedStatusReads++;
    return connected = DeviceStatus({
      ...peerFixture(hostname: 'onlytable').json,
      'board_id': 'only-board',
      'boot_id': 'boot',
    });
  }

  @override
  Stream<DeviceStatus> liveStatus() => updates.stream;
  @override
  Future<CellMap> mapping() async => CellMap.defaults();
  @override
  Future<void> post(
    String path, [
    Map<String, String> values = const {},
  ]) async {
    writes++;
  }

  @override
  Future<void> close() async {
    if (!updates.isClosed) await updates.close();
    await super.close();
  }
}

void main() {
  testWidgets(
    'An open restored connection editor prevents automatic peer handoff',
    (tester) async {
      SharedPreferences.setMockInitialValues({
        'table_workspaces': jsonEncode({
          'activity': 'connection',
          'connection_draft': {'endpoint': 'http://my-unsaved.local'},
        }),
      });
      FlutterSecureStorage.setMockInitialValues({});
      var scans = 0;
      final api = StartupApi();
      final discovery = DeviceDiscovery(
        catalog: DiscoveryCatalog(),
        advertisements: () async {
          scans++;
          return [available];
        },
        verify: (peer) async => peer,
      );
      await tester.pumpWidget(
        MaterialApp(
          home: ControlShell(api: api, discovery: discovery),
        ),
      );
      await tester.pumpAndSettle();
      expect(scans, 0);
      expect(api.baseUrl, 'http://ledboard.local');
      expect(find.text('http://my-unsaved.local'), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  setUp(() {
    SharedPreferences.setMockInitialValues({
      'table_workspaces': jsonEncode({
        'connection': {'method': 'network', 'wanted': false},
      }),
    });
    FlutterSecureStorage.setMockInitialValues({
      'pakarcontroller.device_password': 'old-board-secret',
    });
  });
  testWidgets(
    'Startup defaults auto on, ignores last-session incidental Disconnect and adopts one verified board without old credentials',
    (tester) async {
      var scans = 0;
      final api = StartupApi();
      final discovery = DeviceDiscovery(
        catalog: DiscoveryCatalog(),
        advertisements: () async {
          scans++;
          return [available];
        },
        verify: (peer) async => peer,
      );
      await tester.pumpWidget(
        MaterialApp(
          home: ControlShell(api: api, discovery: discovery),
        ),
      );
      await tester.pumpAndSettle();
      // Connect intent changes before the old subscription is cancelled and
      // the new owner's HTTP observation completes. pumpAndSettle alone only
      // drains scheduled frames, not the entire asynchronous handoff.
      for (
        var tick = 0;
        tick < 100 && find.text('No acknowledged board').evaluate().isNotEmpty;
        tick++
      ) {
        await tester.pump(const Duration(milliseconds: 20));
      }
      expect(scans, 1);
      expect(api.baseUrl, 'http://onlytable.local');
      expect(api.connectionWanted, true);
      expect(api.password, '');
      expect(api.writes, 0);
      expect(
        api.selectedStatusReads,
        greaterThanOrEqualTo(1),
        reason: find
            .byType(Text)
            .evaluate()
            .map((entry) => (entry.widget as Text).data)
            .whereType<String>()
            .join(' | '),
      );
      expect(api.connected?.peer.known, true);
      expect(api.connected?.json['board_id'], 'only-board');
      expect(find.text('No acknowledged board'), findsNothing);
      expect(find.text('Table board · onlytable.local'), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );

  testWidgets(
    'Disconnect during startup scan cannot be undone by its late result',
    (tester) async {
      final pending = Completer<List<DiscoveredDevice>>();
      final api = StartupApi();
      final discovery = DeviceDiscovery(
        catalog: DiscoveryCatalog(),
        advertisements: () => pending.future,
        verify: (peer) async => peer,
      );
      await tester.pumpWidget(
        MaterialApp(
          home: ControlShell(api: api, discovery: discovery),
        ),
      );
      await tester.pumpAndSettle();
      await tester.tap(find.byTooltip('Disconnect'));
      await tester.pump();
      pending.complete([available]);
      await tester.pumpAndSettle();
      expect(api.connectionWanted, false);
      expect(api.baseUrl, 'http://ledboard.local');
      expect(api.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );

  testWidgets(
    'Explicit manual startup preference leaves unavailable selected route untouched',
    (tester) async {
      SharedPreferences.setMockInitialValues({
        'automatic_board_connection': false,
      });
      var scans = 0;
      final api = StartupApi();
      final discovery = DeviceDiscovery(
        catalog: DiscoveryCatalog(),
        advertisements: () async {
          scans++;
          return [available];
        },
        verify: (peer) async => peer,
      );
      await tester.pumpWidget(
        MaterialApp(
          home: ControlShell(api: api, discovery: discovery),
        ),
      );
      await tester.pumpAndSettle();
      expect(scans, 0);
      expect(api.baseUrl, 'http://ledboard.local');
      expect(api.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
}
