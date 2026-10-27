import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/device_settings.dart';
import 'package:pakarcontroller/device_settings_page.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';
import 'settings_fixture.dart';
import 'firmware_test.dart' show firmwareJob;

class SettingsFirmwareApi extends SettingsApi {
  int probes = 0;
  final probeReply = Completer<Map<String, dynamic>>();
  @override
  Future<Map<String, dynamic>> maintenanceJson(
    String path, {
    bool post = false,
    bool mayMutate = false,
    Map<String, String> form = const {},
    Uint8List? upload,
    String? uploadField,
    void Function(int done, int total)? uploadProgress,
    required bool Function() authorize,
  }) async {
    if (!authorize()) throw const StaleConnection();
    if (path == '/api/firmware/remote') return firmwareJob();
    ++probes;
    return probeReply.future;
  }
}

class SettingsApi extends PakarApi {
  Map<String, dynamic> cloud = Map.of(simulatedCloud);
  Map<String, dynamic> access = {
    'ready': true,
    'enabled': true,
    'default_credentials': false,
    'username': 'fixture',
  };
  Map<String, String>? accessWrite;
  int writes = 0;
  Completer<Map<String, dynamic>>? delayedAck;
  @override
  Future<Map<String, dynamic>> getJson(String path) async => switch (path) {
    '/api/network' => Map.of(simulatedNetwork),
    '/api/cloud' => Map.of(cloud),
    '/api/device/name' => {
      'hostname': 'ledboard',
      'display_name': 'Table board',
    },
    '/api/auth/info' => Map.of(access),
    '/api/wifi/scan' => {
      'networks': [
        {'ssid': 'Fixture SSID'},
      ],
    },
    _ => {},
  };
  @override
  Future<Map<String, dynamic>> postJson(
    String path,
    Map<String, String> values, {
    bool Function()? authorize,
  }) async {
    if (authorize?.call() == false) throw const StaleConnection();
    final owner = connectionRevision;
    ++writes;
    if (path == '/api/auth') accessWrite = Map.of(values);
    if (delayedAck != null) {
      final ack = await delayedAck!.future;
      if (owner != connectionRevision) throw const StaleConnection();
      if (path == '/api/auth' && ack['enabled'] is bool) {
        access['enabled'] = ack['enabled'];
        if (values['username']?.isNotEmpty == true) {
          access['username'] = values['username'];
        }
      }
      return ack;
    }
    if (path == '/api/auth') {
      access['enabled'] = values['enabled'] == 'true';
      if (values['username']?.isNotEmpty == true) {
        access['username'] = values['username'];
      }
      return {
        'updated': true,
        'enabled': access['enabled'],
        'reauthenticate': access['enabled'],
      };
    }
    for (final field in SettingsGroup.cloud.fields.entries) {
      final value = values[field.key];
      if (value == null) continue;
      cloud[field.key] = field.value == 'boolean'
          ? value == 'true'
          : field.value == 'port' || field.value == 'interval'
          ? int.parse(value)
          : value;
    }
    return {'updated': true};
  }
}

Widget fixture(
  SettingsApi api,
  DeviceStatus status, {
  bool Function(String)? authorize,
  Future<void> Function(String, String, String)? adopt,
  void Function(String)? navigate,
}) => MaterialApp(
  theme: ThemeData.dark(useMaterial3: true),
  home: Scaffold(
    body: DeviceSettingsPage(
      api: api,
      status: status,
      authorized: authorize ?? status.peer.has,
      navigate: navigate ?? (_) {},
      adoptConnection: adopt ?? (_, _, _) async {},
    ),
  ),
);

