import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

void main() {
  test('Keycontroller probe clears all 216 pixels and steps a 12-pixel ring', () {
    const probe = PixelProbe(index: 204, group: true, rgb: 0x123456,
        pixels: 216, groupSize: 12);
    expect(probe.valid, isTrue);
    expect(probe.canNext, isFalse);
    expect(probe.move(-1).first, 192);
    final frame = probe.frame();
    expect(frame.length, 648);
    expect(frame.take(204 * 3).every((byte) => byte == 0), isTrue);
    expect(frame.sublist(204 * 3), List.generate(36, (i) => [0x12, 0x34, 0x56][i % 3]));
    final packets = diagnosticPackets(frame);
    expect(packets.length, 1);
    expect(packets.single[8], 216);
    expect(packets.single[9], 1);
    expect(probe.copyWith(rgb: 0).pixels, 216);
  });
  test(
    'Workspace stores only bounded cursor, colour, grouping and position draft',
    () {
      final workspace = PixelDiagnosticWorkspace(
        saved: {
          'index': 479,
          'group': true,
          'rgb': 0x1234ab,
          'position': '480',
          'vertical': 50,
          'applied': true,
          'pending': true,
          'boot_id': 'not-authority',
        },
      );
      expect(workspace.probe.first, 474);
      expect(workspace.snapshot(), {
        'index': 479,
        'group': true,
        'rgb': 0x1234ab,
        'position': '480',
        'vertical': 50.0,
      });
      var changes = 0;
      workspace.addListener(() => changes++);
      workspace.probe = const PixelProbe(
        index: 479,
        group: true,
        rgb: 0x1234ab,
      );
      expect(changes, 0);
      workspace.probe = const PixelProbe(
        index: 473,
        group: true,
        rgb: 0xff0000,
      );
      expect(changes, 1);
      expect(workspace.position, '469');
      final invalid = PixelDiagnosticWorkspace(
        saved: {
          'index': 480,
          'rgb': -1,
          'vertical': double.nan,
          'position': 'x' * 9,
        },
      );
      expect(invalid.probe.index, 0);
      expect(invalid.probe.rgb, 0xffffff);
      expect(invalid.vertical, 0);
      expect(invalid.position, '1');
      workspace.dispose();
      invalid.dispose();
    },
  );
  test(
    'Every raw position clears all other pixels; RGB and all-off are exact',
    () {
      for (var index = 0; index < diagnosticPixels; index++) {
        final probe = PixelProbe(index: index, rgb: 0x1234ab);
        final frame = probe.frame();
        expect(frame.length, 1440);
        for (var at = 0; at < diagnosticPixels; at++) {
          expect(
            frame.sublist(at * 3, at * 3 + 3),
            at == index ? [0x12, 0x34, 0xab] : [0, 0, 0],
          );
        }
        expect(probe.frame(off: true).every((v) => v == 0), true);
      }
    },
  );
  test('Six-pixel groups align, include the full tail and never wrap', () {
    for (var index = 0; index < diagnosticPixels; index++) {
      final probe = PixelProbe(index: index, group: true, rgb: 0xff0000);
      expect(probe.first % 6, 0);
      expect(probe.frame().where((v) => v == 255).length, 6);
      if (probe.canNext) expect(probe.move(1).first, probe.first + 6);
      if (probe.canPrevious) expect(probe.move(-1).first, probe.first - 6);
    }
    expect(() => const PixelProbe(group: true).move(-1), throwsFormatException);
  });
  test(
    'Boundaries and invalid inputs fail without generating partial frames',
    () {
      expect(() => const PixelProbe(index: -1).frame(), throwsFormatException);
      expect(() => const PixelProbe(index: 480).frame(), throwsFormatException);
      expect(
        () => const PixelProbe(rgb: 0x1000000).frame(),
        throwsFormatException,
      );
      expect(() => const PixelProbe().move(-1), throwsFormatException);
      expect(() => const PixelProbe(index: 479).move(1), throwsFormatException);
      expect(
        () => const PixelProbe(index: 479, group: true).move(1),
        throwsFormatException,
      );
    },
  );
  test('Full frame uses two native PKAR chunks, only final chunk commits', () {
    final frame = const PixelProbe(index: 479, rgb: 0xabcdef).frame();
    final packets = diagnosticPackets(frame);
    expect(packets.map((p) => p.length), [730, 730]);
    expect(packets[0].sublist(0, 10), [80, 75, 65, 82, 0, 0, 0, 0, 240, 0]);
    expect(packets[1].sublist(0, 10), [80, 75, 65, 82, 0, 0, 240, 0, 240, 1]);
    expect([...packets[0].skip(10), ...packets[1].skip(10)], frame);
    final hex = frame.map((v) => v.toRadixString(16).padLeft(2, '0')).join();
    expect(
      matchesDiagnosticFrame(
        DeviceStatus({
          'pixel_frame': {'revision': 1, 'rgb': hex},
        }),
        frame,
      ),
      true,
    );
    expect(
      matchesDiagnosticFrame(
        DeviceStatus({
          'pixel_frame': {'revision': 1, 'rgb': 'ff' * 1440},
        }),
        frame,
      ),
      false,
    );
  });
}
