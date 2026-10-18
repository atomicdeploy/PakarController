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
  bool failAfterWrite = false;
  final updates = StreamController<DeviceStatus>.broadcast();
  @override
  Future<DeviceStatus> status() async {
    ++reads;
    if (writes > 0 && failAfterWrite) {
      throw const FormatException('Readback unavailable');
    }
    return peerFixture();
  }

  @override
  Future<CellMap> mapping() async => CellMap.defaults();
  @override
  Stream<DeviceStatus> liveStatus() => updates.stream;
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
  setUp(() {
    SharedPreferences.setMockInitialValues({});
    FlutterSecureStorage.setMockInitialValues({});
  });
  test('USB intent and Disconnect never dispatch network reads, writes, or discovery', () async {
    var requests = 0, discovery = 0;
    final api = PakarApi(
      client: MockClient((_) async {
        ++requests;
        return http.Response('{"hostname":"ledboard"}', 200);
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
    'Lost post-ACK readback stops bulk after one write and revokes controls',
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
      expect(find.text('No acknowledged board'), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
}
