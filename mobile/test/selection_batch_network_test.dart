import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/command_readback.dart';
import 'package:pakarcontroller/discovery.dart';
import 'package:pakarcontroller/lan_resolver.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

void main() {
  test('Sticky resolver re-resolves only after its cached TCP route becomes unreachable', () async {
    var lookups = 0, behavior = 'ok';
    final resolver = LanResolver(
      lookup: (_) async {
        ++lookups;
        if (behavior == 'timeout') throw TimeoutException('fixture');
        if (behavior == 'dns') {
          throw const SocketException('fixture lookup failure');
        }
        return [InternetAddress('192.0.2.1')];
      },
    );
    Future<String> good(InternetAddress address) async => address.address;
    expect(await resolver.connect('ledboard.local', good), '192.0.2.1');
    expect(await resolver.connect('LEDBOARD.local', good), '192.0.2.1');
    expect(lookups, 1);
    behavior = 'timeout';
    await expectLater(
      resolver.connect<String>(
        'ledboard.local',
        (_) async => throw TimeoutException('TCP'),
      ),
      throwsA(isA<TimeoutException>()),
    );
    behavior = 'ok';
    expect(await resolver.connect('ledboard.local', good), '192.0.2.1');
    expect(
      lookups,
      3,
    ); // Failed cached TCP route was retired, not timer-expired.
    behavior = 'dns';
    await expectLater(
      resolver.connect<String>(
        'ledboard.local',
        (_) async => throw const SocketException('TCP'),
      ),
      throwsA(isA<HostLookupFailure>()),
    );
    expect(resolver.lookupFailureRevision('ledboard.local'), 1);
    behavior = 'ok';
    await resolver.connect('ledboard.local', good);
    expect(lookups, 5);
  });
  test(
    'A failed cached TCP address can recover a DHCP change by read-only lookup',
    () async {
      var lookup = 0;
      final resolver = LanResolver(
        lookup: (_) async => [InternetAddress('192.0.2.${++lookup}')],
      );
      await resolver.connect('ledboard.local', (a) async => a.address);
      final dials = <String>[];
      final result = await resolver.connect('ledboard.local', (a) async {
        dials.add(a.address);
        if (a.address.endsWith('.1')) {
          throw const SocketException('stale TCP endpoint');
        }
        return a.address;
      });
      expect(result, '192.0.2.2');
      expect(dials, ['192.0.2.1', '192.0.2.2']);
    },
  );
  test('HTTP auth errors/timeouts do not trigger rediscovery or erase a verified fallback route', () async {
    var discover = 0, state = 'resolve';
    final paths = <String>[];
    final api = PakarApi(
      client: MockClient((r) async {
        paths.add(r.url.host);
        if (r.url.host.endsWith('.local')) throw HostLookupFailure(r.url.host);
        if (state == 'auth') return http.Response('private fixture error', 401);
        if (state == 'timeout') {
          throw TimeoutException('fixture response timeout');
        }
        return http.Response(
          jsonEncode({
            ...peerFixture().json,
            'board_id': 'fixture-board',
            'boot_id': 'fixture-boot',
          }),
          200,
        );
      }),
      discovery: () async {
        ++discover;
        return {'http://192.0.2.8'};
      },
    );
    await api.status();
    api.resetTransport();
    state = 'auth';
    await expectLater(api.status(), throwsFormatException);
    expect(api.transportUrl, 'http://192.0.2.8');
    state = 'timeout';
    await expectLater(api.status(), throwsA(isA<TimeoutException>()));
    expect(api.transportUrl, 'http://192.0.2.8');
    expect(discover, 1);
    expect(paths.last, '192.0.2.8');
    await api.close();
  });
  test('Missed discovery sweep retains bounded hostname records, not online or authorization', () {
    final catalog = DiscoveryCatalog();
    const device = DiscoveredDevice(
      'ledboard',
      'http://ledboard.local',
      'mDNS / DNS-SD',
    );
    expect(catalog.merge([device]), [device]);
    expect(catalog.seenThisScan(device), true);
    expect(catalog.merge([]), [device]);
    expect(catalog.seenThisScan(device), false);
  });
  test('Atomic capability and all-selected readback are independently required; Snake hidden', () {
    final status = peerFixture();
    expect(status.peer.allows('/api/cells', {'ids': '1,2'}), true);
    expect(
      peerFixture(features: ['table', 'leds']).peer.allows('/api/cells', {}),
      false,
    );
    expect(status.peer.modes.any((m) => m.id == 'snake'), false);
    expect(
      commandReadbackMatches(status, '/api/cells', {
        'ids': '1-80',
        'state': 'white',
      }),
      true,
    );
    expect(
      commandReadbackMatches(status, '/api/cells', {
        'ids': '1,2',
        'state': 'red',
      }),
      false,
    );
    final partial = DeviceStatus({
      ...status.json,
      'cells': ['red', ...List.filled(79, 'white')],
    });
    expect(
      commandReadbackMatches(partial, '/api/cells', {
        'ids': '1,2',
        'state': 'red',
      }),
      false,
    );
    expect(
      commandReadbackMatches(partial, '/api/cells', {
        'ids': '1',
        'state': 'red',
      }),
      true,
    );
  });
  testWidgets(
    'Select all/invert normalize Find; one atomic batch carries transition and never fans out',
    (tester) async {
      final sent = <Map<String, String>>[];
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: TablePage(
              status: peerFixture(),
              mapping: CellMap.defaults(),
              command: (p, [v = const {}]) async {},
              verifiedCommand: (path, values) async {
                expect(path, '/api/cells');
                sent.add(values);
                return false;
              },
            ),
          ),
        ),
      );
      await tester.ensureVisible(find.text('Select all'));
      await tester.tap(find.text('Select all'));
      await tester.pump();
      expect(
        tester.widget<TextField>(find.byType(TextField).first).controller!.text,
        '1-80',
      );
      await tester.tap(find.text('Invert selection'));
      await tester.pump();
      expect(
        tester.widget<TextField>(find.byType(TextField).first).controller!.text,
        '',
      );
      await tester.enterText(find.byType(TextField).first, '1-3');
      // Let the focused field's bring-into-view work finish before measuring
      // and scrolling to an action in the same outer viewport.
      await tester.pumpAndSettle();
      await tester.ensureVisible(find.text('Invert selection'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Invert selection'));
      await tester.pump();
      expect(
        tester.widget<TextField>(find.byType(TextField).first).controller!.text,
        '4-80',
      );
      await tester.ensureVisible(find.text('Red'));
      await tester.tap(find.text('Red'));
      await tester.pump();
      expect(sent, hasLength(1));
      expect(sent.single['ids']!.split(','), hasLength(77));
      expect(sent.single['effect'], 'immediate');
      expect(find.textContaining('Nothing was retried.'), findsOneWidget);
      await tester.pumpWidget(const SizedBox());
    },
  );
  testWidgets(
    'Melody commands use only live acknowledged AVR cues; offline slots stay stable and disabled',
    (tester) async {
      final sent = <Map<String, String>>[];
      final status = DeviceStatus({
        ...peerFixture(features: ['buzzer']).json,
        'audio_cues': ['fixture-melody', 'fixture-melody', '../bad'],
      });
      expect(status.audioCues, ['fixture-melody']);
      Widget panel(DeviceStatus observed) => MaterialApp(
        home: Scaffold(
          body: FrontPanelPage(
            status: observed,
            command: (p, [v = const {}]) async {
              expect(p, '/api/audio');
              sent.add(v);
            },
          ),
        ),
      );
      await tester.pumpWidget(panel(status));
      await tester.ensureVisible(find.text('Send melody'));
      await tester.tap(find.text('Send melody'));
      await tester.pumpAndSettle();
      expect(sent, [
        {'cue': 'fixture-melody'},
      ]);
      final element = tester.element(find.byType(FrontPanelPage));
      final action = find.widgetWithText(FilledButton, 'Send melody');
      final bounds = tester.getRect(action);
      final offline = DeviceStatus({...status.json, 'avr_online': false});
      await tester.pumpWidget(panel(offline));
      await tester.pumpAndSettle();
      expect(tester.element(find.byType(FrontPanelPage)), same(element));
      expect(tester.getRect(action), bounds);
      expect(tester.widget<FilledButton>(action).onPressed, isNull);
      expect(
        tester
            .widget<DropdownButtonFormField<String>>(
              find.byType(DropdownButtonFormField<String>),
            )
            .onChanged,
        isNull,
      );
      await tester.tap(action);
      await tester.pump();
      expect(sent, hasLength(1)); // Retained history is not fresh permission.
      final recovered = DeviceStatus({
        ...status.json,
        'audio_cues': ['new-live-cue'],
      });
      await tester.pumpWidget(panel(recovered));
      await tester.pumpAndSettle();
      expect(tester.getRect(action), bounds);
      final current = tester.widget<DropdownButtonFormField<String>>(
        find.byType(DropdownButtonFormField<String>),
      );
      expect(current.onChanged, isNotNull);
      final choices = tester.widget<DropdownButton<String>>(
        find.descendant(
          of: find.byType(DropdownButtonFormField<String>),
          matching: find.byType(DropdownButton<String>),
        ),
      );
      expect(choices.items!.map((item) => item.value), ['new-live-cue']);
      await tester.tap(action);
      await tester.pumpAndSettle();
      expect(sent.last, {'cue': 'new-live-cue'});
      expect(sent, hasLength(2));
      await tester.pumpWidget(const SizedBox());
    },
  );
}
