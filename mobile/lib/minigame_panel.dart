import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';

import 'minigame.dart';
import 'models.dart';
import 'workspace_widgets.dart';
import 'key_box_designer.dart';
import 'table_workspace.dart';

typedef MiniGameCommand = Future<bool> Function(String, Map<String, String>);

// Transport/board validation already provides safe, actionable reasons. Keep
// that reason intact without exposing Dart's exception type as interface text.
String _gameErrorText(Object error) =>
    error is FormatException ? error.message.toString() : error.toString();

class MiniGamePanel extends StatefulWidget {
  const MiniGamePanel({
    super.key,
    required this.status,
    required this.connected,
    required this.command,
    required this.openBoxSettings,
    required this.workspace,
  });
  final DeviceStatus status;
  final bool connected;
  final MiniGameCommand command;
  final VoidCallback openBoxSettings;
  final TableWorkspace workspace;
  @override
  State<MiniGamePanel> createState() => _MiniGamePanelState();
}

class _MiniGamePanelState extends State<MiniGamePanel>
    with WidgetsBindingObserver {
  bool pending = false;
  String? error;
  Completer<void>? release;
  MiniGameSnapshot get game => widget.status.peer.minigame!;
  String get owner =>
      '${widget.status.json['board_id']}/${widget.status.json['boot_id']}';
  bool get ready =>
      widget.connected && game.ready && !pending && release == null;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
  }

  @override
  void didUpdateWidget(covariant MiniGamePanel oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!widget.connected ||
        oldWidget.status.json['board_id'] != widget.status.json['board_id'] ||
        oldWidget.status.json['boot_id'] != widget.status.json['boot_id']) {
      endHold();
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state != AppLifecycleState.resumed) endHold();
  }

  @override
  void deactivate() {
    endHold();
    super.deactivate();
  }

  @override
  void dispose() {
    endHold();
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  void endHold() {
    final value = release;
    if (value != null && !value.isCompleted) value.complete();
  }

  Future<bool> send(String path, Map<String, String> values) async {
    if (!ready || !widget.status.peer.allows(path, values)) return false;
    setState(() {
      pending = true;
      error = null;
    });
    try {
      final ok = await widget.command(path, values);
      if (!ok && mounted) {
        setState(() => error = 'The board did not confirm the command.');
      }
      return ok;
    } catch (e) {
      if (mounted) {
        setState(() => error = _gameErrorText(e));
      }
      return false;
    } finally {
      if (mounted) setState(() => pending = false);
    }
  }

  void beginHold(int key) {
    if (!ready) return;
    final command = widget.command, heldOwner = owner;
    final released = Completer<void>();
    setState(() {
      release = released;
      pending = true;
      error = null;
    });
    unawaited(() async {
      // Even an uncertain down may have reached the board. Its matching up is
      // sequenced after that receipt, never dropped by the single-command gate.
      try {
        final accepted = await command(minigameInput, {
          'key': '$key',
          'action': 'down',
        });
        if (!accepted && !released.isCompleted) released.complete();
        if (mounted) setState(() => pending = false);
        await released.future;
      } catch (e) {
        if (mounted) setState(() => error = _gameErrorText(e));
      } finally {
        try {
          if (owner == heldOwner) {
            final accepted = await command(minigameInput, {
              'key': '$key',
              'action': 'up',
            });
            if (!accepted && mounted) {
              setState(() => error = 'Key release was not confirmed.');
            }
          }
        } catch (e) {
          if (mounted) setState(() => error = _gameErrorText(e));
        }
        if (mounted) {
          setState(() {
            release = null;
            pending = false;
          });
        }
      }
    }());
  }

  Future<void> settings() async {
    final initial = game, sourceOwner = owner;
    await showDialog<void>(
      context: context,
      barrierDismissible: false,
      builder: (_) => MiniGameSettings(
        initial: initial,
        save: (patch) async {
          if (!mounted ||
              !ready ||
              owner != sourceOwner ||
              patch.keys.any(
                (key) => !minigameValueMatches(
                  game.settings[key],
                  initial.settings[key],
                ),
              )) {
            throw const FormatException(
              'The board or settings changed. Reopen settings.',
            );
          }
          if (!await send(minigameConfigure, {'settings': jsonEncode(patch)})) {
            throw const FormatException(
              'Settings were not confirmed by the board.',
            );
          }
        },
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final snapshot = game, colors = Theme.of(context).colorScheme;
    final primaryAction = snapshot.running
        ? 'pause'
        : snapshot.phase == 'paused'
        ? 'resume'
        : 'start';
    final winningTime =
        snapshot.mode == 'mine-field' && snapshot.phase == 'success';
    final remainingTime = snapshot.json['timed'] == true && !winningTime;
    final seconds =
        snapshot.number(remainingTime ? 'remaining_ms' : 'elapsed_ms') / 1000;
    final phaseTone = snapshot.phase == 'success'
        ? const Color(0xff38d996)
        : snapshot.phase == 'failed' || snapshot.phase == 'timeout'
        ? colors.error
        : colors.primary;
    return Align(
      alignment: Alignment.topCenter,
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 1240),
        child: ListView(
          key: const PageStorageKey('minigame-workspace-scroll'),
          padding: const EdgeInsets.all(20),
          children: [
            WorkspaceHeader(
              title: 'Game studio',
              icon: Icons.sports_esports_outlined,
              actions: [
                WorkspaceBadge(
                  snapshot.phase.replaceAll('-', ' '),
                  active: widget.connected,
                  icon: Icons.circle,
                ),
              ],
            ),
            Container(
              padding: const EdgeInsets.all(18),
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                  colors: [
                    Color.alphaBlend(
                      colors.primary.withValues(alpha: .09),
                      colors.surfaceContainerLow,
                    ),
                    colors.surfaceContainerLow,
                  ],
                ),
                borderRadius: BorderRadius.circular(22),
                border: Border.all(
                  color: colors.primary.withValues(alpha: .18),
                ),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Wrap(
                    spacing: 24,
                    runSpacing: 16,
                    crossAxisAlignment: WrapCrossAlignment.center,
                    children: [
                      SizedBox(
                        width: 380,
                        child: InputDecorator(
                          decoration: const InputDecoration(
                            labelText: 'Game mode',
                            contentPadding: EdgeInsets.symmetric(
                              horizontal: 14,
                              vertical: 4,
                            ),
                          ),
                          child: DropdownButtonHideUnderline(
                            child: DropdownButton<String>(
                              key: const ValueKey('minigame-mode'),
                              value: snapshot.mode,
                              isExpanded: true,
                              itemHeight:
                                  84 *
                                  MediaQuery.textScalerOf(context).scale(14) /
                                  14,
                              isDense: false,
                              borderRadius: BorderRadius.circular(16),
                              items: [
                                for (final (index, entry)
                                    in snapshot.modes.entries.indexed)
                                  DropdownMenuItem(
                                    value: entry.key,
                                    child: _GameModeChoice(
                                      number: index + 1,
                                      id: entry.key,
                                      label: entry.value,
                                      description: snapshot.description(
                                        entry.key,
                                      ),
                                    ),
                                  ),
                              ],
                              onChanged: !ready
                                  ? null
                                  : (mode) {
                                      if (mode != null &&
                                          mode != snapshot.mode) {
                                        unawaited(
                                          send(minigameConfigure, {
                                            'settings': jsonEncode({
                                              'mode': mode,
                                            }),
                                          }),
                                        );
                                      }
                                    },
                            ),
                          ),
                        ),
                      ),
                      if (snapshot.json['timed'] == true ||
                          !const [
                            'press-to-win',
                            'toggle',
                          ].contains(snapshot.mode))
                        Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            Icon(
                              winningTime
                                  ? Icons.emoji_events_outlined
                                  : remainingTime
                                  ? Icons.timer_outlined
                                  : Icons.av_timer,
                              color: phaseTone,
                              size: 22,
                            ),
                            const SizedBox(width: 9),
                            Flexible(
                              child: Text(
                                '${winningTime
                                    ? 'Winning time'
                                    : remainingTime
                                    ? 'Remaining'
                                    : 'Elapsed'}: ${seconds.toStringAsFixed(winningTime ? 3 : 1)} s',
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                                style: Theme.of(context).textTheme.titleMedium
                                    ?.copyWith(
                                      fontFeatures: const [
                                        FontFeature.tabularFigures(),
                                      ],
                                      fontWeight: FontWeight.w600,
                                    ),
                              ),
                            ),
                          ],
                        ),
                    ],
                  ),
                  const SizedBox(height: 16),
                  Wrap(
                    spacing: 8,
                    runSpacing: 8,
                    children: [
                      for (final action in [primaryAction, 'reset', 'stop'])
                        FilledButton.tonalIcon(
                          key: ValueKey('minigame-$action'),
                          style: FilledButton.styleFrom(
                            minimumSize: const Size(96, 42),
                            backgroundColor: action == primaryAction
                                ? colors.primary
                                : action == 'reset'
                                ? colors.primary.withValues(alpha: .13)
                                : colors.surfaceContainerHigh,
                            foregroundColor: action == primaryAction
                                ? colors.onPrimary
                                : action == 'reset'
                                ? colors.primary
                                : colors.onSurface,
                            shape: RoundedRectangleBorder(
                              borderRadius: BorderRadius.circular(12),
                            ),
                          ),
                          onPressed:
                              ready &&
                                  switch (action) {
                                    'start' =>
                                      !snapshot.running &&
                                          snapshot.phase != 'paused',
                                    'pause' => snapshot.running,
                                    'resume' => snapshot.phase == 'paused',
                                    _ => true,
                                  }
                              ? () => send(minigameControl, {'action': action})
                              : null,
                          icon: Icon(switch (action) {
                            'start' || 'resume' => Icons.play_arrow_rounded,
                            'pause' => Icons.pause_rounded,
                            'reset' => Icons.restart_alt_rounded,
                            _ => Icons.stop_rounded,
                          }, size: 20),
                          label: Text(
                            '${action[0].toUpperCase()}${action.substring(1)}',
                          ),
                        ),
                      TextButton.icon(
                        onPressed: ready ? settings : null,
                        icon: const Icon(Icons.tune, size: 19),
                        label: const Text('Game settings'),
                      ),
                    ],
                  ),
                  const SizedBox(height: 16),
                  Wrap(
                    spacing: 8,
                    runSpacing: 8,
                    children: [
                      WorkspaceBadge(
                        'Round: ${snapshot.number('round')}',
                        icon: Icons.flag_outlined,
                      ),
                      WorkspaceBadge(
                        'Progress: ${snapshot.number('progress')}',
                        icon: Icons.check_circle_outline,
                      ),
                      WorkspaceBadge(
                        'Mistakes: ${snapshot.number('mistakes')}',
                        icon: Icons.close_rounded,
                      ),
                      if (snapshot.number('winner') > 0)
                        WorkspaceBadge(
                          'Winner: ${snapshot.number('winner')}',
                          active: true,
                          icon: Icons.emoji_events_outlined,
                        ),
                      if (snapshot.number('target') > 0)
                        WorkspaceBadge(
                          'Target: ${snapshot.number('target')}',
                          active: true,
                          icon: Icons.my_location,
                        ),
                      if (snapshot.mode == 'color-match')
                        WorkspaceBadge(
                          'Matched: ${snapshot.number('progress')}',
                          icon: Icons.palette_outlined,
                        ),
                      if (snapshot.number('last_reaction_ms') > 0)
                        WorkspaceBadge(
                          'Reaction: ${snapshot.number('last_reaction_ms')} ms',
                          icon: Icons.bolt,
                        ),
                      if (snapshot.number('best_reaction_ms') > 0)
                        WorkspaceBadge(
                          'Best: ${snapshot.number('best_reaction_ms')} ms',
                          icon: Icons.speed,
                        ),
                      if (snapshot.number('warning') > 0)
                        WorkspaceBadge(
                          snapshot.number('warning') > 1
                              ? 'Urgent'
                              : 'Time warning',
                          icon: Icons.timer_outlined,
                        ),
                    ],
                  ),
                  SizedBox(
                    height: 3,
                    child: pending
                        ? const LinearProgressIndicator(minHeight: 2)
                        : null,
                  ),
                ],
              ),
            ),
            const SizedBox(height: 18),
            KeyBoxDesigner(
              status: widget.status,
              workspace: widget.workspace,
              enabled: ready && snapshot.acceptsInput,
              onTap: (key) =>
                  send(minigameInput, {'key': '$key', 'action': 'tap'}),
              onHoldStart: snapshot.usesHold ? beginHold : null,
              onHoldEnd: snapshot.usesHold ? endHold : null,
              openSettings: widget.openBoxSettings,
              inputKeyPrefix: 'minigame-key',
            ),
            if (!widget.connected)
              const Padding(
                padding: EdgeInsets.only(top: 12),
                child: Text('Reconnect to control the game.'),
              ),
            if (error != null)
              Padding(
                padding: const EdgeInsets.only(top: 12),
                child: Text(error!, style: TextStyle(color: colors.error)),
              ),
          ],
        ),
      ),
    );
  }
}