void main() {
  for (final initiallyEnabled in [false, true]) {
    testWidgets(
      'Authentication toggle honors current-password rules: $initiallyEnabled',
      (tester) async {
        tester.view.physicalSize = const Size(1200, 1400);
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);
        final api = SettingsApi()..access['enabled'] = initiallyEnabled;
        api.password = initiallyEnabled ? 'connection-password' : '';
        final adopted = <(String, String, String)>[];
        await tester.pumpWidget(
          fixture(
            api,
            peerFixture(),
            adopt: (url, user, password) async =>
                adopted.add((url, user, password)),
          ),
        );
        await tester.pumpAndSettle();
        await tester.tap(find.widgetWithText(ListTile, 'Device access'));
        await tester.pumpAndSettle();
        Finder field(String label) => find.byWidgetPredicate(
          (widget) =>
              widget is TextField && widget.decoration?.labelText == label,
        );
        expect(
          field('Current password'),
          initiallyEnabled ? findsOneWidget : findsNothing,
        );
        expect(
          field('New password'),
          initiallyEnabled ? findsOneWidget : findsNothing,
        );
        await tester.tap(find.byKey(const ValueKey('device-access-enabled')));
        await tester.pumpAndSettle();
        if (!initiallyEnabled) {
          expect(field('Current password'), findsNothing);
          await tester.enterText(field('New password'), 'new-fixture-password');
          await tester.enterText(
            field('Confirm new password'),
            'new-fixture-password',
          );
        } else {
          expect(field('New password'), findsNothing);
        }
        await tester.tap(find.text('Save device access'));
        await tester.pumpAndSettle();
        await tester.tap(find.widgetWithText(FilledButton, 'Continue'));
        await tester.pumpAndSettle();
        expect(api.accessWrite, {
          'enabled': '${!initiallyEnabled}',
          'current_password': initiallyEnabled ? 'connection-password' : '',
          'username': initiallyEnabled ? '' : 'fixture',
          'password': initiallyEnabled ? '' : 'new-fixture-password',
        });
        expect(adopted, [
          (
            api.baseUrl,
            initiallyEnabled ? '' : 'fixture',
            initiallyEnabled ? '' : 'new-fixture-password',
          ),
        ]);
        expect(api.writes, 1);
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
        await api.close();
      },
    );
  }
  testWidgets('Access metadata change preserves draft and prevents a write', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(1200, 1400);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final api = SettingsApi()..access['enabled'] = false;
    await tester.pumpWidget(fixture(api, peerFixture()));
    await tester.pumpAndSettle();
    await tester.tap(find.widgetWithText(ListTile, 'Device access'));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const ValueKey('device-access-enabled')));
    await tester.pumpAndSettle();
    Finder field(String label) => find.byWidgetPredicate(
      (widget) => widget is TextField && widget.decoration?.labelText == label,
    );
    await tester.enterText(field('New password'), 'new-fixture-password');
    await tester.enterText(
      field('Confirm new password'),
      'new-fixture-password',
    );
    api.access['username'] = 'another-owner';
    await tester.tap(find.text('Save device access'));
    await tester.pumpAndSettle();
    await tester.tap(find.widgetWithText(FilledButton, 'Continue'));
    await tester.pumpAndSettle();
    expect(api.writes, 0);
    expect(find.textContaining('Device access changed'), findsWidgets);
    expect(
      tester.widget<TextField>(field('New password')).controller!.text,
      'new-fixture-password',
    );
    await tester.pumpWidget(const SizedBox());
    await api.close();
  });
  for (final keyGame in [false, true]) {
    testWidgets(
      'LED settings opens the contextual board workspace: key game $keyGame',
      (tester) async {
        tester.view.physicalSize = const Size(1200, 1400);
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);
        final api = SettingsApi();
        final original = peerFixture();
        final status = DeviceStatus({
          ...original.json,
          'capabilities': {
            ...original.json['capabilities'] as Map,
            'role': keyGame ? 'first-wins' : 'table-selection',
          },
        });
        final navigation = <String>[];
        await tester.pumpWidget(fixture(api, status, navigate: navigation.add));
        await tester.pumpAndSettle();
        await tester.tap(find.widgetWithText(ListTile, 'LEDs & cells'));
        await tester.pumpAndSettle();
        await tester.tap(find.text('Open live controls'));
        expect(navigation, [keyGame ? 'output' : 'table']);
        expect(api.writes, 0);
        await tester.pumpWidget(const SizedBox());
        await api.close();
      },
    );
  }
  testWidgets(
    'Firmware ACK revocation releases only the destroyed view lock; late probe does not resume',
    (tester) async {
      tester.view.physicalSize = const Size(1200, 1800);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = SettingsFirmwareApi();
      await tester.pumpWidget(
        fixture(api, peerFixture(features: ['network', 'identity', 'isp'])),
      );
      await tester.pumpAndSettle();
      await tester.tap(find.widgetWithText(ListTile, 'Firmware & storage'));
      await tester.pumpAndSettle();
      await tester.ensureVisible(find.text('Probe ISP'));
      await tester.tap(find.text('Probe ISP'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Confirm'));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 50));
      expect(api.probes, 1);
      await tester.pumpWidget(
        fixture(api, peerFixture(features: ['network', 'identity'])),
      );
      await tester.pumpAndSettle();
      expect(find.text('Firmware & storage'), findsNothing);
      expect(
        tester
            .widget<ListTile>(find.widgetWithText(ListTile, 'Wi-Fi & Ethernet'))
            .onTap,
        isNotNull,
      );
      api.probeReply.complete({'signature': '1E950F'});
      await tester.pumpAndSettle();
      expect(api.probes, 1);
      expect(find.textContaining('ISP signature confirmed'), findsNothing);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      await api.close();
    },
  );
  testWidgets(
    'Searchable catalog and real fields adapt to phone and tablet, no overflow',
    (tester) async {
      for (final size in [const Size(390, 840), const Size(1200, 900)]) {
        tester.view.physicalSize = size;
        tester.view.devicePixelRatio = 1;
        final api = SettingsApi();
        await tester.pumpWidget(fixture(api, peerFixture()));
        await tester.pumpAndSettle();
        expect(find.text('Device settings'), findsOneWidget);
        await tester.tap(
          size.width < 760
              ? find.widgetWithText(ChoiceChip, 'MQTT')
              : find.widgetWithText(ListTile, 'MQTT'),
        );
        await tester.pumpAndSettle();
        expect(find.text('MQTT broker'), findsOneWidget);
        expect(
          find.byKey(const ValueKey('SettingsGroup.cloud:mqtt_host')),
          findsOneWidget,
        );
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
        await tester.pump();
        await api.close();
      }
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
    },
  );
  testWidgets(
    'Dirty MQTT draft survives section switch, explicit save verifies public readback',
    (tester) async {
      tester.view.physicalSize = const Size(1200, 1200);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = SettingsApi();
      await tester.pumpWidget(fixture(api, peerFixture()));
      await tester.pumpAndSettle();
      await tester.tap(find.widgetWithText(ListTile, 'MQTT'));
      await tester.pumpAndSettle();
      final field = find.byKey(const ValueKey('SettingsGroup.cloud:mqtt_user'));
      await tester.enterText(field, 'edited-user');
      await tester.enterText(
        find.byKey(const ValueKey('SettingsGroup.cloud:mqtt_port')),
        '۸۸۸٣',
      );
      await tester.pump();
      await tester.tap(find.widgetWithText(ListTile, 'WebSocket'));
      await tester.pumpAndSettle();
      await tester.tap(find.widgetWithText(ListTile, 'MQTT'));
      await tester.pumpAndSettle();
      expect(tester.widget<TextField>(field).controller!.text, 'edited-user');
      expect(api.writes, 0);
      await tester.ensureVisible(find.text('Save and verify'));
      await tester.tap(find.text('Save and verify'));
      await tester.pumpAndSettle();
      expect(api.writes, 1);
      expect(
        find.textContaining('verified by device readback'),
        findsOneWidget,
      );
      expect(api.cloud['mqtt_user'], 'edited-user');
      expect(api.cloud['mqtt_port'], 8883);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
      await api.close();
    },
  );
  testWidgets(
    'Missing network ACK hides Wi-Fi/cloud/access, live ACK loss revokes draft',
    (tester) async {
      tester.view.physicalSize = const Size(1200, 900);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final api = SettingsApi();
      await tester.pumpWidget(fixture(api, peerFixture()));
      await tester.pumpAndSettle();
      await tester.tap(find.widgetWithText(ListTile, 'MQTT'));
      await tester.pumpAndSettle();
      await tester.enterText(
        find.byKey(const ValueKey('SettingsGroup.cloud:mqtt_user')),
        'private-draft',
      );
      await tester.pumpWidget(
        fixture(api, peerFixture(features: ['identity'])),
      );
      await tester.pumpAndSettle();
      expect(find.text('MQTT'), findsNothing);
      expect(find.text('Wi-Fi'), findsNothing);
      expect(find.text('Device access'), findsNothing);
      expect(find.text('Board identity'), findsOneWidget);
      expect(api.writes, 0);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      await tester.pump();
      await api.close();
    },
  );
  for (final route in ['access', 'identity']) {
    for (final changedOwner in [false, true]) {
      testWidgets(
        'Acknowledged $route handoff after old view closes: changed owner $changedOwner',
        (tester) async {
          tester.view.physicalSize = const Size(1200, 1400);
          tester.view.devicePixelRatio = 1;
          addTearDown(tester.view.resetPhysicalSize);
          addTearDown(tester.view.resetDevicePixelRatio);
          final api = SettingsApi()..delayedAck = Completer();
          final adopted = <(String, String, String)>[];
          await tester.pumpWidget(
            fixture(
              api,
              peerFixture(),
              adopt: (url, user, password) async {
                adopted.add((url, user, password));
              },
            ),
          );
          await tester.pumpAndSettle();
          await tester.tap(
            find.widgetWithText(
              ListTile,
              route == 'access' ? 'Device access' : 'Board identity',
            ),
          );
          await tester.pumpAndSettle();
          Finder field(String label) => find.byWidgetPredicate(
            (widget) =>
                widget is TextField && widget.decoration?.labelText == label,
          );
          if (route == 'access') {
            await tester.enterText(
              field('Current password'),
              'old-fixture-pass',
            );
            await tester.enterText(field('New password'), 'new-fixture-pass');
            await tester.enterText(
              field('Confirm new password'),
              'new-fixture-pass',
            );
          } else {
            await tester.enterText(
              field('Hostname · optional .local'),
              'renamed',
            );
          }
          final save = find.text(
            route == 'access' ? 'Save device access' : 'Save identity',
          );
          await tester.ensureVisible(save);
          await tester.tap(save);
          await tester.pumpAndSettle();
          await tester.tap(find.widgetWithText(FilledButton, 'Continue'));
          await tester.pump();
          await tester.pump(const Duration(milliseconds: 50));
          expect(api.writes, 1);
          // Device may close its old WebSocket before delivering a successful
          // auth/rename HTTP response. The owning root can still follow that ACK.
          await tester.pumpWidget(const SizedBox());
          if (changedOwner) api.baseUrl = 'http://different.local';
          api.delayedAck!.complete(
            route == 'access'
                ? {'updated': true, 'enabled': true, 'reauthenticate': true}
                : {
                    'accepted': true,
                    'persisted': true,
                    'hostname': 'renamed',
                    'display_name': 'Table board',
                    'apply_pending': true,
                  },
          );
          await tester.pumpAndSettle();
          expect(adopted.length, changedOwner ? 0 : 1);
          if (!changedOwner) {
            expect(
              adopted.single,
              route == 'access'
                  ? ('http://ledboard.local', 'fixture', 'new-fixture-pass')
                  : ('http://renamed.local', api.user, api.password),
            );
          }
          expect(tester.takeException(), isNull);
          await api.close();
        },
      );
    }
  }
}
