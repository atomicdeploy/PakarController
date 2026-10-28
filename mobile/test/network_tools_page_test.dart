import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/advanced_page.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/command_wire.dart';
import 'package:pakarcontroller/network_tools_page.dart';

import 'dart:convert';

import 'peer_fixture.dart';

class NetworkToolsFixture extends PakarApi {
  NetworkToolsFixture() : super(enforcePeerVersions: false);
  final writes = <Map<String, dynamic>>[];
  final settings = <String, dynamic>{
    'ntp_enabled': true,
    'ntp_dhcp': true,
    'ntp_server': 'pool.ntp.org',
    'http_date_url': 'https://example.com/',
    'http_date_automatic': false,
    'public_ip_url': 'https://api.ipify.org/',
    'public_ip_automatic': false,
    'refresh_seconds': 3600,
  };
  @override
  Future<Map<String, dynamic>> getJson(String path) async => {
    'settings': {...settings},
    'clock': {'valid': true, 'utc_epoch': 1793190896, 'source': 'ntp'},
    'observed': {'public_ip_observed': false},
    'job': {'busy': false, 'phase': 'idle'},
  };
  @override
  Future<Map<String, dynamic>> networkToolsWrite(
    String path,
    Map<String, dynamic> body,
  ) async {
    writes.add({'path': path, ...body});
    if (path.endsWith('/tools')) settings.addAll(body);
    return getJson('/api/network/tools');
  }
}

void main() {
  test('Network command uses the shared typed correlated command envelope', () {
    final fixture = peerFixture(
      features: ['network', 'network-tools', 'command-ack'],
    );
    final status = fixture.json
      ..addAll({
        'board_id': 'esp32s3-001122334455',
        'boot_id': '0123456789abcdef0123456789abcdef',
        'command_clock_ms': 1234,
      });
    final packet = jsonDecode(
      commandPacket(fixture, '/api/network/tools', {
        'settings': jsonEncode({
          'ntp_enabled': false,
          'ntp_server': 'time.example.org',
        }),
      }),
    ) as Map;
    expect(packet['settings'], {
      'ntp_enabled': false,
      'ntp_server': 'time.example.org',
    });
    expect(packet['expires_at_ms'], 9234);
    expect(packet['board_id'], status['board_id']);
    expect(packet['command'], 'configure-network-tools');
  });
  testWidgets('Advanced has distinct destinations and opens without a write', (
    tester,
  ) async {
    final api = NetworkToolsFixture();
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: AdvancedPage(
            api: api,
            observed: () => peerFixture(features: ['network-tools']),
            openSettings: (_) {},
          ),
        ),
      ),
    );
    expect(find.text('Clock & network tools'), findsOneWidget);
    expect(find.text('Remote connections'), findsOneWidget);
    expect(find.text('App bridge & repeater'), findsOneWidget);
    await tester.tap(find.text('Clock & network tools'));
    await tester.pumpAndSettle();
    expect(find.text('NTP server / fallback'), findsOneWidget);
    expect(api.writes, isEmpty);
    expect(tester.takeException(), isNull);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets(
    'Network tools preserve drafts on refresh and explicitly save typed settings',
    (tester) async {
      final api = NetworkToolsFixture();
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(body: NetworkToolsPage(api: api)),
        ),
      );
      await tester.pumpAndSettle();
      final server = find.widgetWithText(TextField, 'NTP server / fallback');
      await tester.enterText(server, 'time.example.org');
      await tester.tap(find.byTooltip('Refresh observed values'));
      await tester.pumpAndSettle();
      expect(find.text('time.example.org'), findsOneWidget);
      expect(api.writes, isEmpty);
      await tester.tap(find.text('Save'));
      await tester.pumpAndSettle();
      expect(api.writes.single['ntp_server'], 'time.example.org');
      expect(api.writes.single.containsKey('ntp_enabled'), false);
      expect(api.writes.single.containsKey('refresh_seconds'), false);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );
}
