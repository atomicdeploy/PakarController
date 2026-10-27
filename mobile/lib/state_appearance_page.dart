import 'dart:convert';
import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'models.dart';
import 'command_readback.dart';
import 'color_picker.dart';

/// Uses the same acknowledged command owner as table and key controls.
/// Draft fields and temporary previews never stand in for saved observations.
class StateAppearancePage extends StatefulWidget {
  const StateAppearancePage({
    super.key,
    required this.status,
    required this.command,
    required this.readStatus,
    required this.ids,
    this.statusUpdates,
  });
  final DeviceStatus status;
  final Future<bool> Function(String, Map<String, String>) command;
  final Future<DeviceStatus> Function() readStatus;
  final String ids;
  final Stream<DeviceStatus>? statusUpdates;
  @override
  State<StateAppearancePage> createState() => _StateAppearancePageState();
}

class _StateAppearancePageState extends State<StateAppearancePage> {
  late DeviceStatus observed = widget.status;
  bool busy = false;
  bool peerRetired = false;
  String? error;
  final timingDrafts = <String, String>{};
  StreamSubscription<DeviceStatus>? subscription;
  final testColor = TextEditingController(text: '00FF00');
  Map get presentation =>
      observed.json['state_presentation'] as Map? ?? const {};
  bool get samePeer =>
      !peerRetired &&
      observed.json['board_id'] == widget.status.json['board_id'] &&
      observed.json['boot_id'] == widget.status.json['boot_id'];
  @override
  void initState() {
    super.initState();
    subscription = widget.statusUpdates?.listen((next) {
      if (!mounted || next.olderThan(observed)) return;
      // Small telemetry frames need not contain the settings body. They may
      // retire a peer, but must not erase its last complete appearance state.
      final changedPeer =
          next.json['board_id'] != observed.json['board_id'] ||
          next.json['boot_id'] != observed.json['boot_id'];
      if (changedPeer || next.json['state_presentation'] is Map) {
        setState(() {
          if (changedPeer) peerRetired = true;
          observed = next;
        });
      }
    });
  }

  @override
  void dispose() {
    subscription?.cancel();
    testColor.dispose();
    super.dispose();
  }

  Future<void> send(String path, Map<String, String> values) async {
    if (busy || !samePeer) return;
    setState(() {
      busy = true;
      error = null;
    });
    try {
      if (!await widget.command(path, values)) {
        throw const FormatException('Device did not confirm this change');
      }
      final next = await widget.readStatus();
      if (next.json['board_id'] != widget.status.json['board_id'] ||
          next.json['boot_id'] != widget.status.json['boot_id']) {
        throw const FormatException(
          'Board changed; reopen appearance settings',
        );
      }
      if (path == '/api/led/presentation' &&
          !commandReadbackMatches(next, path, values)) {
        throw const FormatException(
          'Settings changed before readback; review the current values',
        );
      }
      if (mounted) {
        setState(() {
          if (!peerRetired && !next.olderThan(observed)) observed = next;
          if (path == '/api/led/presentation') {
            final patch = jsonDecode(values['settings']!) as Map;
            for (final field in patch.keys) {
              if (int.tryParse(timingDrafts[field] ?? '') == patch[field]) {
                timingDrafts.remove(field);
              }
            }
          }
        });
      }
    } on Object catch (e) {
      if (mounted) setState(() => error = '$e');
    } finally {
      if (mounted) setState(() => busy = false);
    }
  }

