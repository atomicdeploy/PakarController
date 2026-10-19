import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/table_workspace.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/connection_preferences.dart';

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
    'Connection fields follow restored profile only while pristine and retain a bounded nonsecret draft',
    (tester) async {
      final api = LifecycleApi();
      final draft = ConnectionDraft();
      Widget view() => MaterialApp(
        home: Scaffold(
          body: SettingsPage(
            api: api,
            reconnect: () async {},
            active: false,
            draft: draft,
          ),
        ),
      );
      await tester.pumpWidget(view());
      final host = find.widgetWithText(TextField, 'Device hostname or URL');
      final user = find.widgetWithText(TextField, 'User');
      final password = find.widgetWithText(TextField, 'Password (optional)');
      api.baseUrl = 'http://saved-board.local';
      api.user = 'saved-user';
      api.password = 'secure-value';
      await tester.pumpWidget(view());
      expect(
        tester.widget<TextField>(host).controller!.text,
        'http://saved-board.local',
      );
      expect(tester.widget<TextField>(user).controller!.text, 'saved-user');
      expect(
        tester.widget<TextField>(password).controller!.text,
        'secure-value',
      );
      await tester.enterText(host, 'my-unsubmitted-board.local');
      await tester.enterText(password, 'unsaved-secret');
      api.baseUrl = 'http://renamed-board.local';
      api.password = 'new-secure-value';
      await tester.pumpWidget(view());
      expect(
        tester.widget<TextField>(host).controller!.text,
        'my-unsubmitted-board.local',
      );
      expect(
        tester.widget<TextField>(password).controller!.text,
        'unsaved-secret',
      );
      final encoded = jsonEncode(draft.snapshot());
      expect(encoded, contains('my-unsubmitted-board.local'));
      expect(encoded, isNot(contains('secret')));
      await tester.pumpWidget(const SizedBox());
      final restored = ConnectionDraft()..restore(jsonDecode(encoded));
      expect(restored.endpoint, 'my-unsubmitted-board.local');
      expect(api.baseUrl, 'http://renamed-board.local');
      expect(api.writes, 0);
      restored.dispose();
      draft.dispose();
      await api.close();
    },
  );
  testWidgets(
    'Front-panel drafts and diagnostic preferences survive disconnect, reboot and process replacement without replay',
    (tester) async {
      tester.view.physicalSize = const Size(1200, 1200);
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
      final workspace = tester
          .widget<TablePage>(find.byType(TablePage))
          .workspace!;
      workspace.pixels.probe = const PixelProbe(
        index: 479,
        group: true,
        rgb: 0x1234ab,
      );
      workspace.pixels.position =
          '479'; // Unsubmitted position draft is not a frame.
      workspace.pixels.vertical = 52;
      await tester.tap(find.text('Front panel').last);
      await tester.pumpAndSettle();
      final marquee = find.widgetWithText(TextField, 'Marquee text');
      await tester.enterText(marquee, 'RESTORE MY TEXT');
      workspace.frontPanel.repeat = 'interval';
      workspace.frontPanel.speed = 480;
      workspace.frontPanel.forceScroll = true;
      workspace.frontPanel.selectedCue = 'welcome';
      await tester.tap(find.byTooltip('Disconnect'));
      await tester.pumpAndSettle();
      final prefs = await SharedPreferences.getInstance();
      final cache = jsonDecode(prefs.getString('table_workspaces')!) as Map;
      final saved = (cache['boards'] as Map)['board-A'] as Map;
      expect((saved['front_panel'] as Map)['text'], 'RESTORE MY TEXT');
      expect((saved['pixel_diagnostic'] as Map)['rgb'], 0x1234ab);
      expect(cache['connection'], {'method': 'network', 'wanted': false});
      expect(saved.containsKey('cells'), false);
      expect(saved.containsKey('mode'), false);
      expect(saved.containsKey('boot_id'), false);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
      final next = LifecycleApi()
        ..healthyWs = true
        ..observed = peer('board-A', boot: 'reboot');
      await tester.pumpWidget(PakarControllerApp(api: next));
      await tester.pumpAndSettle();
      expect(next.connectionWanted, false);
      expect(next.reads, 0);
      expect(
        find.byType(FrontPanelPage),
        findsNothing,
      ); // Cached UI grants no capability.
      await tester.tap(find.byTooltip('Connect'));
      await tester.pumpAndSettle();
      expect(
        tester.widget<TextField>(marquee).controller!.text,
        'RESTORE MY TEXT',
      );
      expect(
        tester
            .widget<FrontPanelPage>(find.byType(FrontPanelPage))
            .workspace!
            .repeat,
        'interval',
      );
      await tester.tap(find.text('Table').last);
      await tester.pumpAndSettle();
      final restored = tester
          .widget<TablePage>(find.byType(TablePage))
          .workspace!;
      expect(restored.pixels.probe.index, 479);
      expect(restored.pixels.probe.group, true);
      expect(restored.pixels.probe.rgb, 0x1234ab);
      expect(restored.pixels.position, '479');
      expect(restored.pixels.vertical, 52);
      expect(restored.frontPanel.speed, 480);
      expect(restored.frontPanel.forceScroll, true);
      expect(restored.frontPanel.selectedCue, 'welcome');
      expect(api.writes + next.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  testWidgets(
    'Final unmount flushes edits and retained USB intent never falls back to Wi-Fi',
    (tester) async {
      final api = LifecycleApi()
        ..healthyWs = true
        ..observed = peer('board-A');
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Table').last);
      await tester.pumpAndSettle();
      final workspace = tester
          .widget<TablePage>(find.byType(TablePage))
          .workspace!;
      workspace.selection = {2, 80};
      workspace.query.text = '2,80';
      // No 350ms debounce or pause callback before the process is replaced.
      api.connectionMethod = ConnectionMethod.usb;
      api.disconnect();
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
      final prefs = await SharedPreferences.getInstance();
      final saved = jsonDecode(prefs.getString('table_workspaces')!) as Map;
      expect((saved['connection'] as Map)['method'], 'usb');
      expect(((saved['boards'] as Map)['board-A'] as Map)['selection'], [
        2,
        80,
      ]);
      final next = LifecycleApi();
      await tester.pumpWidget(PakarControllerApp(api: next));
      await tester.pumpAndSettle();
      expect(next.connectionMethod, ConnectionMethod.usb);
      expect(next.connectionWanted, false);
      expect(next.reads, 0);
      expect(next.subscriptions, 0);
      expect(next.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  testWidgets(
    'Edits autosave before background and restore indicators, selection and both scroll axes after process replacement',
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
      final workspace = tester
          .widget<TablePage>(find.byType(TablePage))
          .workspace!;
      workspace.pageScroll.jumpTo(200);
      await tester.pumpAndSettle();
      final indicators = find.widgetWithText(FilterChip, 'LED indicators');
      await tester.tap(indicators);
      await tester.pumpAndSettle();
      final multiple = find.widgetWithText(FilterChip, 'Multiple selection');
      await tester.tap(multiple);
      await tester.pumpAndSettle();
      workspace.cellEffect = 'fade';
      await showCanvas(tester);
      workspace.pageScroll.jumpTo(
        workspace.pageScroll.position.maxScrollExtent - 10,
      );
      workspace.tableScroll.jumpTo(320);
      await tester.pump(const Duration(milliseconds: 500));
      await tester.pumpAndSettle();
      final y = workspace.pageScroll.offset, x = workspace.tableScroll.offset;
      final prefs = await SharedPreferences.getInstance();
      final cache = jsonDecode(prefs.getString('table_workspaces')!) as Map;
      final saved = (cache['boards'] as Map)['board-A'] as Map;
      expect(cache['activity'], 'table');
      expect(saved['selection'], [1, 2, 3, 80]);
      expect(saved['indicators'], false);
      expect(saved['multiple'], true);
      expect(saved['effect'], 'fade');
      expect(saved['vertical'], y);
      expect(saved['horizontal'], x);
      // No pause callback: model an OS kill after the latest autosave.
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
      final next = LifecycleApi()
        ..healthyWs = true
        ..observed = peer('board-A', boot: 'after-process-death');
      await tester.pumpWidget(PakarControllerApp(api: next));
      await tester.pumpAndSettle();
      final restored = tester
          .widget<TablePage>(find.byType(TablePage))
          .workspace!;
      expect(restored.showIndicators, false);
      expect(restored.multiple, true);
      expect(restored.cellEffect, 'fade');
      expect(restored.selection, {1, 2, 3, 80});
      expect(queryText(tester), '1-3,80');
      expect(restored.pageScroll.offset, y);
      expect(restored.tableScroll.offset, x);
      expect(selected(tester, 80), true);
      expect(api.writes + next.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  testWidgets(
    'Resume promptly retires a silent old socket with a read-only HTTP fallback',
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
      final workspace = tester
          .widget<TablePage>(find.byType(TablePage))
          .workspace!;
      workspace.showIndicators = true;
      final sockets = api.subscriptions;
      final reads = api.reads;
      addTearDown(() => resume(tester));
      pause(tester);
      api.healthyWs = false;
      await tester.pump(const Duration(minutes: 2));
      expect(api.subscriptions, sockets);
      resume(tester);
      await tester.pump();
      expect(api.probes, 1);
      await tester.pump(const Duration(milliseconds: 751));
      // Broadcast subscription cancellation completes on the real event loop,
      // outside the widget binding's synthetic resume clock.
      await tester.runAsync(() => Future<void>.delayed(Duration.zero));
      await tester.pumpAndSettle();
      expect(api.subscriptions, sockets + 1);
      expect(api.reads, reads + 1);
      expect(workspace.selection, {4, 5, 6});
      expect(workspace.showIndicators, true);
      api.healthyWs = true;
      api.updates.add(api.observed);
      await tester.pumpAndSettle();
      expect(red(tester).onPressed, isNotNull);
      expect(api.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );
  testWidgets('Colour controls precede the separate selection row on phones', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(390, 1000);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final api = LifecycleApi()..healthyWs = true;
    await tester.pumpWidget(PakarControllerApp(api: api));
    await tester.pumpAndSettle();
    await tester.tap(find.text('Table').last);
    await tester.pumpAndSettle();
    final colours = find.byKey(const ValueKey('table-colour-actions'));
    final selection = find.byKey(const ValueKey('table-selection-actions'));
    expect(
      tester.getRect(colours).bottom,
      lessThanOrEqualTo(tester.getRect(selection).top),
    );
    expect(
      find.descendant(of: colours, matching: find.text('Set red')),
      findsOneWidget,
    );
    expect(
      find.descendant(of: selection, matching: find.text('Select all')),
      findsOneWidget,
    );
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
    await tester.pump();
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
      expect(tester.widget<TablePage>(find.byType(TablePage)).connected, false);
      api.failReads = false;
      api.healthyWs = true;
      api.updates.add(api.observed);
      await tester.pumpAndSettle();
      expect(selected(tester, 5), true);
      expect(tester.widget<TablePage>(find.byType(TablePage)).connected, true);
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
  testWidgets(
    'Reattaching interrupted scrolls uses current offsets, not scroll-end defaults',
    (tester) async {
      final state = TableWorkspace();
      Widget view() => MaterialApp(
        home: Scaffold(
          body: ListView(
            controller: state.pageScroll,
            children: [
              SizedBox(
                height: 2000,
                child: SingleChildScrollView(
                  controller: state.tableScroll,
                  scrollDirection: Axis.horizontal,
                  child: const SizedBox(width: 2000),
                ),
              ),
            ],
          ),
        ),
      );
      await tester.pumpWidget(view());
      state.pageScroll.jumpTo(225);
      state.tableScroll.jumpTo(310);
      await tester.pump();
      await tester.pumpWidget(const SizedBox());
      await tester.pumpWidget(view());
      await tester.pump();
      expect(state.pageScroll.offset, 225);
      expect(state.tableScroll.offset, 310);
      await tester.pumpWidget(const SizedBox());
      state.dispose();
    },
  );
  test(
    'Workspace writer serializes and coalesces writes to the latest snapshot',
    () async {
      final calls = <String>[];
      final waits = <Completer<bool>>[];
      final writer = WorkspaceCacheWriter(
        write: (value) {
          calls.add(value);
          final pending = Completer<bool>();
          waits.add(pending);
          return pending.future;
        },
      );
      final first = writer.save('first');
      final middle = writer.save('intermediate');
      final last = writer.save('last');
      expect(calls, ['first']);
      waits[0].complete(true);
      await Future<void>.delayed(Duration.zero);
      expect(calls, ['first', 'last']);
      waits[1].complete(true);
      await Future.wait([first, middle, last]);
      await writer.save('last');
      expect(calls, ['first', 'last']);
      final next = writer.save('next');
      expect(calls, ['first', 'last', 'next']);
      waits[2].complete(true);
      await next;
      expect(writer.saved, 'next');
    },
  );
  test(
    'Workspace save failure keeps retry possible and never resets user state',
    () async {
      var attempts = 0;
      final writer = WorkspaceCacheWriter(write: (_) async => ++attempts > 1);
      await expectLater(writer.save('selection'), throwsStateError);
      expect(writer.saved, '');
      await writer.save('selection');
      expect(writer.saved, 'selection');
      expect(attempts, 2);
    },
  );
  test(
    'Every user choice notifies, but caret-only edits do not dirty persistence',
    () {
      final state = TableWorkspace();
      var edits = 0;
      state.addListener(() => edits++);
      state.selection = {1, 80};
      state.multiple = true;
      state.showIndicators = false;
      state.cellEffect = 'fade';
      state.query.text = '1,80';
      expect(edits, 5);
      state.query.selection = const TextSelection.collapsed(offset: 1);
      state.showIndicators = false;
      state.selection = {80, 1};
      expect(edits, 5);
      expect(() => state.selection.add(2), throwsUnsupportedError);
      state.dispose();
    },
  );
}
