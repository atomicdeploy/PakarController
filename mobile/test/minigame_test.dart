import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/command_wire.dart';
import 'package:pakarcontroller/minigame.dart';
import 'package:pakarcontroller/minigame_panel.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/output_panel.dart';

import 'output_panel_test.dart' show keyObservation;

DeviceStatus miniStatus({
  bool ready = true,
  String phase = 'ready',
  String board = 'keys-A',
  String mode = 'press-to-win',
  String toggleKind = 'toggle',
}) {
  final base = keyObservation(board: board).json;
  return DeviceStatus({
    ...base,
    'command_clock_ms': 500,
    'capabilities': {
      ...base['capabilities'] as Map,
      'acknowledged_features': [
        ...(base['capabilities'] as Map)['acknowledged_features'] as List,
        'command-ack',
      ],
    },
    'minigame': {
      'ready': ready,
      'mode': mode,
      'phase': phase,
      'revision': 2,
      'available_modes': [
        {'id': 'press-to-win', 'label': 'Press to win'},
        {'id': 'toggle', 'label': 'Switches'},
      ],
      'timed': true,
      'elapsed_ms': 1500,
      'remaining_ms': 58500,
      'participating_mask': 3,
      'held_mask': 0,
      'latched_mask': 0,
      'winner': 0,
      'target': 1,
      'round': 1,
      'progress': 2,
      'mistakes': 0,
      'warning': 0,
      'last_reaction_ms': 200,
      'best_reaction_ms': 150,
      'settings': {
        'mode': mode,
        'limit_ms': 60000,
        'warning_ms': 10000,
        'urgent_ms': 3000,
        'penalty_ms': 2000,
        'window_ms': 1200,
        'show_ms': 500,
        'gap_ms': 200,
        'feedback_ms': 240,
        'rounds': 5,
        'sequence_length': 4,
        'palette_size': 7,
        'fixed_target': 0,
        'toggle_kind': toggleKind,
        'hard': false,
        'outputs': List.generate(8, (_) => {'source': 'disabled', 'key': 0}),
      },
    },
  });
}

Widget fixture(
  DeviceStatus status,
  MiniGameCommand command, {
  bool connected = true,
}) => MaterialApp(
  home: Scaffold(
    body: OutputPanel(
      section: OutputPanelSection.game,
      status: status,
      connected: connected,
      command: (path, [values = const {}]) async {
        await command(path, values);
      },
      verifiedCommand: command,
      openDiagnostics: () {},
    ),
  ),
);

MiniGameSnapshot configuredGame(String mode, Map<String, Object> settings) {
  final raw = miniStatus().json['minigame'] as Map;
  return MiniGameSnapshot.read({
    ...raw,
    'mode': mode,
    'available_modes': [
      for (final id in minigameDescriptions.keys) {'id': id, 'label': id},
    ],
    'settings': {...raw['settings'] as Map, 'mode': mode, ...settings},
  })!;
}

Widget settingsFixture(
  MiniGameSnapshot game,
  Future<void> Function(Map<String, Object>) save,
) => MaterialApp(
  home: Scaffold(
    body: MiniGameSettings(initial: game, save: save),
  ),
);