class _GameModeChoice extends StatelessWidget {
  const _GameModeChoice({
    required this.number,
    required this.id,
    required this.label,
    required this.description,
  });
  final int number;
  final String id, label, description;
  @override
  Widget build(BuildContext context) => Row(
    children: [
      SizedBox(
        width: 26,
        child: Text(
          '$number.',
          style: Theme.of(context).textTheme.labelMedium
              ?.copyWith(color: Theme.of(context).colorScheme.onSurfaceVariant),
        ),
      ),
      Container(
        width: 36,
        height: 36,
        alignment: Alignment.center,
        decoration: BoxDecoration(
          color: Theme.of(context).colorScheme.primary.withValues(alpha: .10),
          borderRadius: BorderRadius.circular(10),
        ),
        child: Icon(
          switch (id) {
            'press-to-win' => Icons.emoji_events_outlined,
            'illuminated' => Icons.my_location,
            'color-match' => Icons.palette_outlined,
            'simon' => Icons.queue_music,
            'memory-pairs' => Icons.style_outlined,
            'clear-all' => Icons.checklist_rounded,
            'color-order' => Icons.sort_rounded,
            'switch-pattern' => Icons.grid_view_rounded,
            'reaction-chase' => Icons.bolt_rounded,
            'mine-field' => Icons.diamond_outlined,
            'toggle' => Icons.toggle_on_outlined,
            _ => Icons.sports_esports_outlined,
          },
          size: 22,
          color: Theme.of(context).colorScheme.primary,
        ),
      ),
      const SizedBox(width: 12),
      Expanded(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              label,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: Theme.of(context).textTheme.bodyMedium
                  ?.copyWith(fontWeight: FontWeight.w600),
            ),
            const SizedBox(height: 3),
            Text(
              description,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: Theme.of(context).colorScheme.onSurfaceVariant,
              ),
            ),
          ],
        ),
      ),
    ],
  );
}

