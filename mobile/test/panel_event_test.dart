import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/mirroring.dart';
import 'package:pakarcontroller/panel_event.dart';

import 'mirroring_test.dart' show SilentOutput, mirroredPeer, toneEvent;

Map<String, dynamic> panelEvent(int id) => {
  'type': 'panel-event',
  'board_id': 'board-a',
  'boot_id': 'boot-a',
  'event_id': id,
  'uptime_ms': 123,
  'kind': 'cell',
  'value': 80,
  'text': 'Cell 80 red',
  'hardware_confirmed': false,
  'presentation': {
    'text': 'Cell',
    'lifetime_ms': 1000,
    'blink': 'steady',
    'marquee': 'once',
    'priority': 40,
  },
};

void main() {
  test(
    'Semantic decoder bounds optional data without treating it as hardware',
    () {
      expect(PanelEvent.decode(panelEvent(1))!.preview, 'Cell');
      for (final change in <Map<String, dynamic>>[
        {'hardware_confirmed': true},
        {'kind': 'unknown'},
        {'value': 1.5},
        {'event_id': -1},
        {'uptime_ms': 0x100000000},
        {'text': 'غلط'},
        {'text': List.filled(41, 'a').join()},
      ]) {
        expect(PanelEvent.decode({...panelEvent(1), ...change}), isNull);
      }
      final invalid = panelEvent(1);
      invalid['presentation'] = {
        ...(invalid['presentation'] as Map),
        'text': 'five!',
      };
      expect(PanelEvent.decode(invalid), isNull);
      expect(
        PanelEvent.decode({...panelEvent(1), 'future_optional': 1}),
        isNotNull,
      );
    },
  );

  testWidgets(
    'Separate semantic sequence is bounded and never changes confirmed masks or plays tones',
    (tester) async {
      final output = SilentOutput(),
          mirror = PeripheralMirror(output: SilentOutput());
      mirror.dispose();
      final actual = PeripheralMirror(output: output);
      actual.event(panelEvent(1));
      expect(actual.panelHistory, isEmpty);
      actual.observe(mirroredPeer(), 1);
      final confirmed = actual.segments.toList();
      actual.event(panelEvent(0xffffffff));
      actual.event(panelEvent(0));
      actual.event(panelEvent(0));
      expect(actual.panelHistory.length, 2);
      actual.event({...panelEvent(1), 'board_id': 'other'});
      expect(actual.panelHistory.length, 2);
      for (var id = 1; id < 70; id++) {
        actual.event(panelEvent(id));
      }
      expect(actual.panelHistory.length, 32);
      expect(actual.panelPresentation!.value, 80);
      expect(actual.segments, confirmed);
      expect(output.opens, 0);
      expect(output.notes, isEmpty);
      // The AVR stream has an independent contiguous sequence, starting at any ID.
      actual.event(toneEvent(8, 0, 0));
      actual.event(toneEvent(9, 0, 0));
      expect(actual.status, isNotNull);
      actual.observe(mirroredPeer(session: 2), 1);
      expect(actual.panelHistory, isEmpty);
      actual.dispose();
    },
  );

  testWidgets(
    'Presentation expires and semantic traffic cannot renew the owner lease',
    (tester) async {
      var time = 0;
      final mirror = PeripheralMirror(output: SilentOutput(), now: () => time);
      mirror.observe(mirroredPeer(), 1);
      mirror.event(panelEvent(1));
      expect(mirror.panelPresentation, isNotNull);
      time = 1000001;
      await tester.pump(const Duration(milliseconds: 1001));
      expect(mirror.panelPresentation, isNull);
      await tester.pump(const Duration(seconds: 4));
      mirror.event(panelEvent(2));
      expect(mirror.panelHistory.length, 2);
      await tester.pump(const Duration(seconds: 1));
      expect(mirror.status, isNull);
      expect(mirror.panelHistory, isEmpty);
      mirror.dispose();
    },
  );
}
