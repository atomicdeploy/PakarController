import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/table_workspace.dart';

import 'connection_lifecycle_test.dart' show LifecycleApi;
import 'peer_fixture.dart';

DeviceStatus peer(String board, {String boot = 'first'}) => DeviceStatus({
  ...peerFixture().json,
  'board_id': board,
  'boot_id': boot,
  'led_output': {'enabled': true, 'mode': 'static'},
});
Finder get canvas => find.byKey(const ValueKey('table-cell-canvas'));
Finder get search => find.widgetWithText(TextField, 'Find logical cell');
FilledButton red(WidgetTester tester) =>
    tester.widget<FilledButton>(find.widgetWithText(FilledButton, 'Set red'));
bool selected(WidgetTester tester, int id) =>
    tester.widget<CellButton>(find.byKey(ValueKey('cell-$id'))).highlighted;
String queryText(WidgetTester tester) =>
    tester.widget<TablePage>(find.byType(TablePage)).workspace!.query.text;
Future<void> showCanvas(WidgetTester tester) async {
  await tester.scrollUntilVisible(
    canvas,
    250,
    scrollable: find
        .descendant(
          of: find.byKey(const PageStorageKey('table-page-scroll')),
          matching: find.byType(Scrollable),
        )
        .first,
  );
  await tester.pumpAndSettle();
}

void resume(WidgetTester tester) {
  if (tester.binding.lifecycleState == AppLifecycleState.paused) {
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.hidden);
  }
  if (tester.binding.lifecycleState == AppLifecycleState.hidden) {
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.inactive);
  }
  if (tester.binding.lifecycleState != AppLifecycleState.resumed) {
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
  }
}

void pause(WidgetTester tester) {
  resume(tester); // A fresh widget binding can start with no lifecycle state.
  if (tester.binding.lifecycleState == AppLifecycleState.resumed) {
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.inactive);
  }
  if (tester.binding.lifecycleState == AppLifecycleState.inactive) {
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.hidden);
  }
  if (tester.binding.lifecycleState == AppLifecycleState.hidden) {
    tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.paused);
  }
}

