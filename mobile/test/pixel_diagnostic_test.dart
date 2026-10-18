import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

void main() {
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
