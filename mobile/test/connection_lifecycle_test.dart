import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/command_readback.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/cell_search.dart';

import 'peer_fixture.dart';

class LifecycleApi extends PakarApi {
  int reads = 0, writes = 0;
  int mappingReads = 0;
  bool healthyWs = false;
  bool failReads = false;
  int subscriptions = 0, probes = 0;
  DeviceStatus observed = peerFixture();
  @override
  bool get liveFresh => healthyWs;
  bool failAfterWrite = false;
  final updates = StreamController<DeviceStatus>.broadcast();
  @override
  Future<DeviceStatus> status() async {
    ++reads;
    if (failReads || writes > 0 && failAfterWrite) {
      throw const FormatException('Readback unavailable');
    }
    return observed;
  }

  @override
  Future<CellMap> mapping() async {
    ++mappingReads;
    return CellMap.defaults();
  }

  @override
  Stream<DeviceStatus> liveStatus() {
    subscriptions++;
    return updates.stream;
  }

  @override
  bool requestLiveStatus() {
    probes++;
    return true;
  }

  @override
  Future<void> post(
    String path, [
    Map<String, String> values = const {},
  ]) async {
    ++writes;
  }

  @override
  Future<Map<String, dynamic>> postJson(
    String path,
    Map<String, String> values, {
    bool Function()? authorize,
  }) async {
    if (authorize?.call() == false) throw const StaleConnection();
    ++writes;
    return {
      'accepted': true,
      'count': parseCellSearch(values['ids'] ?? '').length,
    };
  }

  @override
  Future<void> close() async {
    await updates.close();
    await super.close();
  }
}

