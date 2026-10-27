import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

import 'peer_fixture.dart';

/// Explicit simulated board. Its output specification is not physical proof.
DeviceStatus advancedDiagnosticFixture({
  PixelProbe probe = const PixelProbe(),
  int revision = 0,
  bool off = false,
  bool pending = false,
}) => DeviceStatus({
  ...peerFixture(
    features: [
      'leds',
      'table',
      'command-ack',
      'pixel-diagnostic',
      'pixel-diagnostic-effects',
      'pixel-diagnostic-overlay',
    ],
  ).json,
  'board_id': 'fixture', 'boot_id': 'a' * 32, 'command_clock_ms': 1000,
  'pixels': 480, 'effect': 'rainbow',
  'led_output': {
    'enabled': true,
    'mode': 'effects',
    'diagnostic': {
      'revision': revision,
      'active': revision > 0 && probe.enabled,
      'enabled': probe.enabled,
      'index': probe.first,
      'count': probe.count,
      'rgb': probe.rgb,
      'off': off,
      'effect': probe.effect,
      'composite': probe.overlay ? 'overlay' : 'clear',
      'period_ms': probe.periodMs,
      'duty_percent': probe.dutyPercent,
      'min_brightness': probe.minBrightness,
      'max_brightness': probe.maxBrightness,
    },
  },
  // Moving output deliberately has no fixed RGB frame to compare with a draft.
  'pixel_frame': {'revision': revision + 1, 'pending': pending},
});
