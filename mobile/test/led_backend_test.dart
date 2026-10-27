import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/command_readback.dart';
import 'package:pakarcontroller/command_wire.dart';
import 'package:pakarcontroller/led_backend.dart';
import 'package:pakarcontroller/led_backend_control.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/output_panel.dart';

import 'peer_fixture.dart';

Map<String, dynamic> backendStatus({
  String active = 'gpio-static',
  String requested = 'gpio-static',
  bool pending = false,
  int error = 0,
  List<String> available = const ['gpio-static', 'rmt-dma'],
}) => {
  ...peerFixture(features: ['table', 'leds', 'command-ack']).json,
  'board_id': 'ledcontroller-fixture',
  'boot_id': 'a' * 32,
  'command_clock_ms': 1000,
  'capabilities': {
    ...peerFixture(features: ['table', 'leds', 'command-ack'])
            .json['capabilities']
        as Map,
    'led_output': {
      'backend': active,
      'rmt': active == 'rmt-dma',
      'dma': active == 'rmt-dma',
      'available_backends': available,
      'runtime_switch': true,
    },
  },
  'led_output': {
    'enabled': true,
    'mode': 'effects',
    'backend_saved': true,
    'driver': {
      'backend': active,
      'requested_backend': requested,
      'available_backends': available,
      'switch_pending': pending,
      'switch_error': error,
      'error_code': 0,
      'ready': true,
    },
  },
};

void main() {
  test('Backend ACK is not applied state, and failure or unsaved choice never verifies', () {
    const request = {'backend': 'rmt-dma'};
    final pending = backendStatus(requested: 'rmt-dma', pending: true);
    expect(
      commandReadbackMatches(DeviceStatus(pending), '/api/led/output', request),
      false,
    );
    final actual = backendStatus(active: 'rmt-dma', requested: 'rmt-dma');
    expect(
      commandReadbackMatches(DeviceStatus(actual), '/api/led/output', request),
      true,
    );
    for (final patch in [
      {'switch_pending': true},
      {'switch_error': -1},
      {'ready': false},
      {'requested_backend': 'gpio-static'},
      {'switch_pending': null},
    ]) {
      final value = jsonDecode(jsonEncode(actual)) as Map<String, dynamic>;
      (value['led_output']['driver'] as Map).addAll(patch);
      expect(
        commandReadbackMatches(DeviceStatus(value), '/api/led/output', request),
        false,
      );
    }
    actual['led_output']['backend_saved'] = false;
    expect(
      commandReadbackMatches(DeviceStatus(actual), '/api/led/output', request),
      false,
    );
    actual['led_output']['backend_saved'] = true;
    actual['led_output']['backend_settings_error'] = 'backend_commit_failed';
    expect(LedBackendState.fromStatus(actual)?.failed, true);
    expect(
      LedBackendState.fromStatus(actual)?.failureDescription,
      contains('backend_commit_failed'),
    );
    expect(
      commandReadbackMatches(DeviceStatus(actual), '/api/led/output', request),
      false,
    );
  });

  test(
    'Old firmware without compiled driver metadata has no runtime selector',
    () {
      final state = backendStatus();
      (state['capabilities']['led_output'] as Map).remove('available_backends');
      expect(LedBackendState.fromStatus(state), isNull);
      final incomplete = backendStatus();
      (incomplete['led_output']['driver'] as Map).remove('available_backends');
      expect(LedBackendState.fromStatus(incomplete), isNull);
    },
  );

  test('Only explicitly compiled backends can be sent, renderer and service remain independent', () {
    final state = DeviceStatus(backendStatus(available: ['gpio-static']));
    expect(state.peer.allows('/api/led/output', {'backend': 'rmt-dma'}), false);
    expect(state.peer.allows('/api/led/output', {'backend': 'none'}), false);
    expect(state.peer.allows('/api/led/output', {'mode': 'effects'}), true);
    expect(state.peer.allows('/api/led/output', {'enabled': 'false'}), true);
    expect(
      state.peer.allows('/api/led/output', {
        'backend': 'gpio-static',
        'enabled': 'true',
      }),
      false,
    );
    final packet = jsonDecode(
      commandPacket(state, '/api/led/output', {'backend': 'gpio-static'}),
    );
    expect(packet['command'], 'led-output');
    expect(packet['backend'], 'gpio-static');
    expect(packet.containsKey('mode'), false);
    expect(
      LedBackendState.fromStatus(backendStatus(active: 'none'))?.active,
      'none',
    );
  });

  testWidgets(
    'Output keeps the active backend until live readback and prevents duplicate switches',
    (tester) async {
      tester.view.physicalSize = const Size(900, 1200);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final accepted = Completer<void>();
      final writes = <Map<String, String>>[];
      var status = DeviceStatus(backendStatus());
      Widget panel() => MaterialApp(
        home: Scaffold(
          body: OutputPanel(
            status: status,
            connected: true,
            command: (path, [values = const {}]) {
              expect(path, '/api/led/output');
              writes.add(values);
              return accepted.future;
            },
            openDiagnostics: () {},
          ),
        ),
      );
      await tester.pumpWidget(panel());
      final selector = find.byKey(const ValueKey('led-backend-selector'));
      await tester.ensureVisible(selector);
      await tester.tap(selector);
      await tester.pumpAndSettle();
      await tester.tap(find.text('RMT + DMA').last);
      await tester.pumpAndSettle();
      expect(writes, [
        {'backend': 'rmt-dma'},
      ]);
      expect(
        tester.widget<DropdownButton<String>>(selector).value,
        'gpio-static',
      );
      expect(tester.widget<DropdownButton<String>>(selector).onChanged, isNull);
      status = DeviceStatus(backendStatus(requested: 'rmt-dma', pending: true));
      await tester.pumpWidget(panel());
      accepted.complete();
      await tester.pump();
      expect(tester.widget<DropdownButton<String>>(selector).onChanged, isNull);
      expect(find.text('Switching to RMT + DMA…'), findsOneWidget);
      status = DeviceStatus(
        backendStatus(active: 'rmt-dma', requested: 'rmt-dma'),
      );
      await tester.pumpWidget(panel());
      expect(tester.widget<DropdownButton<String>>(selector).value, 'rmt-dma');
      expect(find.text('Active: RMT + DMA'), findsOneWidget);
      expect(writes.length, 1);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'Backend control fits narrow view with errors and does not offer absent drivers',
    (tester) async {
      tester.view.physicalSize = const Size(320, 640);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final state = LedBackendState.fromStatus(
        backendStatus(error: 261, available: ['gpio-static']),
      )!;
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Padding(
              padding: const EdgeInsets.all(20),
              child: LedBackendControl(
                state: state,
                enabled: false,
                onChanged: (_) => fail('offline write'),
              ),
            ),
          ),
        ),
      );
      expect(find.text('Transmitter error 261'), findsOneWidget);
      expect(find.text('RMT + DMA'), findsNothing);
      expect(tester.takeException(), isNull);
      expect(
        tester
            .widget<DropdownButton<String>>(find.byType(DropdownButton<String>))
            .onChanged,
        isNull,
      );
    },
  );
}
