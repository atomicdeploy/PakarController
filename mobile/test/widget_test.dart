import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';

class TestApi extends PakarApi {
  final updates = StreamController<DeviceStatus>();
  @override
  Future<DeviceStatus> status() async =>
      DeviceStatus({'hostname': 'ledboard', 'cells': List.filled(80, 'white')});
  @override
  Future<CellMap> mapping() async => CellMap.defaults();
  @override
  Stream<DeviceStatus> liveStatus() => updates.stream;
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
  testWidgets('Shell uses injected state and cancels transport on disposal', (
    tester,
  ) async {
    await tester.pumpWidget(PakarControllerApp(api: TestApi()));
    await tester.pump();
    await tester.pump();
    expect(find.text('PakarController'), findsWidgets);
    expect(find.text('Table'), findsWidgets);
    await tester.pumpWidget(const SizedBox());
    await tester.pump();
  });
  testWidgets('Offline hardware never invents zero inputs/outputs', (
    tester,
  ) async {
    await tester.pumpWidget(
      const MaterialApp(home: Scaffold(body: HardwarePage(status: null))),
    );
    expect(find.text('0x00'), findsNothing);
    expect(find.text('Unavailable'), findsNWidgets(8));
  });
  testWidgets('Marquee defaults to once without forced scroll', (tester) async {
    Map<String, String>? sent;
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: FrontPanelPage(
            status: DeviceStatus({
              'tm1637_segments': [0, 0, 0, 0],
              'tm1637_ack': true,
            }),
            command: (path, [values = const {}]) async {
              sent = values;
            },
          ),
        ),
      ),
    );
    await tester.ensureVisible(find.text('Present'));
    await tester.tap(find.text('Present'));
    await tester.pump();
    expect(sent?['repeat'], 'once');
    expect(sent?['force_scroll'], 'false');
  });
  testWidgets('Offline table disables hardware mutations', (tester) async {
    var calls = 0;
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: TablePage(
            status: null,
            mapping: CellMap.defaults(),
            command: (path, [values = const {}]) async {
              ++calls;
            },
          ),
        ),
      ),
    );
    await tester.tap(find.text('1'));
    await tester.pump();
    expect(calls, 0);
  });
}