void main() {
  setUp(() {
    SharedPreferences.setMockInitialValues({});
    FlutterSecureStorage.setMockInitialValues({});
  });
  testWidgets(
    'Background/resume retains selection, both scroll axes and canvas without reads or reconnect',
    (tester) async {
      tester.view.physicalSize = const Size(390, 850);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = LifecycleApi()
        ..healthyWs = true
        ..observed = peer('board-A');
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Table').last);
      await tester.pumpAndSettle();
      await tester.enterText(search, '1-3,80');
      await tester.pumpAndSettle();
      await showCanvas(tester);
      final vertical = tester
          .widget<ListView>(
            find.byKey(const PageStorageKey('table-page-scroll')),
          )
          .controller!;
      final horizontal = tester
          .widget<SingleChildScrollView>(
            find.byKey(const ValueKey('table-cell-scroll')),
          )
          .controller!;
      vertical.jumpTo(vertical.position.maxScrollExtent - 10);
      horizontal.jumpTo(320);
      await tester.pumpAndSettle();
      final y = vertical.offset,
          x = horizontal.offset,
          bounds = tester.getRect(canvas);
      final reads = api.reads,
          maps = api.mappingReads,
          sockets = api.subscriptions;
      addTearDown(() => resume(tester));
      pause(tester);
      api.healthyWs = false;
      await tester.pump(const Duration(seconds: 30));
      await tester.pump();
      expect(api.reads, reads);
      expect(api.mappingReads, maps);
      expect(api.subscriptions, sockets);
      expect(selected(tester, 1), true);
      expect(selected(tester, 80), true);
      expect(red(tester).onPressed, isNull);
      resume(tester);
      await tester.pump(const Duration(milliseconds: 50));
      expect(api.probes, 1);
      expect(api.reads, reads);
      expect(api.subscriptions, sockets);
      expect(tester.getRect(canvas), bounds);
      api.healthyWs = true;
      api.updates.add(api.observed);
      await tester.pumpAndSettle();
      expect(vertical.offset, y);
      expect(horizontal.offset, x);
      expect(tester.getRect(canvas), bounds);
      expect(queryText(tester), '1-3,80');
      expect(selected(tester, 2), true);
      expect(red(tester).onPressed, isNotNull);
      expect(api.writes, 0);
      expect(api.reads, reads);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  testWidgets(
    'Lease expiry disables writes but preserves layout; recovery and explicit reconnect keep selection',
    (tester) async {
      final api = LifecycleApi()
        ..healthyWs = true
        ..observed = peer('board-A');
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Table').last);
      await tester.pumpAndSettle();
      await tester.enterText(search, '4-6');
      await tester.pumpAndSettle();
      await showCanvas(tester);
      final before = tester.getRect(canvas);
      api.failReads = true;
      api.healthyWs = false;
      await tester.pump(const Duration(seconds: 7));
      await tester.pumpAndSettle();
      expect(canvas, findsOneWidget);
      expect(tester.getRect(canvas), before);
      expect(selected(tester, 5), true);
      expect(red(tester).onPressed, isNull);
      api.failReads = false;
      api.healthyWs = true;
      api.updates.add(api.observed);
      await tester.pumpAndSettle();
      expect(selected(tester, 5), true);
      expect(red(tester).onPressed, isNotNull);
      await tester.tap(find.byTooltip('Disconnect'));
      await tester.pumpAndSettle();
      await tester.tap(find.byTooltip('Connect'));
      await tester.pumpAndSettle();
      expect(selected(tester, 5), true);
      expect(queryText(tester), '4-6');
      expect(api.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  testWidgets(
    'Workspace follows board identity, survives reboot and restores after background save/relaunch',
    (tester) async {
      final api = LifecycleApi()
        ..healthyWs = true
        ..observed = peer('board-A');
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Table').last);
      await tester.pumpAndSettle();
      await tester.enterText(search, '1,20,80');
      await tester.pumpAndSettle();
      await showCanvas(tester);
      api.observed = peer('board-A', boot: 'second');
      api.updates.add(api.observed);
      await tester.pumpAndSettle();
      expect(selected(tester, 20), true);
      api.observed = peer('board-B');
      api.updates.add(api.observed);
      await tester.pumpAndSettle();
      await showCanvas(tester);
      expect(selected(tester, 20), false);
      expect(queryText(tester), '');
      api.observed = peer('board-A', boot: 'second');
      api.updates.add(api.observed);
      await tester.pumpAndSettle();
      expect(selected(tester, 20), true);
      addTearDown(() => resume(tester));
      pause(tester);
      await tester.pumpAndSettle();
      final prefs = await SharedPreferences.getInstance();
      final cache = jsonDecode(prefs.getString('table_workspaces')!) as Map;
      expect((cache['boards'] as Map).length, 2);
      expect(cache['activity'], 'table');
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
      resume(tester);
      final next = LifecycleApi()
        ..healthyWs = true
        ..observed = peer('board-A', boot: 'third');
      await tester.pumpWidget(PakarControllerApp(api: next));
      await tester.pumpAndSettle();
      expect(selected(tester, 80), true);
      expect(selected(tester, 20), true);
      expect(queryText(tester), '1,20,80');
      expect(next.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  test('Workspace cache bounds invalid IDs, offsets and input without any device state', () {
    final state = TableWorkspace(
      saved: {
        'selection': [0, 81],
        'query': '1',
        'vertical': double.nan,
        'horizontal': -5,
      },
    );
    expect(state.selection, isEmpty);
    expect(state.snapshot()['vertical'], 0);
    expect(state.snapshot()['horizontal'], 0);
    expect(state.snapshot().keys, isNot(contains('boot_id')));
    state.dispose();
  });
}