/// Adapted from PCController EffectEditor.number/numberDrafts/save: invalid text
/// stays local, numeric values are bounded, and only explicit save commits.
class MiniGameSettings extends StatefulWidget {
  const MiniGameSettings({
    super.key,
    required this.initial,
    required this.save,
  });
  final MiniGameSnapshot initial;
  final Future<void> Function(Map<String, Object>) save;
  @override
  State<MiniGameSettings> createState() => _MiniGameSettingsState();
}

class _MiniGameSettingsState extends State<MiniGameSettings> {
  final form = GlobalKey<FormState>();
  final patch = <String, Object>{};
  late final draft = Map<String, dynamic>.from(
    jsonDecode(jsonEncode(widget.initial.settings)) as Map,
  );
  bool saving = false;
  bool dirty = false, dismissing = false;
  String? error;
  static const labels = {
    'limit_ms': 'Time limit (ms; 0 = untimed)',
    'warning_ms': 'Warning remaining (ms)',
    'urgent_ms': 'Urgent remaining (ms)',
    'penalty_ms': 'Mistake penalty (ms)',
    'window_ms': 'Input window (ms)',
    'show_ms': 'Show time (ms)',
    'gap_ms': 'Gap (ms)',
    'feedback_ms': 'Feedback time (ms)',
    'intro_ms': 'Reveal fade per box (ms)',
    'preview_ms': 'Pairs preview hold (ms)',
    'mismatch_ms': 'Wrong pair feedback (ms)',
    'rounds': 'Rounds',
    'sequence_length': 'Sequence length',
    'sequence_growth': 'Additional steps per round',
    'palette_size': 'Palette size',
    'fixed_target': 'Fixed target (0 = random)',
  };
  void change(String key, Object value) => setState(() {
    dirty = true;
    draft[key] = value;
    patch[key] = value;
  });
  Future<void> dismiss() async {
    if (saving || dismissing) return;
    dismissing = true;
    try {
      if (dirty) {
        final discard = await showDialog<bool>(
          context: context,
          builder: (dialog) => AlertDialog(
            title: const Text('Unsaved game settings'),
            content: const Text('Discard these changes?'),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(dialog, false),
                child: const Text('Keep editing'),
              ),
              FilledButton(
                onPressed: () => Navigator.pop(dialog, true),
                child: const Text('Discard changes'),
              ),
            ],
          ),
        );
        if (discard != true) return;
      }
      if (mounted) Navigator.pop(context);
    } finally {
      dismissing = false;
    }
  }

  Future<void> save() async {
    if (saving || !form.currentState!.validate()) return;
    if (patch.isEmpty) {
      Navigator.pop(context);
      return;
    }
    setState(() {
      saving = true;
      error = null;
    });
    try {
      widget.initial.command(minigameConfigure, {
        'settings': jsonEncode(patch),
      });
      await widget.save(patch);
      if (mounted) Navigator.pop(context);
    } catch (e) {
      if (mounted) {
        setState(() {
          saving = false;
          error = _gameErrorText(e);
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) => PopScope(
    canPop: false,
    onPopInvokedWithResult: (didPop, _) {
      if (!didPop) unawaited(dismiss());
    },
    child: AlertDialog(
      title: const Text('Game settings'),
      content: SizedBox(
        width: 560,
        child: SingleChildScrollView(
          child: Form(
            key: form,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('Saving resets the game to Ready.'),
                const SizedBox(height: 16),
                if (const ['first', 'last'].contains(draft['boot_mode']))
                  DropdownButtonFormField<String>(
                    initialValue: draft['boot_mode'] as String,
                    isExpanded: true,
                    decoration: const InputDecoration(
                      labelText: 'Game on power-up',
                    ),
                    items: const [
                      DropdownMenuItem(
                        value: 'first',
                        child: Text('Press to win (first available game)'),
                      ),
                      DropdownMenuItem(
                        value: 'last',
                        child: Text('Last configured game · fresh round'),
                      ),
                    ],
                    onChanged: saving
                        ? null
                        : (value) {
                            if (value != null) change('boot_mode', value);
                          },
                  ),
                Wrap(
                  spacing: 12,
                  runSpacing: 12,
                  children: [
                    for (final entry in minigameLimits.entries)
                      if (draft[entry.key] is int &&
                          (entry.key != 'sequence_growth' ||
                              draft['mode'] == 'simon'))
                        SizedBox(
                          width: 230,
                          child: TextFormField(
                            key: ValueKey('minigame-setting-${entry.key}'),
                            enabled: !saving,
                            initialValue: '${draft[entry.key]}',
                            decoration: InputDecoration(
                              labelText:
                                  entry.key == 'show_ms' &&
                                      draft['mode'] == 'mine-field'
                                  ? 'Pulse period (ms)'
                                  : labels[entry.key],
                            ),
                            keyboardType: TextInputType.number,
                            validator: (text) {
                              final n = int.tryParse(text ?? '');
                              return n == null ||
                                      n < entry.value.$1 ||
                                      n > entry.value.$2
                                  ? 'Enter ${entry.value.$1}–${entry.value.$2}'
                                  : null;
                            },
                            onChanged: (text) {
                              final n = int.tryParse(text);
                              if (n != null) {
                                change(entry.key, n);
                              } else {
                                setState(() => dirty = true);
                              }
                            },
                          ),
                        ),
                  ],
                ),
                for (final entry in {
                  'intro_enabled': 'Reveal colors and notes before play',
                  'color_arpeggio': 'Arpeggio color notes',
                  'segment_animation': 'Ready display · perimeter animation',
                  'timer_beeps': 'Timer · second ticks and minute warning',
                  'timer_alternate_tones': 'Timer · alternate odd/even tones',
                }.entries)
                  if (draft[entry.key] is bool &&
                      (entry.key != 'intro_enabled' ||
                          const [
                            'color-match',
                            'memory-pairs',
                          ].contains(draft['mode'])))
                    SwitchListTile(
                      contentPadding: EdgeInsets.zero,
                      title: Text(entry.value),
                      value: draft[entry.key] as bool,
                      onChanged: saving ? null : (v) => change(entry.key, v),
                    ),
                if (draft['hard'] is bool)
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: const Text('Hard mode'),
                    value: draft['hard'] as bool,
                    onChanged: saving ? null : (v) => change('hard', v),
                  ),
                if (const [
                  'toggle',
                  'latch',
                  'press',
                ].contains(draft['toggle_kind']))
                  DropdownButtonFormField<String>(
                    initialValue: draft['toggle_kind'] as String,
                    decoration: const InputDecoration(
                      labelText: 'Toggle behavior',
                    ),
                    items: [
                      for (final v in ['toggle', 'latch', 'press'])
                        DropdownMenuItem(value: v, child: Text(v)),
                    ],
                    onChanged: saving
                        ? null
                        : (v) {
                            if (v != null) change('toggle_kind', v);
                          },
                  ),
                if (validOutputs(draft['outputs'])) ...[
                  const SizedBox(height: 20),
                  const Text('Output mapping'),
                  for (var i = 0; i < 8; i++)
                    Padding(
                      padding: const EdgeInsets.only(top: 12),
                      child: Row(
                        children: [
                          Expanded(
                            child: DropdownButtonFormField<String>(
                              initialValue:
                                  (draft['outputs'] as List)[i]['source']
                                      as String,
                              decoration: InputDecoration(
                                labelText: 'Output ${i + 1}',
                              ),
                              items: [
                                for (final v in minigameOutputSources)
                                  DropdownMenuItem(value: v, child: Text(v)),
                              ],
                              onChanged: saving
                                  ? null
                                  : (v) {
                                      if (v == null) return;
                                      final outputs = draft['outputs'] as List;
                                      final key = outputs[i]['key'] as int;
                                      outputs[i] = {
                                        'source': v,
                                        'key':
                                            minigameOutputSources.indexOf(v) < 4
                                            ? 0
                                            : key.clamp(1, 9),
                                      };
                                      change('outputs', outputs);
                                    },
                            ),
                          ),
                          if (minigameOutputSources.indexOf(
                                (draft['outputs'] as List)[i]['source']
                                    as String,
                              ) >=
                              4) ...[
                            const SizedBox(width: 12),
                            SizedBox(
                              width: 100,
                              child: DropdownButtonFormField<int>(
                                key: ValueKey(
                                  'output-key-$i-${(draft['outputs'] as List)[i]['key']}',
                                ),
                                initialValue:
                                    (draft['outputs'] as List)[i]['key'] as int,
                                decoration: const InputDecoration(
                                  labelText: 'Key',
                                ),
                                items: [
                                  for (var k = 1; k <= 9; k++)
                                    DropdownMenuItem(
                                      value: k,
                                      child: Text('$k'),
                                    ),
                                ],
                                onChanged: saving
                                    ? null
                                    : (v) {
                                        if (v == null) return;
                                        final outputs =
                                            draft['outputs'] as List;
                                        outputs[i] = {
                                          ...outputs[i] as Map,
                                          'key': v,
                                        };
                                        change('outputs', outputs);
                                      },
                              ),
                            ),
                          ],
                        ],
                      ),
                    ),
                ],
                if (error != null)
                  Text(
                    error!,
                    style: TextStyle(
                      color: Theme.of(context).colorScheme.error,
                    ),
                  ),
              ],
            ),
          ),
        ),
      ),
      actions: [
        TextButton(
          onPressed: saving ? null : dismiss,
          child: const Text('Cancel'),
        ),
        FilledButton(
          onPressed: saving ? null : save,
          child: Text(saving ? 'Saving…' : 'Save to board'),
        ),
      ],
    ),
  );
}