void main() {
  testWidgets(
    'Healthy WS avoids hidden settings/status polling and needless map reads',
    (tester) async {
      final api = LifecycleApi()..healthyWs = true;
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Table').last);
      await tester.pumpAndSettle();
      final reads = api.reads, maps = api.mappingReads;
      for (var tick = 0; tick < 6; tick++) {
        api.updates.add(peerFixture());
        await tester.pump();
        await tester.pump(const Duration(seconds: 3));
        await tester.pump();
      }
      expect(api.reads, reads);
      expect(api.mappingReads, maps);
      expect(find.text('No acknowledged board'), findsNothing);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  setUp(() {
    SharedPreferences.setMockInitialValues({});
    FlutterSecureStorage.setMockInitialValues({});
  });
  test('USB intent and Disconnect never dispatch network reads, writes, or discovery', () async {
    var requests = 0, discovery = 0;
    final api = PakarApi(
      client: MockClient((_) async {
        ++requests;
        return http.Response('{"version":"0.2.8","hostname":"ledboard"}', 200);
      }),
      discovery: () async {
        ++discovery;
        return {};
      },
    );
    api.connectionMethod = ConnectionMethod.usb;
    await expectLater(api.status(), throwsFormatException);
    await expectLater(api.post('/api/cell'), throwsFormatException);
    await expectLater(
      api.postJson('/api/led/output', {}),
      throwsFormatException,
    );
    await expectLater(
      api.maintenanceBytes('/api/firmware', authorize: () => true),
      throwsFormatException,
    );
    await expectLater(api.liveStatus().first, throwsFormatException);
    api.connectionMethod = ConnectionMethod.network;
    api.disconnect();
    await expectLater(api.status(), throwsA(isA<ConnectionStopped>()));
    expect(requests, 0);
    expect(discovery, 0);
    api.connect();
    expect((await api.status()).hostname, 'ledboard');
    expect(requests, 1);
    await api.close();
  });
  test(
    'Cell and output readback never infer an applied value from defaults',
    () {
      expect(
        commandReadbackMatches(peerFixture(), '/api/cell', {
          'id': '1',
          'state': 'red',
        }),
        false,
      );
      expect(
        commandReadbackMatches(peerFixture(), '/api/cell', {
          'id': '1',
          'state': 'white',
        }),
        true,
      );
      expect(
        commandReadbackMatches(peerFixture(), '/api/cell', {
          'id': '81',
          'state': 'white',
        }),
        false,
      );
      expect(
        commandReadbackMatches(peerFixture(), '/api/led/output', {
          'enabled': 'false',
        }),
        false,
      );
      expect(
        commandReadbackMatches(peerFixture(), '/api/brightness', {
          'value': '255',
        }),
        false,
      );
      final output = DeviceStatus({
        ...peerFixture().json,
        'led_output': {'enabled': false, 'mode': 'static'},
      });
      expect(
        commandReadbackMatches(output, '/api/led/output', {
          'enabled': 'false',
          'mode': 'static',
        }),
        true,
      );
      expect(
        commandReadbackMatches(output, '/api/led/output', {'mode': 'effects'}),
        false,
      );
    },
  );
  test(
    'Mapping is rejected when the peer reboots between GET and postflight',
    () async {
      var reads = 0;
      final api = PakarApi(
        client: MockClient((request) async {
          if (request.url.path == '/api/cells/mapping') {
            return http.Response(
              jsonEncode({
                'physical': List.generate(80, (index) => index + 1),
                'names': List.generate(80, (index) => '${index + 1}'),
              }),
              200,
            );
          }
          return http.Response(
            jsonEncode({
              'version': '0.2.8',
              'hostname': 'ledboard',
              'board_id': 'fixture-board',
              'boot_id': ++reads == 1 ? 'first-boot' : 'second-boot',
            }),
            200,
          );
        }),
      );
      await expectLater(api.mapping(), throwsA(isA<StaleConnection>()));
      expect(reads, 2);
      await api.close();
    },
  );
  testWidgets(
    'Disconnect retires live updates and polling until explicit Connect',
    (tester) async {
      final api = LifecycleApi();
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.byTooltip('Disconnect'));
      await tester.pumpAndSettle();
      final reads = api.reads;
      api.updates.add(peerFixture());
      await tester.pump(const Duration(seconds: 12));
      expect(api.reads, reads);
      expect(find.text('No acknowledged board'), findsOneWidget);
      expect(find.byTooltip('Connect'), findsOneWidget);
      await tester.tap(find.byTooltip('Connect'));
      await tester.pumpAndSettle();
      expect(api.reads, greaterThan(reads));
      expect(find.byTooltip('Disconnect'), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  testWidgets(
    'Lost post-ACK readback stops bulk without discarding a fresh workspace',
    (tester) async {
      final api = LifecycleApi()..failAfterWrite = true;
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Table').last);
      await tester.pumpAndSettle();
      await tester.enterText(
        find.widgetWithText(TextField, 'Find logical cell'),
        '1-3',
      );
      await tester.pump();
      await tester.ensureVisible(find.text('Set red'));
      await tester.tap(find.text('Set red'));
      await tester.pumpAndSettle();
      expect(api.writes, 1);
      expect(find.textContaining('Readback unavailable'), findsOneWidget);
      // Ordinary successful status pushes cannot acknowledge a failed command.
      api.failAfterWrite = false;
      for (var tick = 0; tick < 3; tick++) {
        api.updates.add(peerFixture());
        await tester.pump();
      }
      expect(find.textContaining('Readback unavailable'), findsOneWidget);
      await tester.tap(find.byKey(const Key('command-status-details')));
      await tester.pumpAndSettle();
      expect(find.text('Command not completed'), findsOneWidget);
      expect(find.textContaining('Readback unavailable'), findsNWidgets(2));
      await tester.tap(find.text('Close'));
      await tester.pumpAndSettle();
      api.failReads = true;
      expect(find.text('No acknowledged board'), findsNothing);
      expect(find.byKey(const ValueKey('table-cell-canvas')), findsOneWidget);
      // Expiry revokes actuation, never the user-owned table workspace.
      expect(
        tester.widget<CellButton>(find.byKey(const ValueKey('cell-1'))).state,
        'white',
      );
      await tester.pump(const Duration(seconds: 7));
      await tester.pumpAndSettle();
      expect(api.writes, 1);
      expect(find.text('No acknowledged board'), findsNothing);
      expect(find.byKey(const ValueKey('table-cell-canvas')), findsOneWidget);
      expect(
        tester
            .widget<FilledButton>(find.widgetWithText(FilledButton, 'Set red'))
            .onPressed,
        isNull,
      );
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
}