  Future<void> patch(Map<String, Object> value) =>
      send('/api/led/presentation', {'settings': jsonEncode(value)});
  Future<void> chooseColor(String state, int rgb) async {
    final value = await showLedColorPicker(
      context,
      initial: rgb,
      title: '$state color',
    );
    if (value != null && mounted) {
      await patch({
        'states': {
          state: {'rgb': value},
        },
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final states = presentation['states'] as Map? ?? const {};
    return Scaffold(
      appBar: AppBar(title: const Text('State colors & effects')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          SwitchListTile(
            title: const Text('Extended effects'),
            value: presentation['enabled'] == true,
            onChanged: busy || !samePeer ? null : (v) => patch({'enabled': v}),
          ),
          for (final state in ['idle', 'won', 'lost', 'off'])
            Builder(
              builder: (context) {
                final style = states[state] as Map? ?? const {};
                final rgb = style['rgb'];
                final motion = style['motion'];
                return Card(
                  child: Padding(
                    padding: const EdgeInsets.all(12),
                    child: Wrap(
                      spacing: 16,
                      runSpacing: 8,
                      crossAxisAlignment: WrapCrossAlignment.center,
                      children: [
                        SizedBox(
                          width: 60,
                          child: Text(
                            state.toUpperCase(),
                            style: Theme.of(context).textTheme.labelLarge,
                          ),
                        ),
                        OutlinedButton.icon(
                          onPressed:
                              busy || !samePeer || state == 'off' || rgb is! int
                              ? null
                              : () => chooseColor(state, rgb),
                          icon: Icon(
                            Icons.circle,
                            color: rgb is int ? Color(0xff000000 | rgb) : null,
                          ),
                          label: Text(
                            rgb is int
                                ? '#${rgb.toRadixString(16).padLeft(6, '0').toUpperCase()}'
                                : 'Unknown',
                          ),
                        ),
                        if (state != 'off' &&
                            (observed.peer.tableSelection || state != 'idle') &&
                            motion is int &&
                            motion >= 0 &&
                            motion < 4)
                          DropdownButton<int>(
                            value: motion,
                            items: [
                              for (final entry in [
                                'Solid',
                                'Pulse',
                                'Circular chase',
                                'Chase',
                              ].asMap().entries)
                                DropdownMenuItem(
                                  value: entry.key,
                                  child: Text(entry.value),
                                ),
                            ],
                            onChanged: busy || !samePeer
                                ? null
                                : (v) {
                                    if (v != null) {
                                      patch({
                                        'states': {
                                          state: {'motion': v},
                                        },
                                      });
                                    }
                                  },
                          ),
                      ],
                    ),
                  ),
                );
              },
            ),
          for (final field in const {
            'fade_ms': 'Transition (ms)',
            'pulse_ms': 'Pulse period (ms)',
            'ring_ms': 'Circular period (ms)',
            'chase_ms': 'Chase period (ms)',
          }.entries)
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 6),
              child: TextFormField(
                key: ValueKey('${field.key}:${presentation[field.key]}'),
                initialValue:
                    timingDrafts[field.key] ??
                    '${presentation[field.key] ?? ''}',
                enabled: !busy && samePeer,
                keyboardType: TextInputType.number,
                inputFormatters: [FilteringTextInputFormatter.digitsOnly],
                onChanged: (value) =>
                    setState(() => timingDrafts[field.key] = value),
                decoration: InputDecoration(
                  labelText: field.value,
                  suffixIcon: IconButton(
                    tooltip: 'Apply ${field.value}',
                    icon: const Icon(Icons.check),
                    onPressed:
                        busy ||
                            !samePeer ||
                            !timingDrafts.containsKey(field.key)
                        ? null
                        : () {
                            final n = int.tryParse(timingDrafts[field.key]!);
                            if (n == null || n < 0 || n > 65535) {
                              setState(
                                () => error =
                                    'Enter a duration from 0 to 65535 ms',
                              );
                              return;
                            }
                            patch({field.key: n});
                          },
                  ),
                ),
                onFieldSubmitted: (text) {
                  final n = int.tryParse(text);
                  if (n != null) patch({field.key: n});
                },
              ),
            ),
          SwitchListTile(
            title: const Text('Chase affects adjacent pixels'),
            value: presentation['neighbors'] == true,
            onChanged: busy || !samePeer
                ? null
                : (v) => patch({'neighbors': v}),
          ),
          SwitchListTile(
            title: const Text('Bright chasing dot'),
            value: presentation['invert'] == true,
            onChanged: busy || !samePeer ? null : (v) => patch({'invert': v}),
          ),
          if (observed.peer.tableSelection)
            SwitchListTile(
              title: const Text('Diagonal lost-state ripple'),
              value: presentation['diagonal'] == true,
              onChanged: busy || !samePeer
                  ? null
                  : (v) => patch({'diagonal': v}),
            ),
          const Divider(),
          Text(
            'Temporary color test',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          OutlinedButton.icon(
            icon: Icon(
              Icons.palette_outlined,
              color: Color(
                0xff000000 | (int.tryParse(testColor.text, radix: 16) ?? 0),
              ),
            ),
            label: Text(rgbName(int.tryParse(testColor.text, radix: 16) ?? 0)),
            onPressed: busy
                ? null
                : () async {
                    final rgb = await showLedColorPicker(
                      context,
                      initial:
                          int.tryParse(testColor.text, radix: 16) ?? 0x00ff00,
                    );
                    if (rgb != null && mounted) {
                      setState(() => testColor.text = rgbHex(rgb));
                    }
                  },
          ),
          Wrap(
            spacing: 12,
            children: [
              FilledButton.icon(
                icon: const Icon(Icons.visibility_outlined),
                label: const Text('Preview · 5 seconds'),
                onPressed: busy || !samePeer || widget.ids.isEmpty
                    ? null
                    : () {
                        final color = int.tryParse(testColor.text, radix: 16);
                        if (color != null && testColor.text.length == 6) {
                          send('/api/led/preview', {
                            'ids': widget.ids,
                            'rgb': '$color',
                            'duration_ms': '5000',
                          });
                        }
                      },
              ),
              OutlinedButton.icon(
                icon: const Icon(Icons.stop),
                label: const Text('Stop preview'),
                onPressed: busy || !samePeer
                    ? null
                    : () => send('/api/led/preview', {'cancel': 'true'}),
              ),
            ],
          ),
          SizedBox(
            height: 48,
            child: busy
                ? const LinearProgressIndicator()
                : Text(
                    error ?? '',
                    style: TextStyle(
                      color: Theme.of(context).colorScheme.error,
                    ),
                  ),
          ),
        ],
      ),
    );
  }
}
