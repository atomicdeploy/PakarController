import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/status_rgb.dart';

void main() {
  test('compact RGB event changes only fresh matching RGB and rejects old or foreign revision', () {
    final s = DeviceStatus({
      'board_id': 'board-a',
      'boot_id': 'a' * 32,
      'hostname': 'ledboard',
      'command_clock_ms': 42,
      'cells': ['red'],
      'capabilities': {
        'board_kind': 'controller',
        'name': 'LED board',
        'hostname': 'ledboard',
        'acknowledged_features': ['status-rgb'],
      },
    });
    Map<String, dynamic> event(int revision, String boot) => {
      'type': 'status-rgb',
      'board_id': 'board-a',
      'boot_id': boot,
      'revision': revision,
      'status_rgb': {
        'revision': revision,
        'available': true,
        'have_output': true,
        'rgb': [1, 2, 3],
      },
    };
    final next = s.withStatusRgbEvent(event(3, 'a' * 32))!;
    expect(next.json['command_clock_ms'], 42);
    expect(next.json['cells'], ['red']);
    expect(next.withStatusRgbEvent(event(3, 'a' * 32)), isNull);
    expect(next.withStatusRgbEvent(event(4, 'b' * 32)), isNull);
    expect(s.preservingNewerRgb(next).json['status_rgb']['revision'], 3);
  });
  test('RGB settings require explicit complete observations and safe GPIO', () {
    expect(
      () => statusRgbConfiguration({
        'enabled': true,
        'pin': 48,
        'brightness': 64,
      }),
      throwsFormatException,
    );
    for (final pin in [4, 10, 17, 18, -1]) {
      expect(
        () => statusRgbConfiguration({
          'enabled': true,
          'pin': pin,
          'pin_confirmed': true,
          'brightness': 64,
        }),
        throwsFormatException,
      );
    }
    expect(
      statusRgbConfiguration({
        'enabled': true,
        'pin': 48,
        'pin_confirmed': true,
        'brightness': 64,
      })['pin'],
      48,
    );
  });
  test('physical RGB is observed not synthesized or double dimmed', () {
    final s = DeviceStatus({
      'hostname': 'ledboard',
      'capabilities': {
        'board_kind': 'controller',
        'name': 'LED board',
        'hostname': 'ledboard',
        'acknowledged_features': ['status-rgb'],
      },
      'status_rgb': {
        'available': true,
        'have_output': true,
        'enabled': true,
        'rgb': [0, 255, 40],
        'brightness': 16,
      },
    });
    expect(
      statusRgbColor(s, fresh: true),
      const Color.fromARGB(255, 0, 255, 40),
    );
    expect(statusRgbColor(s, fresh: false), isNull);
    s.json['status_rgb']['have_output'] = false;
    expect(statusRgbColor(s, fresh: true), isNull);
  });
}
