import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/peer_context.dart';

import 'peer_fixture.dart';

void main() {
  test('LED backend capability is explicit; omitted or invalid is unknown', () {
    for (final item in [
      {'backend': 'gpio-static', 'rmt': false, 'dma': false},
      {'backend': 'rmt-dma', 'rmt': true, 'dma': true},
      {'backend': 'fastled', 'rmt': false, 'dma': false},
    ]) {
      final json = peerFixture().json;
      (json['capabilities'] as Map)['led_output'] = item;
      final driver = DeviceStatus(json).peer.ledOutput!;
      expect(driver.backend, item['backend']);
      expect(driver.rmt, item['rmt']);
      expect(driver.dma, item['dma']);
    }
    for (final item in [
      null,
      {},
      {'backend': 'rmt-dma', 'rmt': false, 'dma': true},
      {'backend': 'gpio-static', 'rmt': false, 'dma': 'false'},
    ]) {
      final json = peerFixture().json;
      (json['capabilities'] as Map)['led_output'] = item;
      final peer = DeviceStatus(json).peer;
      expect(peer.known, true);
      expect(peer.ledOutput, isNull);
    }
    expect(peerFixture().peer.ledOutput, isNull);
    final noLeds = peerFixture(features: ['buzzer']).json;
    noLeds['capabilities']['led_output'] = {
      'backend': 'rmt-dma',
      'rmt': true,
      'dma': true,
    };
    expect(DeviceStatus(noLeds).peer.ledOutput, isNull);
  });
  test(
    'No peer ACK cannot be inferred from live pixels, cells or AVR flags',
    () {
      final status = DeviceStatus({
        'hostname': 'ledboard',
        'pixels': 480,
        'avr_online': true,
        'tm1637_ack': true,
        'cells': List.filled(80, 'white'),
      });
      expect(status.peer.known, false);
      expect(status.peer.table, false);
      expect(status.peer.frontPanel, false);
      expect(status.peer.modes, isEmpty);
      expect(status.peer.allows('/api/cell', {'id': '1'}), false);
    },
  );
  test('Independent ACKs and measured display gate follow donor ready AND advertised', () {
    final buzzer = peerFixture(features: ['buzzer'], kind: 'controller');
    expect(buzzer.peer.has('buzzer'), true);
    expect(buzzer.peer.has('text'), false);
    expect(buzzer.peer.has('display'), false);
    expect(buzzer.peer.table, false);
    expect(buzzer.peer.modes, isEmpty);
    expect(peerFixture(displayAck: false).peer.has('display'), false);
    expect(
      peerFixture(features: ['text'], displayAck: false).peer.has('text'),
      true,
    );
  });
  test('Table geometry and advertised mode identity must agree; First wins needs IO', () {
    expect(peerFixture(kind: 'controller').peer.modes, isEmpty);
    expect(peerFixture(features: ['table']).peer.modes.map((m) => m.id), [
      'cell',
    ]);
    final status = peerFixture();
    expect(status.peer.modes.map((m) => m.id), ['cell', 'first-wins']);
    expect(status.peer.allows('/api/mode', {'value': 'snake'}), false);
    expect(status.peer.allows('/api/mode', {'value': 'not-advertised'}), false);
    final invalid = peerFixture().json;
    (invalid['capabilities']['table'] as Map)['rows'] = 9;
    expect(DeviceStatus(invalid).peer.table, false);
    (invalid['capabilities']['table'] as Map)['rows'] = 8.0;
    expect(DeviceStatus(invalid).peer.table, false);
  });
  test('Mixed host and missing feature ACKs fail closed', () {
    final value = peerFixture().json;
    value['capabilities']['hostname'] = 'other';
    expect(DeviceStatus(value).peer.known, false);
    value['capabilities']['hostname'] = 'ledboard';
    value['capabilities']['acknowledged_features'] = null;
    expect(DeviceStatus(value).peer.known, false);
  });
  test(
    'Inline identity text/icon are bounded; no URL fetch or name translation',
    () {
      expect(
        peerFixture(title: 'میز فارسی').peer.application!.title,
        'میز فارسی',
      );
      for (final title in ['', 'a' * 161, 'Bad\nTitle']) {
        expect(peerFixture(title: title).peer.application, isNull);
      }
      final bytes = Uint8List(33);
      bytes.setRange(0, 8, [137, 80, 78, 71, 13, 10, 26, 10]);
      bytes.setRange(12, 16, ascii.encode('IHDR'));
      ByteData.sublistView(bytes).setUint32(16, 64);
      ByteData.sublistView(bytes).setUint32(20, 64);
      final json = {
        'id': 'app',
        'name': 'App',
        'title': 'App title',
        'icon_png_base64': base64Encode(bytes),
      };
      expect(PeerApplication.parse(json)!.icon, isNotNull);
      ByteData.sublistView(bytes).setUint32(16, 4096);
      json['icon_png_base64'] = base64Encode(bytes);
      expect(PeerApplication.parse(json)!.icon, isNull);
      json['icon_png_base64'] = 'https://other/logo.png';
      expect(PeerApplication.parse(json)!.icon, isNull);
    },
  );
}
