import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/device_registry.dart';
import 'package:pakarcontroller/devices_page.dart';
import 'package:pakarcontroller/discovery.dart';

import 'peer_fixture.dart';

class MemoryRegistry extends DeviceRegistry {
  MemoryRegistry(this.devices);
  final List<SavedDevice> devices;
  final secrets = <String, String>{};
  @override
  Future<List<SavedDevice>> load() async => devices.toList();
  @override
  Future<SavedDevice> save(SavedDevice device, String? password) async {
    devices.removeWhere((d) => d.id == device.id);
    devices.add(device);
    if (password != null) secrets[device.id] = password;
    return device;
  }

  @override
  Future<String?> password(SavedDevice device) async => secrets[device.id];
}

MemoryRegistry directoryFixture() => MemoryRegistry([
  SavedDevice(
    id: 'table',
    name: 'Studio table',
    endpoint: 'ledboard.local',
    location: DeviceLocation.local,
    addresses: {'192.0.2.4'},
    boardId: 'board-one',
  ),
  SavedDevice(
    id: 'remote',
    name: 'Workshop',
    endpoint: 'https://workshop.example',
    location: DeviceLocation.remote,
  ),
]);
DeviceDiscovery directoryDiscovery() => DeviceDiscovery(
  catalog: DiscoveryCatalog(),
  advertisements: () async => [
    const DiscoveredDevice(
      'Table',
      'http://192.0.2.4',
      'SSDP',
      boardId: 'board-one',
      verified: true,
      authRequired: true,
    ),
    const DiscoveredDevice(
      'Table',
      'http://192.0.2.4:80',
      'MNDP',
      boardId: 'board-one',
      mac: '02:11:22:33:44:55',
      verified: true,
      authRequired: true,
    ),
    const DiscoveredDevice(
      'Entry display',
      'http://192.0.2.5',
      'mDNS / DNS-SD',
      boardId: 'board-two',
      verified: true,
    ),
  ],
  verify: (d) async => d.verified ? d : null,
);

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  test('Actual registry accepts optional secrets, preserves on edit and coalesces profiles', () async {
    SharedPreferences.setMockInitialValues({});
    FlutterSecureStorage.setMockInitialValues({});
    final registry = DeviceRegistry();
    final first = SavedDevice(
      id: 'one',
      name: 'Table',
      endpoint: 'ledboard.local',
      location: DeviceLocation.local,
      addresses: {'192.0.2.4'},
      boardId: 'board-one',
    );
    await registry.save(first, '');
    expect(await registry.password(first), '');
    await registry.save(first, 'secret');
    final same = SavedDevice(
      id: 'two',
      name: 'My table',
      endpoint: 'http://192.0.2.4',
      location: DeviceLocation.local,
      boardId: 'board-one',
    );
    final updated = await registry.save(same, null);
    expect(updated.id, 'one');
    expect(await registry.load(), hasLength(1));
    expect(await registry.password(updated), 'secret');
    expect(
      (await SharedPreferences.getInstance()).getString(DeviceRegistry.key),
      isNot(contains('secret')),
    );
  });
  test('Identity/IP/MAC merge preserves channels and saved names, not duplicate rows', () async {
    final registry = directoryFixture(), discovery = directoryDiscovery();
    final found = await discovery.scan();
    final rows = unifiedDevices(
      await registry.load(),
      found,
      discovery.catalog,
    );
    expect(rows, hasLength(3));
    final table = rows.singleWhere((d) => d.name == 'Studio table');
    expect(table.available, true);
    expect(table.peer.protocol, contains('SSDP'));
    expect(table.peer.protocol, contains('MNDP'));
    expect(rows.singleWhere((d) => d.name == 'Workshop').available, false);
    expect(
      coalesceDevices([
        const DiscoveredDevice('a', 'http://192.0.2.4', 'SSDP', boardId: 'a'),
        const DiscoveredDevice('b', 'http://192.0.2.4', 'MNDP', boardId: 'b'),
      ]),
      hasLength(2),
      reason: 'DHCP reuse cannot override conflicting board IDs',
    );
    expect(
      coalesceDevices([
        const DiscoveredDevice(
          'a',
          'http://192.0.2.4',
          'SSDP',
          mac: '02:11:22:33:44:55',
        ),
        const DiscoveredDevice(
          'a',
          'http://192.0.2.9',
          'MNDP',
          mac: '021122334455',
        ),
      ]),
      hasLength(1),
    );
  });
  test(
    'Failed sweep retires availability but retains remembered device',
    () async {
      final catalog = DiscoveryCatalog();
      catalog.merge([
        const DiscoveredDevice('Table', 'http://192.0.2.4', 'SSDP'),
      ]);
      final failed = DeviceDiscovery(
        catalog: catalog,
        advertisements: () async => throw StateError('fixture'),
      );
      await expectLater(failed.scan(), throwsStateError);
      final known = catalog.merge([]).single;
      expect(catalog.seenThisScan(known), false);
    },
  );
  test(
    'Board identity never normalizes away its interface origins or saved route',
    () {
      final catalog = DiscoveryCatalog();
      const ap = 'http://192.168.4.1', sta = 'http://10.0.0.7:8080';
      const ethernet = 'https://10.0.0.7:8443',
          hostname = 'http://ledboard.local';
      catalog.merge([
        const DiscoveredDevice(
          'Table',
          hostname,
          'mDNS',
          boardId: 'board-one',
          verified: true,
        ),
      ]);
      final found = catalog.merge([
        const DiscoveredDevice(
          'Table',
          sta,
          'SSDP',
          boardId: 'board-one',
          aliases: {ap, hostname},
          verified: true,
        ),
        const DiscoveredDevice(
          'Table',
          ethernet,
          'MNDP',
          boardId: 'board-one',
          verified: true,
        ),
      ]);
      final entry = unifiedDevices([], found, catalog).single;
      expect(entry.peer.origins, containsAll([ap, sta, ethernet, hostname]));
      expect(entry.observedOrigins, {sta, ethernet});
      expect(
        entry.preferredOrigin(ConnectionMethod.network),
        sta,
        reason:
            'A historical hostname or unprobed AP alias is not a live route',
      );
      final saved = SavedDevice(
        id: 'table',
        name: 'My table',
        endpoint: ap,
        location: DeviceLocation.local,
        boardId: 'board-one',
      );
      final pinned = unifiedDevices([saved], found, catalog).single;
      expect(
        pinned.preferredOrigin(ConnectionMethod.network),
        ap,
        reason:
            'An explicit saved interface must not silently become STA/Ethernet',
      );
      expect(pinned.observedOrigins.contains(ap), false);
      catalog.merge([]);
      expect(catalog.observedOrigins(entry.peer), isEmpty);
      expect(
        coalesceDevices([
          entry.peer,
          const DiscoveredDevice(
            'Table',
            sta,
            'SSDP',
            boardId: 'different-board',
          ),
        ]),
        hasLength(2),
      );
    },
  );
  for (final changeRoute in [false, true]) {
    testWidgets('Exact interface selection survives discovery: $changeRoute', (
      tester,
    ) async {
      const ap = 'http://192.168.4.1', sta = 'http://10.0.0.7:8080';
      const hostname = 'http://ledboard.local';
      final registry = MemoryRegistry([
        SavedDevice(
          id: 'table',
          name: 'My table',
          endpoint: ap,
          location: DeviceLocation.local,
          boardId: 'board-one',
        ),
      ]);
      var scans = 0;
      final discovery = DeviceDiscovery(
        catalog: DiscoveryCatalog(),
        advertisements: () async {
          scans++;
          final peers = [
            for (final origin in [ap, sta, hostname])
              DiscoveredDevice(
                'Table',
                origin,
                'SSDP',
                boardId: 'board-one',
                verified: true,
              ),
          ];
          return scans.isEven ? peers.reversed.toList() : peers;
        },
        verify: (d) async => d,
      );
      final requests = <String>[], selected = <SavedDevice>[];
      await tester.pumpWidget(
        MaterialApp(
          theme: studioTheme(),
          home: Scaffold(
            body: DevicesPage(
              registry: registry,
              discovery: discovery,
              select: (d, _) async => selected.add(d),
              apiFactory: (d, _) => PakarApi(
                baseUrl: d.endpoint,
                client: MockClient((request) async {
                  requests.add(request.url.origin);
                  return http.Response(
                    jsonEncode({
                      ...peerFixture().json,
                      'board_id': 'board-one',
                      'boot_id': 'boot-one',
                    }),
                    200,
                  );
                }),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      final selector = find.byKey(
        const ValueKey('device-route:board:board-one'),
      );
      expect(tester.widget<DropdownButton<String>>(selector).value, ap);
      if (changeRoute) {
        await tester.tap(selector);
        await tester.pumpAndSettle();
        await tester.tap(find.text(sta).last);
        await tester.pumpAndSettle();
      }
      await tester.tap(find.widgetWithText(OutlinedButton, 'Discover'));
      await tester.pumpAndSettle();
      final expected = changeRoute ? sta : ap;
      expect(tester.widget<DropdownButton<String>>(selector).value, expected);
      await tester.tap(find.text('My table'));
      await tester.pumpAndSettle();
      expect(requests, [expected]);
      expect(selected.single.endpoint, expected);
      expect(selected.single.aliases, containsAll([ap, sta, hostname]));
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    });
  }
  for (final width in [390.0, 1200.0]) {
    testWidgets(
      'Unified device directory at $width, unavailable profiles dimmed',
      (tester) async {
        tester.view.physicalSize = Size(width, 1100);
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);
        await tester.pumpWidget(
          MaterialApp(
            theme: studioTheme(),
            home: Scaffold(
              body: DevicesPage(
                registry: directoryFixture(),
                discovery: directoryDiscovery(),
                select: (_, _) async {},
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        expect(find.text('Studio table'), findsOneWidget);
        expect(find.text('Nearby advertisements'), findsNothing);
        expect(find.textContaining('unverified'), findsNothing);
        final dim = find.ancestor(
          of: find.text('Workshop'),
          matching: find.byType(AnimatedOpacity),
        );
        expect(tester.widget<AnimatedOpacity>(dim).opacity, lessThan(1));
        await tester.tap(find.text('Add device'));
        await tester.pumpAndSettle();
        expect(
          find.widgetWithText(TextField, 'New password (optional)'),
          findsNothing,
        );
        expect(find.widgetWithText(TextField, 'Password'), findsNothing);
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      },
    );
  }
  for (final protected in [false, true]) {
    testWidgets('Credentials requested only after challenge: $protected', (
      tester,
    ) async {
      final registry = directoryFixture();
      var reads = 0, selections = 0;
      await tester.pumpWidget(
        MaterialApp(
          theme: studioTheme(),
          home: Scaffold(
            body: DevicesPage(
              registry: registry,
              discovery: directoryDiscovery(),
              select: (_, password) async {
                selections++;
                expect(password, protected ? 'fixture-secret' : '');
              },
              apiFactory: (d, password) => PakarApi(
                baseUrl: d.endpoint,
                user: d.user,
                password: password,
                client: MockClient((request) async {
                  reads++;
                  if (protected && password.isEmpty) {
                    expect(request.headers.containsKey('authorization'), false);
                    return http.Response('{}', 401);
                  }
                  if (!protected) {
                    expect(request.headers.containsKey('authorization'), false);
                  }
                  return http.Response(
                    jsonEncode({
                      ...peerFixture().json,
                      'board_id': 'board-one',
                      'boot_id': 'boot-one',
                    }),
                    200,
                  );
                }),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await tester.tap(find.text('Studio table'));
      await tester.pumpAndSettle();
      if (protected) {
        expect(selections, 0);
        expect(find.byType(AlertDialog), findsOneWidget);
        await tester.enterText(
          find.widgetWithText(TextField, 'Password'),
          'fixture-secret',
        );
        await tester.tap(find.widgetWithText(FilledButton, 'Sign in'));
        await tester.pumpAndSettle();
        await tester.pump(const Duration(milliseconds: 300));
        await tester.pumpAndSettle();
      } else {
        expect(find.byType(AlertDialog), findsNothing);
      }
      expect(
        selections,
        1,
        reason: tester
            .widgetList<Text>(find.byType(Text))
            .map((w) => w.data)
            .join(' | '),
      );
      expect(reads, protected ? 2 : 1);
      expect(registry.secrets['table'], protected ? 'fixture-secret' : '');
      await tester.pumpWidget(const SizedBox());
    });
  }
}
