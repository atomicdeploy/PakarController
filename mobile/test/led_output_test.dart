import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/output_panel.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

DeviceStatus observedOutput(Object? output, {bool leds = true}) =>
    DeviceStatus({
      ...peerFixture(features: ['table', if (leds) 'leds']).json,
      'led_output': output,
    });

void main() {
  test('Output mode is optional, exact and never inferred from enabled', () {
    for (final value in [
      null,
      {},
      {'enabled': true},
      {'mode': 1},
      {'mode': 'future'},
    ]) {
      expect(observedOutput(value).ledOutputMode, isNull);
    }
    for (final mode in ['static', 'effects']) {
      expect(observedOutput({'mode': mode}).ledOutputMode, mode);
      expect(observedOutput({'mode': mode}, leds: false).ledOutputMode, isNull);
      expect(
        observedOutput({'mode': mode}).peer
            .allows('/api/led/output', {'mode': mode}),
        true,
      );
    }
  });
  test('Output is optional, boolean-only and requires explicit LED ACK', () {
    expect(peerFixture().ledOutputEnabled, isNull);
    for (final output in [
      null,
      false,
      {},
      {'enabled': 'true'},
      {'enabled': 1},
    ]) {
      expect(observedOutput(output).ledOutputEnabled, isNull);
    }
    expect(observedOutput({'enabled': true}).ledOutputEnabled, true);
    expect(observedOutput({'enabled': false}).ledOutputEnabled, false);
    expect(
      observedOutput({'enabled': true}, leds: false).ledOutputEnabled,
      isNull,
    );
    expect(
      DeviceStatus({
        'led_output': {'enabled': true},
      }).ledOutputEnabled,
      isNull,
    );
  });

  test(
    'Output permission is independent of modes and Idle is not selectable',
    () {
      final peer = observedOutput({'enabled': false}).peer;
      expect(peer.allows('/api/led/output', {'enabled': 'true'}), true);
      expect(peer.allows('/api/led/output', {'enabled': 'false'}), true);
      expect(peer.allows('/api/mode', {'value': 'idle'}), false);
      expect(peer.modes.any((mode) => mode.id == 'idle'), false);
      expect(
        observedOutput({
          'enabled': true,
        }, leds: false).peer.allows('/api/led/output', {'enabled': 'true'}),
        false,
      );
    },
  );

  testWidgets(
    'Output control requires observed state and disappears on peer loss',
    (tester) async {
      Future<void> render(DeviceStatus? status) => tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: OutputPanel(
              connected: true,
              openDiagnostics: () {},
              status: status,
              command: (path, [values = const {}]) async {},
            ),
          ),
        ),
      );
      for (final status in [
        null,
        peerFixture(),
        observedOutput({'enabled': true}, leds: false),
      ]) {
        await render(status);
        expect(find.text('LED output service'), findsNothing);
      }
      await render(observedOutput({'enabled': true}));
      expect(find.text('LED output service'), findsOneWidget);
      expect(find.text('Idle'), findsNothing);
      await render(null);
      expect(find.text('LED output service'), findsNothing);
    },
  );

  testWidgets('Output sends once, waits, and changes only on live readback', (
    tester,
  ) async {
    final ack = Completer<void>();
    final writes = <(String, Map<String, String>)>[];
    Future<void> render(bool enabled) => tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: OutputPanel(
            connected: true,
            openDiagnostics: () {},
            status: observedOutput({'enabled': enabled}),
            command: (path, [values = const {}]) async {
              writes.add((path, Map.of(values)));
              await ack.future;
            },
          ),
        ),
      ),
    );
    final toggle = find.byType(SwitchListTile);
    await render(true);
    await tester.ensureVisible(toggle);
    await tester.tap(toggle);
    await tester.pump();
    expect(writes.length, 1);
    expect(writes.single.$1, '/api/led/output');
    expect(writes.single.$2, {'enabled': 'false'});
    expect(tester.widget<SwitchListTile>(toggle).value, true);
    expect(tester.widget<SwitchListTile>(toggle).onChanged, isNull);
    await tester.tap(toggle);
    await tester.pump();
    expect(writes.length, 1);
    ack.complete();
    await tester.pump();
    expect(tester.widget<SwitchListTile>(toggle).value, true);
    await render(false);
    expect(tester.widget<SwitchListTile>(toggle).value, false);
    await tester.tap(toggle);
    await tester.pump();
    expect(writes.last.$1, '/api/led/output');
    expect(writes.last.$2, {'enabled': 'true'});
    expect(tester.widget<SwitchListTile>(toggle).value, false);
    expect(tester.takeException(), isNull);
  });

  testWidgets('Mode uses readback; rainbow keeps mode and output unchanged', (
    tester,
  ) async {
    final ack = Completer<void>();
    final writes = <(String, Map<String, String>)>[];
    Future<void> render(String mode) => tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: OutputPanel(
            connected: true,
            openDiagnostics: () {},
            status: observedOutput({'enabled': false, 'mode': mode}),
            command: (path, [values = const {}]) async {
              writes.add((path, Map.of(values)));
              await ack.future;
            },
          ),
        ),
      ),
    );
    ChoiceChip chip(String label) =>
        tester.widget<ChoiceChip>(find.widgetWithText(ChoiceChip, label));
    await render('static');
    final effects = find.widgetWithText(ChoiceChip, 'Effects');
    await tester.ensureVisible(effects);
    await tester.tap(effects);
    await tester.pump();
    expect(writes.length, 1);
    expect(writes.single.$1, '/api/led/output');
    expect(writes.single.$2, {'mode': 'effects'});
    expect(chip('Static').selected, true);
    expect(chip('Effects').selected, false);
    expect(chip('Effects').onSelected, isNull);
    ack.complete();
    await tester.pump();
    expect(chip('Static').selected, true);
    await render('effects');
    expect(chip('Effects').selected, true);
    await render('static');
    final rainbow = find.text('Rainbow test');
    await tester.ensureVisible(rainbow);
    await tester.tap(rainbow);
    await tester.pump();
    expect(writes.length, 2);
    expect(writes.last.$1, '/api/effect');
    expect(writes.last.$2, {'value': 'rainbow'});
    expect(chip('Static').selected, true);
    // Rainbow is below the switch; scroll back before inspecting the lazily
    // recycled ListView child rather than assuming every control stays mounted.
    await tester.scrollUntilVisible(
      find.byType(SwitchListTile),
      -150,
      scrollable: find.byType(Scrollable).first,
    );
    expect(
      tester.widget<SwitchListTile>(find.byType(SwitchListTile)).value,
      false,
    );
    expect(tester.takeException(), isNull);
  });
}