void main() {
  test(
    'Receipt owner survives the input fence without admitting a new command',
    () {
      final ready = miniStatus(), fenced = miniStatus(ready: false);
      expect(
        commandPeerMatches(
          fenced,
          ready.json['board_id'],
          ready.json['boot_id'],
        ),
        isTrue,
      );
      expect(fenced.peer.allows(minigameControl, {'action': 'start'}), isFalse);
      expect(
        commandPeerMatches(
          miniStatus(board: 'other'),
          ready.json['board_id'],
          ready.json['boot_id'],
        ),
        isFalse,
      );
      expect(commandPeerMatches(null, null, null), isFalse);
    },
  );
  test('Boot and timer presentation settings are strict optional fields', () {
    final game = miniStatus().peer.minigame!;
    expect(
      game.command(minigameConfigure, {
        'settings': jsonEncode({
          'boot_mode': 'last',
          'segment_animation': true,
          'timer_beeps': true,
          'timer_alternate_tones': false,
        }),
      }),
      isNotNull,
    );
    for (final patch in [
      {'boot_mode': 'resume-winner'},
      {'timer_beeps': 1},
    ]) {
      expect(
        () => game.command(minigameConfigure, {'settings': jsonEncode(patch)}),
        throwsFormatException,
      );
    }
  });
  test('Intro and arpeggio settings are typed, bounded and optional', () {
    final snapshot = miniStatus().peer.minigame!;
    final fields = {
      'settings': '{"intro_enabled":true,"color_arpeggio":true,"intro_ms":1000,"preview_ms":1500,"mismatch_ms":1200}',
    };
    expect(snapshot.allows(minigameConfigure, fields), isTrue);
    expect(
      snapshot.allows(minigameConfigure, {'settings': '{"mismatch_ms":100}'}),
      isFalse,
    );
    expect(
      snapshot.allows(minigameConfigure, {
        'settings': '{"color_arpeggio":"true"}',
      }),
      isFalse,
    );
    expect(snapshot.description('toggle'), contains('without a timer'));
  });
  testWidgets('Untimed press to win has no elapsed countdown', (tester) async {
    final base = miniStatus().json;
    final status = DeviceStatus({
      ...base,
      'minigame': {
        ...base['minigame'] as Map,
        'timed': false,
        'remaining_ms': 0,
        'settings': {
          ...(base['minigame'] as Map)['settings'] as Map,
          'limit_ms': 0,
        },
      },
    });
    await tester.pumpWidget(fixture(status, (_, _) async => true));
    await tester.pump();
    expect(find.textContaining('Elapsed:'), findsNothing);
    expect(find.textContaining('Remaining:'), findsNothing);
    expect(tester.takeException(), isNull);
  });
  testWidgets(
    'Advertised unready service never falls back to legacy game presses',
    (tester) async {
      final commands = <String>[];
      await tester.pumpWidget(
        fixture(miniStatus(ready: false), (path, values) async {
          commands.add(path);
          return true;
        }),
      );
      expect(find.byKey(const ValueKey('key-layout-canvas')), findsOneWidget);
      expect(find.byKey(const ValueKey('game-key-drag-1')), findsNothing);
      expect(
        tester
            .widget<GestureDetector>(
              find.byKey(const ValueKey('minigame-key-1')),
            )
            .onTap,
        isNull,
      );
      expect(
        tester
            .widget<FilledButton>(find.byKey(const ValueKey('minigame-start')))
            .onPressed,
        isNull,
      );
      expect(commands, isEmpty);
    },
  );
  test('Minigame command admission requires advertised ready snapshot and bounded settings', () {
    expect(
      keyObservation().peer.allows(minigameControl, {'action': 'start'}),
      isFalse,
    );
    expect(
      miniStatus(ready: false).peer
          .allows(minigameControl, {'action': 'start'}),
      isFalse,
    );
    final status = miniStatus();
    expect(
      status.peer.allows(minigameConfigure, {
        'settings': '{"mode":"invented"}',
      }),
      isFalse,
    );
    expect(
      status.peer.allows(minigameConfigure, {'settings': '{"enabled_mask":1}'}),
      isFalse,
    );
    expect(
      status.peer.allows(minigameConfigure, {'settings': '{"window_ms":0}'}),
      isFalse,
    );
    expect(
      status.peer.allows(minigameConfigure, {
        'settings': '{"urgent_ms":10001}',
      }),
      isFalse,
    );
    final packet = jsonDecode(
      commandPacket(status, minigameConfigure, {
        'settings': '{"mode":"toggle","hard":true}',
      }),
    ) as Map;
    expect(packet['command'], 'minigame-configure');
    expect(packet['settings'], {'mode': 'toggle', 'hard': true});
    expect(packet['board_id'], 'keys-A');
    expect(packet['expires_at_ms'], 8500);
    final input = jsonDecode(
      commandPacket(status, minigameInput, {'key': '2', 'action': 'down'}),
    ) as Map;
    expect(input['key'], 2);
    expect(input['action'], 'down');
  });

  test('Saved settings readback succeeds during an unready input fence', () {
    final status = miniStatus(ready: false);
    final raw = status.json['minigame'];
    final values = {'settings': '{"limit_ms":60000,"hard":false}'};
    expect(minigameReadbackMatches(raw, minigameConfigure, values), isTrue);
    expect(status.peer.allows(minigameConfigure, values), isFalse);
    expect(
      minigameReadbackMatches(raw, minigameConfigure, {
        'settings': '{"limit_ms":61000}',
      }),
      isFalse,
    );
    expect(
      minigameReadbackMatches(raw, minigameConfigure, {
        'settings': '{"window_ms":0}',
      }),
      isFalse,
    );
  });

  test(
    'Settings readback compares every output position and rejects mismatches',
    () {
      final status = miniStatus();
      final settings = status.peer.minigame!.settings;
      final outputs = List<Map<String, Object>>.generate(
        8,
        (_) => {'source': 'disabled', 'key': 0},
      );
      outputs[3] = {'source': 'held', 'key': 2};
      final values = {
        'settings': jsonEncode({'outputs': outputs}),
      };
      expect(
        minigameReadbackMatches(
          status.json['minigame'],
          minigameConfigure,
          values,
        ),
        isFalse,
      );
      final updated = {
        ...status.json['minigame'] as Map,
        'settings': {...settings, 'outputs': outputs},
      };
      expect(
        minigameReadbackMatches(updated, minigameConfigure, values),
        isTrue,
      );
      expect(validOutputs([...outputs.take(7)]), isFalse);
    },
  );

  testWidgets(
    'Mode picker numbers only advertised games with icons and muted descriptions',
    (tester) async {
      tester.view.physicalSize = const Size(420, 1000);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final commands = <String>[];
      await tester.pumpWidget(
        fixture(miniStatus(), (path, _) async {
          commands.add(path);
          return true;
        }),
      );
      await tester.tap(find.byKey(const ValueKey('minigame-mode')));
      await tester.pumpAndSettle();
      expect(find.text('2.'), findsOneWidget);
      expect(find.text('3.'), findsNothing);
      expect(find.byIcon(Icons.toggle_on_outlined), findsOneWidget);
      final description = tester.widget<Text>(
        find.text(miniStatus().peer.minigame!.description('toggle')),
      );
      expect(description.maxLines, 2);
      final context = tester.element(find.text('Switches'));
      expect(
        description.style?.color,
        Theme.of(context).colorScheme.onSurfaceVariant,
      );
      expect(commands, isEmpty);
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'Opening never starts and controls use only board catalog and live phase',
    (tester) async {
      final commands = <(String, Map<String, String>)>[];
      Future<bool> command(String path, Map<String, String> values) async {
        commands.add((path, values));
        return true;
      }

      await tester.pumpWidget(fixture(miniStatus(), command));
      expect(commands, isEmpty);
      expect(find.text('Remaining: 58.5 s'), findsOneWidget);
      expect(find.text('Progress: 2'), findsOneWidget);
      await tester.tap(find.byKey(const ValueKey('minigame-mode')));
      await tester.pumpAndSettle();
      expect(find.text('Switches'), findsOneWidget);
      expect(find.text('Simon'), findsNothing);
      await tester.tap(find.text('Switches'));
      await tester.pumpAndSettle();
      expect(commands.single.$1, minigameConfigure);
      expect(jsonDecode(commands.single.$2['settings']!), {'mode': 'toggle'});
      await tester.tap(find.byKey(const ValueKey('minigame-start')));
      await tester.pumpAndSettle();
      expect(commands.last.$1, minigameControl);
      expect(commands.last.$2, {'action': 'start'});
      expect(find.byKey(const ValueKey('minigame-pause')), findsNothing);
      await tester.pumpWidget(fixture(miniStatus(phase: 'paused'), command));
      await tester.tap(find.byKey(const ValueKey('minigame-resume')));
      await tester.pumpAndSettle();
      expect(commands.last.$1, minigameControl);
      expect(commands.last.$2, {'action': 'resume'});
    },
  );

  testWidgets(
    'One contextual transport button follows phase without sending on refresh',
    (tester) async {
      final commands = <Map<String, String>>[];
      for (final phase in [
        'ready',
        'show',
        'input',
        'feedback',
        'paused',
        'success',
        'failed',
        'timeout',
        'stopped',
      ]) {
        await tester.pumpWidget(
          fixture(miniStatus(phase: phase), (_, values) async {
            commands.add(values);
            return true;
          }),
        );
        await tester.pump();
        final expected = phase == 'paused'
            ? 'resume'
            : ['show', 'input', 'feedback'].contains(phase)
            ? 'pause'
            : 'start';
        for (final action in ['start', 'pause', 'resume']) {
          expect(
            find.byKey(ValueKey('minigame-$action')),
            action == expected ? findsOneWidget : findsNothing,
            reason: '$phase must expose only $expected',
          );
        }
        expect(commands, isEmpty);
        await tester.tap(find.byKey(ValueKey('minigame-$expected')));
        await tester.pumpAndSettle();
        expect(commands.single, {'action': expected});
        commands.clear();
        expect(tester.takeException(), isNull);
      }
    },
  );

  testWidgets(
    'Tap and hold release remain serialized while down ACK is pending',
    (tester) async {
      final commands = <(String, Map<String, String>)>[];
      final down = Completer<bool>();
      await tester.pumpWidget(
        fixture(
          miniStatus(mode: 'toggle', phase: 'input', toggleKind: 'press'),
          (path, values) async {
            commands.add((path, values));
            return values['action'] == 'down' ? down.future : true;
          },
        ),
      );
      await tester.ensureVisible(find.byKey(const ValueKey('minigame-key-1')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const ValueKey('minigame-key-1')));
      await tester.pumpAndSettle();
      expect(commands.single.$1, minigameInput);
      expect(commands.single.$2, {'key': '1', 'action': 'tap'});
      final gesture = await tester.startGesture(
        tester.getCenter(find.byKey(const ValueKey('minigame-key-2'))),
      );
      await tester.pump(const Duration(milliseconds: 600));
      expect(commands.last.$1, minigameInput);
      expect(commands.last.$2, {'key': '2', 'action': 'down'});
      await gesture.up();
      await tester.pump();
      expect(commands.length, 2);
      down.complete(true);
      await tester.pumpAndSettle();
      expect(commands.last.$1, minigameInput);
      expect(commands.last.$2, {'key': '2', 'action': 'up'});
      expect(commands.length, 3);
    },
  );

  testWidgets('Disabled and disconnected keys do not issue input', (
    tester,
  ) async {
    final commands = <String>[];
    Future<bool> command(String path, Map<String, String> values) async {
      commands.add(path);
      return true;
    }

    await tester.pumpWidget(fixture(miniStatus(), command));
    await tester.ensureVisible(find.byKey(const ValueKey('minigame-key-3')));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const ValueKey('minigame-key-3')));
    await tester.pumpAndSettle();
    expect(commands, isEmpty);
    await tester.pumpWidget(fixture(miniStatus(), command, connected: false));
    await tester.ensureVisible(find.byKey(const ValueKey('minigame-key-1')));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const ValueKey('minigame-key-1')));
    await tester.pumpAndSettle();
    expect(commands, isEmpty);
  });

  testWidgets(
    'Invalid settings stay local and a failed save retains the editor',
    (tester) async {
      final commands = <Map<String, String>>[];
      await tester.pumpWidget(
        fixture(miniStatus(), (path, values) async {
          commands.add(values);
          return false;
        }),
      );
      await tester.tap(find.text('Game settings'));
      await tester.pumpAndSettle();
      await tester.enterText(
        find.byKey(const ValueKey('minigame-setting-limit_ms')),
        '-1',
      );
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(commands, isEmpty);
      await tester.enterText(
        find.byKey(const ValueKey('minigame-setting-limit_ms')),
        '12000',
      );
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(jsonDecode(commands.single['settings']!), {'limit_ms': 12000});
      expect(find.text('Save to board'), findsOneWidget);
    },
  );

  testWidgets(
    'Illuminated response window shows board 10 seconds and remains configurable',
    (tester) async {
      Map<String, Object>? saved;
      await tester.pumpWidget(
        settingsFixture(
          configuredGame('illuminated', {'window_ms': 10000}),
          (patch) async => saved = patch,
        ),
      );
      final field = find.byKey(const ValueKey('minigame-setting-window_ms'));
      expect(tester.widget<TextFormField>(field).initialValue, '10000');
      await tester.ensureVisible(field);
      await tester.enterText(field, '15000');
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(saved, {'window_ms': 15000});
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'Color pattern intro stays off when supplied off; pairs reveal stays on and editable',
    (tester) async {
      for (final mode in ['color-match', 'memory-pairs']) {
        Map<String, Object>? saved;
        final intro = mode == 'memory-pairs';
        await tester.pumpWidget(
          settingsFixture(
            configuredGame(mode, {
              'intro_enabled': intro,
              'color_arpeggio': true,
              'intro_ms': 1000,
              'preview_ms': 1500,
              'mismatch_ms': 1200,
            }),
            (patch) async => saved = patch,
          ),
        );
        final reveal = find.widgetWithText(
          SwitchListTile,
          'Reveal colors and notes before play',
        );
        await tester.ensureVisible(reveal);
        expect(tester.widget<SwitchListTile>(reveal).value, intro);
        expect(find.text('Arpeggio color notes'), findsOneWidget);
        await tester.tap(reveal);
        await tester.pump();
        expect(tester.widget<SwitchListTile>(reveal).value, !intro);
        await tester.tap(find.text('Save to board'));
        await tester.pumpAndSettle();
        expect(saved, {'intro_enabled': !intro});
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      }
    },
  );

  testWidgets(
    'Simon response feedback and steps use observed settings and save a narrow patch',
    (tester) async {
      Map<String, Object>? saved;
      await tester.pumpWidget(
        settingsFixture(
          configuredGame('simon', {
            'rounds': 3,
            'sequence_length': 3,
            'sequence_growth': 2,
            'show_ms': 300,
            'gap_ms': 120,
            'feedback_ms': 240,
          }),
          (patch) async => saved = patch,
        ),
      );
      expect(
        tester
            .widget<TextFormField>(
              find.byKey(const ValueKey('minigame-setting-rounds')),
            )
            .initialValue,
        '3',
      );
      expect(
        tester
            .widget<TextFormField>(
              find.byKey(const ValueKey('minigame-setting-sequence_length')),
            )
            .initialValue,
        '3',
      );
      for (final entry in {
        'sequence_growth': '2',
        'show_ms': '300',
        'gap_ms': '120',
      }.entries) {
        expect(
          tester
              .widget<TextFormField>(
                find.byKey(ValueKey('minigame-setting-${entry.key}')),
              )
              .initialValue,
          entry.value,
        );
      }
      final feedback = find.byKey(
        const ValueKey('minigame-setting-feedback_ms'),
      );
      await tester.ensureVisible(feedback);
      await tester.enterText(feedback, '600');
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(saved, {'feedback_ms': 600});
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'Simon growth is optional, bounded, saved alone and hidden for other games',
    (tester) async {
      final game = configuredGame('simon', {
        'rounds': 3,
        'sequence_length': 3,
        'sequence_growth': 2,
      });
      expect(
        game.allows(minigameConfigure, {'settings': '{"sequence_growth":0}'}),
        isTrue,
      );
      expect(
        game.allows(minigameConfigure, {'settings': '{"sequence_growth":31}'}),
        isTrue,
      );
      expect(
        game.allows(minigameConfigure, {'settings': '{"sequence_growth":32}'}),
        isFalse,
      );
      Map<String, Object>? saved;
      await tester.pumpWidget(
        settingsFixture(game, (patch) async => saved = patch),
      );
      final growth = find.byKey(
        const ValueKey('minigame-setting-sequence_growth'),
      );
      await tester.ensureVisible(growth);
      await tester.enterText(growth, '1');
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(saved, {'sequence_growth': 1});
      await tester.pumpWidget(const SizedBox());
      await tester.pumpWidget(
        settingsFixture(
          configuredGame('illuminated', {'sequence_growth': 2}),
          (_) async {},
        ),
      );
      expect(growth, findsNothing);
      await tester.pumpWidget(const SizedBox());
      await tester.pumpWidget(
        settingsFixture(configuredGame('simon', {}), (_) async {}),
      );
      expect(
        growth,
        findsNothing,
        reason: 'Older board settings must not acquire guessed growth defaults',
      );
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'Rejected game settings keep their actionable cause without a Dart exception prefix',
    (tester) async {
      await tester.pumpWidget(
        settingsFixture(
          configuredGame('illuminated', {'window_ms': 10000}),
          (_) async => throw const FormatException(
            'The board could not save the input window.',
          ),
        ),
      );
      final field = find.byKey(const ValueKey('minigame-setting-window_ms'));
      await tester.ensureVisible(field);
      await tester.enterText(field, '12000');
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      final error = find.text('The board could not save the input window.');
      await tester.ensureVisible(error);
      expect(error, findsOneWidget);
      expect(find.textContaining('FormatException:'), findsNothing);
      expect(find.text('Save to board'), findsOneWidget);
      expect(
        tester
            .widget<EditableText>(
              find.descendant(of: field, matching: find.byType(EditableText)),
            )
            .controller
            .text,
        '12000',
      );
      expect(tester.takeException(), isNull);
    },
  );

  test('Mine Field follows the advertised catalog and uses existing typed settings', () {
    final game = configuredGame('mine-field', {
      'limit_ms': 60000,
      'show_ms': 1000,
    });
    final modes = game.modes.keys.toList();
    expect(modes.length, 11);
    expect(modes.indexOf('mine-field'), modes.indexOf('reaction-chase') + 1);
    expect(modes.last, 'toggle');
    expect(game.description('mine-field'), contains('all cyan or all yellow'));
    expect(
      game.command(minigameConfigure, {
        'settings': '{"mode":"mine-field","limit_ms":60000,"show_ms":1000}',
      }),
      {
        'settings': {'mode': 'mine-field', 'limit_ms': 60000, 'show_ms': 1000},
      },
    );
    expect(
      miniStatus().peer.minigame!.allows(minigameConfigure, {
        'settings': '{"mode":"mine-field"}',
      }),
      isFalse,
      reason: 'A client must not add Mine Field to an older board catalog',
    );
  });

  testWidgets(
    'Mine Field shows a distinct icon, does not auto-start and edits its pulse period',
    (tester) async {
      final game = configuredGame('mine-field', {
        'limit_ms': 60000,
        'show_ms': 1000,
      });
      final commands = <Map<String, String>>[];
      final status = DeviceStatus({
        ...miniStatus().json,
        'minigame': game.json,
      });
      await tester.pumpWidget(
        fixture(status, (_, values) async {
          commands.add(values);
          return true;
        }),
      );
      expect(find.byIcon(Icons.diamond_outlined), findsOneWidget);
      expect(commands, isEmpty);
      await tester.pumpWidget(const SizedBox());
      Map<String, Object>? saved;
      await tester.pumpWidget(
        settingsFixture(game, (patch) async => saved = patch),
      );
      expect(find.text('Pulse period (ms)'), findsOneWidget);
      final pulse = find.byKey(const ValueKey('minigame-setting-show_ms'));
      expect(tester.widget<TextFormField>(pulse).initialValue, '1000');
      await tester.ensureVisible(pulse);
      await tester.enterText(pulse, '800');
      await tester.tap(find.text('Save to board'));
      await tester.pumpAndSettle();
      expect(saved, {'show_ms': 800});
      await tester.pumpWidget(const SizedBox());
      await tester.pumpWidget(
        settingsFixture(
          configuredGame('simon', {'show_ms': 300}),
          (_) async {},
        ),
      );
      expect(find.text('Pulse period (ms)'), findsNothing);
      expect(find.text('Show time (ms)'), findsOneWidget);
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'Timed Mine Field success shows winning elapsed milliseconds without sending commands',
    (tester) async {
      final game = configuredGame('mine-field', {
        'limit_ms': 60000,
        'show_ms': 1000,
      });
      final commands = <Map<String, String>>[];
      final status = DeviceStatus({
        ...miniStatus().json,
        'minigame': {
          ...game.json,
          'phase': 'success',
          'timed': true,
          'elapsed_ms': 1234,
          'remaining_ms': 58766,
        },
      });
      await tester.pumpWidget(
        fixture(status, (_, values) async {
          commands.add(values);
          return true;
        }),
      );
      expect(find.text('Winning time: 1.234 s'), findsOneWidget);
      expect(find.textContaining('Remaining:'), findsNothing);
      expect(commands, isEmpty);
      final simon = DeviceStatus({
        ...status.json,
        'minigame': {...status.json['minigame'] as Map, 'mode': 'simon'},
      });
      await tester.pumpWidget(
        fixture(simon, (_, values) async {
          commands.add(values);
          return true;
        }),
      );
      expect(find.text('Winning time: 1.234 s'), findsNothing);
      expect(find.text('Remaining: 58.8 s'), findsOneWidget);
      expect(commands, isEmpty);
      expect(tester.takeException(), isNull);
    },
  );
}
