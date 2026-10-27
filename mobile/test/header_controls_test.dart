import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/header_controls.dart';
import 'package:pakarcontroller/main.dart';

import 'connection_lifecycle_test.dart' show LifecycleApi;

void main() {
  setUp(() {
    SharedPreferences.setMockInitialValues({});
    FlutterSecureStorage.setMockInitialValues({});
  });

  test(
    'Header preferences sanitize, reorder and preserve all-hidden choices',
    () {
      final layout = HeaderLayout.restore({
        'order': ['settings', 'settings', 'unknown', 'connection'],
        'enabled': [],
      });
      expect(layout.order, [
        HeaderAction.settings,
        HeaderAction.connection,
        HeaderAction.refresh,
        HeaderAction.devices,
      ]);
      expect(layout.enabled, isEmpty);
      final edited = layout.toggle(HeaderAction.devices, true).reorder(3, 0);
      expect(
        HeaderLayout.restore(edited.snapshot()).snapshot(),
        edited.snapshot(),
      );
      expect(edited.order.first, HeaderAction.devices);
      expect(HeaderLayout.restore(null).enabled, {
        HeaderAction.connection,
        HeaderAction.refresh,
      });
    },
  );

  test('Refresh is stale-only and connection icon describes the action', () {
    final layout = HeaderLayout();
    expect(
      layout.available(wanted: true, fresh: true, settingsAvailable: false),
      [HeaderAction.connection],
    );
    expect(
      layout.available(wanted: true, fresh: false, settingsAvailable: false),
      [HeaderAction.connection, HeaderAction.refresh],
    );
    expect(
      layout.available(wanted: false, fresh: false, settingsAvailable: false),
      [HeaderAction.connection],
    );
    expect(connectionActionIcon(wanted: true, fresh: true), Icons.link_off);
    expect(connectionActionIcon(wanted: false, fresh: false), Icons.link);
    expect(
      connectionActionLabel(wanted: true, fresh: false),
      'Cancel connection',
    );
  });

  testWidgets(
    'Healthy header hides refresh; status opens retained connection dialog without commands',
    (tester) async {
      final api = LifecycleApi()..healthyWs = true;
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      expect(find.byKey(const ValueKey('header-refresh')), findsNothing);
      expect(find.byTooltip('Disconnect'), findsOneWidget);
      expect(find.text('Live device status received'), findsNothing);
      expect(
        find.descendant(
          of: find.byKey(const Key('connection-status-area')),
          matching: find.text('Connected'),
        ),
        findsOneWidget,
      );
      await tester.tap(find.byKey(const Key('connection-status-area')));
      await tester.pumpAndSettle();
      expect(find.byType(Dialog), findsOneWidget);
      expect(find.text('Connection method'), findsWidgets);
      expect(api.writes, 0);
      await tester.tap(find.byTooltip('Close connection'));
      await tester.pumpAndSettle();
      await tester.tap(find.byTooltip('Disconnect'));
      await tester.pumpAndSettle();
      expect(find.byTooltip('Connect'), findsOneWidget);
      expect(find.byKey(const ValueKey('header-refresh')), findsNothing);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );

  testWidgets(
    'Header customization saves and restores without changing board state',
    (tester) async {
      var api = LifecycleApi()..healthyWs = true;
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      await tester.tap(find.byTooltip('Device context menu'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Configure header…'));
      await tester.pumpAndSettle();
      final connection = find.byKey(const ValueKey('connection'));
      await tester.tap(
        find.descendant(of: connection, matching: find.byType(Checkbox)),
      );
      await tester.tap(find.byTooltip('Move Connection'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Move down'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Save').last);
      await tester.pumpAndSettle();
      await tester.pump(const Duration(milliseconds: 400));
      expect(find.byKey(const ValueKey('header-connection')), findsNothing);
      final prefs = await SharedPreferences.getInstance();
      expect(
        jsonDecode(prefs.getString('table_workspaces')!)['header']['enabled'],
        ['refresh'],
      );
      expect(
        jsonDecode(prefs.getString('table_workspaces')!)['header']['order'],
        ['refresh', 'connection', 'devices', 'settings'],
      );
      expect(api.writes, 0);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
      api = LifecycleApi()..healthyWs = true;
      await tester.pumpWidget(PakarControllerApp(api: api));
      await tester.pumpAndSettle();
      expect(find.byKey(const ValueKey('header-connection')), findsNothing);
      await tester.tap(find.byTooltip('Device context menu'));
      await tester.pumpAndSettle();
      expect(find.text('Disconnect'), findsOneWidget);
      expect(find.text('Configure header…'), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
    },
  );

  testWidgets('Stale state exposes refresh and a new event hides it again', (
    tester,
  ) async {
    final api = LifecycleApi()..healthyWs = true;
    await tester.pumpWidget(PakarControllerApp(api: api));
    await tester.pumpAndSettle();
    api.failReads = true;
    api.healthyWs = false;
    await tester.pump(const Duration(seconds: 7));
    await tester.pump();
    expect(find.byKey(const ValueKey('header-refresh')), findsOneWidget);
    expect(find.byTooltip('Cancel connection'), findsOneWidget);
    api.healthyWs = true;
    api.failReads = false;
    api.updates.add(api.observed);
    await tester.pumpAndSettle();
    expect(find.byKey(const ValueKey('header-refresh')), findsNothing);
    expect(find.byTooltip('Disconnect'), findsOneWidget);
    expect(api.writes, 0);
    await tester.pumpWidget(const SizedBox());
    await tester.pump();
  });

  testWidgets('Header editor fits phone tablet RTL and large text', (
    tester,
  ) async {
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    for (final width in [320.0, 800.0]) {
      for (final direction in TextDirection.values) {
        tester.view.physicalSize = Size(width, 800);
        tester.view.devicePixelRatio = 1;
        await tester.pumpWidget(
          MaterialApp(
            home: MediaQuery(
              data: MediaQueryData(
                size: Size(width, 800),
                textScaler: TextScaler.linear(1.8),
              ),
              child: Directionality(
                textDirection: direction,
                child: HeaderLayoutEditor(initial: HeaderLayout()),
              ),
            ),
          ),
        );
        await tester.pump();
        expect(tester.takeException(), isNull);
      }
    }
    await tester.pumpWidget(const SizedBox());
  });
}
