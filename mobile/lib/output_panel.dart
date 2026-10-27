import 'dart:convert';

import 'package:flutter/material.dart';

import 'game_presentation_editor.dart';
import 'game_settings_command.dart';
import 'state_appearance_page.dart';

import 'icons.dart';
import 'models.dart';
import 'key_pixel_preview.dart';
import 'peer_context.dart';
import 'pixel_diagnostic.dart';
import 'table_workspace.dart';
import 'workspace_widgets.dart';
import 'led_backend.dart';
import 'led_backend_control.dart';

/// Pealayer layout.rs: hardware_channel_drag_handle/drop_target and
/// effect_library_card_header: stable keyed cards, a reserved action gutter,
/// handle-owned reordering, and no command caused by arranging the workspace.
/// Flutter renders these semantics; egui widgets are not byte-compatible.
class OutputPanel extends StatefulWidget {
  const OutputPanel({
    super.key,
    required this.status,
    required this.connected,
    required this.command,
    required this.openDiagnostics,
    this.workspace,
    this.verifiedCommand,
    this.readStatus,
  });
  final DeviceStatus? status;
  final bool connected;
  final Future<void> Function(String, [Map<String, String>]) command;
  final VoidCallback openDiagnostics;
  final TableWorkspace? workspace;
  final Future<bool> Function(String, Map<String, String>)? verifiedCommand;
  final Future<DeviceStatus> Function()? readStatus;
  @override
  State<OutputPanel> createState() => _OutputPanelState();
}

class _OutputPanelState extends State<OutputPanel> {
  late final workspace = widget.workspace ?? TableWorkspace();
  bool arranging = false, arrangingKeys = false, pending = false;
  final modes = <PeerMode>[];
  String? driver, outputMode;
  bool? outputEnabled;
  double? brightnessDraft;
  String? presentedOwner;
  bool hasSceneContent = false;

  void remember() {
    final peer = widget.status?.peer;
    if (peer?.known == true) {
      final owner =
          '${widget.status?.json['board_id'] ?? peer!.hostname}/${peer!.kind}/${peer.application?.id}/${peer.role}';
      if (presentedOwner != null && owner != presentedOwner) {
        modes.clear();
        driver = outputMode = null;
        outputEnabled = null;
      }
      presentedOwner = owner;
      modes.clear();
    }
    for (final mode in widget.status?.peer.modes ?? const <PeerMode>[]) {
      final i = modes.indexWhere((item) => item.id == mode.id);
      if (i < 0) {
        modes.add(mode);
      } else {
        modes[i] = mode;
      }
    }
    driver = widget.status?.peer.ledOutput?.label ?? driver;
    outputMode = widget.status?.ledOutputMode ?? outputMode;
    outputEnabled = widget.status?.ledOutputEnabled ?? outputEnabled;
    if (peer?.known == true) {
      hasSceneContent =
          peer?.role == 'first-wins' ||
          modes.length > 1 ||
          modes.any((mode) => mode.id == 'first-wins') ||
          (peer?.has('key-settings') == true &&
              validGameConfig(widget.status?.json['key_game']));
    }
  }

  @override
  void initState() {
    super.initState();
    remember();
  }

  @override
  void didUpdateWidget(covariant OutputPanel oldWidget) {
    super.didUpdateWidget(oldWidget);
    remember();
  }

  @override
  void dispose() {
    if (widget.workspace == null) workspace.dispose();
    super.dispose();
  }

  bool get ready => widget.connected && !pending;
  bool get ledsReady => ready && widget.status?.peer.has('leds') == true;
  Future<void> send(
    String path, [
    Map<String, String> values = const {},
  ]) async {
    if (!ready) return;
    setState(() => pending = true);
    try {
      await widget.command(path, values);
    } finally {
      if (mounted) setState(() => pending = false);
    }
  }

  Future<void> gameSettings() async {
    final source = widget.status;
    final config = source?.json['key_game'] as Map?;
    if (config == null || !validGameConfig(config)) return;
    final board = source?.json['board_id'], boot = source?.json['boot_id'];
    if (board is! String || boot is! String) return;
    var mask = config['enabled_mask'] as int;
    var extendedEffects = config['extended_effects'] as bool;
    var chasePeriodMs = (config['chase_period_ms'] as int?) ?? 900;
    var chaseNeighbors = (config['chase_neighbors'] as bool?) ?? true;
    var chaseInvert = (config['chase_invert'] as bool?) ?? false;
    var autoResetMs = config['auto_reset_ms'] as int;
    final melodies = <String, String>{
      for (final name in ['winner_cue', 'reset_cue'])
        if (source!.peer.has('game-melodies') && config[name] is String)
          name: config[name] as String,
    };
    final availableCues = {'silent', ...source!.audioCues};
    var autoResetValid = true;
    final colors = {
      for (final entry in {
        'idle_rgb': 0xffffff,
        'win_rgb': 0x00ff00,
        'lose_rgb': 0xff0000,
      }.entries)
        entry.key: TextEditingController(
          text: ((config[entry.key] as int?) ?? entry.value)
              .toRadixString(16)
              .padLeft(6, '0')
              .toUpperCase(),
        ),
    };
    final result = await showDialog<int>(
      context: context,
      builder: (context) => StatefulBuilder(
        builder: (context, update) => AlertDialog(
          title: const Text('Game settings'),
          content: SizedBox(
            width: 420,
            child: SingleChildScrollView(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  if (source.peer.has('state-presentation') &&
                      widget.verifiedCommand != null &&
                      widget.readStatus != null)
                    TextButton.icon(
                      icon: const Icon(Icons.palette_outlined),
                      label: const Text('State colors & effects…'),
                      onPressed: () => Navigator.of(context).push(
                        MaterialPageRoute<void>(
                          builder: (_) => StateAppearancePage(
                            status: source,
                            command: widget.verifiedCommand!,
                            readStatus: widget.readStatus!,
                            ids: '1-18',
                          ),
                        ),
                      ),
                    ),
                  if (source.peer.has('game-presentation') &&
                      config['presentation'] is Map)
                    TextButton.icon(
                      icon: const Icon(Icons.animation),
                      label: const Text('Round presentation…'),
                      onPressed: () async {
                        final patch = await showDialog<Map<String, Object>>(
                          context: context,
                          builder: (_) => GamePresentationEditor(
                            presentation: config['presentation'] as Map,
                          ),
                        );
                        if (patch == null || !mounted || !ready) return;
                        final now = widget.status;
                        final actual =
                            (now?.json['key_game'] as Map?)?['presentation'];
                        if (now?.json['board_id'] != board ||
                            now?.json['boot_id'] != boot ||
                            !presentationMatches(
                              actual,
                              config['presentation'],
                            )) {
                          if (context.mounted) {
                            ScaffoldMessenger.of(context).showSnackBar(
                              const SnackBar(
                                content: Text(
                                  'Presentation changed on the board. Reopen settings to review.',
                                ),
                              ),
                            );
                          }
                          return;
                        }
                        await send('/api/game/settings', {
                          'presentation': jsonEncode(patch),
                        });
                      },
                    ),
                  Wrap(
                    spacing: 8,
                    children: [
                      for (final count in [2, 4, 9])
                        ChoiceChip(
                          label: Text('$count boxes'),
                          selected: mask == (1 << count) - 1,
                          onSelected: (_) =>
                              update(() => mask = (1 << count) - 1),
                        ),
                    ],
                  ),
                  const SizedBox(height: 16),
                  Wrap(
                    children: [
                      for (var key = 1; key <= 9; key++)
                        SizedBox(
                          width: 120,
                          child: CheckboxListTile(
                            title: Text('Box $key'),
                            contentPadding: EdgeInsets.zero,
                            value: mask & (1 << (key - 1)) != 0,
                            onChanged: (value) => update(() {
                              if (value == true) {
                                mask |= 1 << (key - 1);
                              } else {
                                mask &= ~(1 << (key - 1));
                              }
                            }),
                          ),
                        ),
                    ],
                  ),
                  const Divider(),
                  for (final name in melodies.keys)
                    Padding(
                      padding: const EdgeInsets.only(bottom: 12),
                      child: Row(
                        children: [
                          Expanded(
                            child: DropdownButtonFormField<String>(
                              initialValue: melodies[name],
                              decoration: InputDecoration(
                                labelText: name == 'winner_cue'
                                    ? 'Winner melody'
                                    : 'Reset melody',
                              ),
                              items: {...availableCues, melodies[name]!}
                                  .map(
                                    (cue) => DropdownMenuItem(
                                      value: cue,
                                      child: Text(
                                        cue == 'silent'
                                            ? 'Silent'
                                            : cue.replaceAll('-', ' '),
                                      ),
                                    ),
                                  )
                                  .toList(),
                              onChanged: (cue) {
                                if (cue != null) {
                                  update(() => melodies[name] = cue);
                                }
                              },
                            ),
                          ),
                          IconButton(
                            tooltip: 'Preview on board',
                            icon: const Icon(Icons.play_arrow),
                            onPressed:
                                melodies[name] == 'silent' ||
                                    !availableCues.contains(melodies[name])
                                ? null
                                : () => send('/api/audio', {
                                    'cue': melodies[name]!,
                                  }),
                          ),
                        ],
                      ),
                    ),
                  if (config.containsKey('auto_reset_ms')) ...[
                    SwitchListTile(
                      title: const Text('Automatic round reset'),
                      value: autoResetMs > 0,
                      onChanged: (enabled) => update(() {
                        autoResetMs = enabled ? 10000 : 0;
                        autoResetValid = true;
                      }),
                    ),
                    if (autoResetMs > 0)
                      TextFormField(
                        initialValue: '${autoResetMs / 1000}',
                        keyboardType: const TextInputType.numberWithOptions(
                          decimal: true,
                        ),
                        decoration: InputDecoration(
                          labelText: 'Reset after (seconds)',
                          errorText: autoResetValid
                              ? null
                              : 'Enter 0.001–86400 seconds',
                        ),
                        onChanged: (text) => update(() {
                          final seconds = double.tryParse(text);
                          autoResetValid =
                              seconds != null &&
                              seconds.isFinite &&
                              seconds >= 0.001 &&
                              seconds <= 86400;
                          if (autoResetValid) {
                            autoResetMs = (seconds! * 1000).round();
                          }
                        }),
                      ),
                  ],
                  if (config.containsKey('extended_effects'))
                    SwitchListTile(
                      title: const Text('Extended effects'),
                      value: extendedEffects,
                      onChanged: (value) =>
                          update(() => extendedEffects = value),
                    ),
                  if (config['chase_period_ms'] is int) ...[
                    ListTile(
                      title: const Text('Chase lap'),
                      subtitle: Text('$chasePeriodMs ms · illumination group'),
                    ),
                    Slider(
                      value: chasePeriodMs.clamp(400, 3000).toDouble(),
                      min: 400,
                      max: 3000,
                      divisions: 52,
                      onChanged: (value) =>
                          update(() => chasePeriodMs = value.round()),
                    ),
                  ],
                  if (config['chase_neighbors'] is bool)
                    SwitchListTile(
                      title: const Text('Affect adjacent pixels'),
                      value: chaseNeighbors,
                      onChanged: (value) =>
                          update(() => chaseNeighbors = value),
                    ),
                  if (config['chase_invert'] is bool)
                    SwitchListTile(
                      title: const Text('Bright dot instead of dim dot'),
                      value: chaseInvert,
                      onChanged: (value) => update(() => chaseInvert = value),
                    ),
                  for (final entry in {
                    'idle_rgb': 'Idle',
                    'win_rgb': 'Winner',
                    'lose_rgb': 'Other boxes',
                  }.entries)
                    Padding(
                      padding: const EdgeInsets.only(top: 12),
                      child: TextField(
                        controller: colors[entry.key],
                        maxLength: 6,
                        decoration: InputDecoration(
                          labelText: entry.value,
                          prefixText: '#',
                          counterText: '',
                          errorText:
                              RegExp(r'^[0-9a-fA-F]{6}$')
                                  .hasMatch(colors[entry.key]!.text)
                              ? null
                              : 'Six hexadecimal digits',
                        ),
                        onChanged: (_) => update(() {}),
                      ),
                    ),
                ],
              ),
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed:
                  mask == 0 ||
                      !autoResetValid ||
                      colors.values.any(
                        (c) => !RegExp(r'^[0-9a-fA-F]{6}$').hasMatch(c.text),
                      )
                  ? null
                  : () => Navigator.pop(context, mask),
              child: const Text('Save'),
            ),
          ],
        ),
      ),
    );
    final values = <String, String>{};
    if (result != null) {
      for (final entry in melodies.entries) {
        if (entry.value != config[entry.key]) values[entry.key] = entry.value;
      }
      if (result != config['enabled_mask']) values['enabled_mask'] = '$result';
      if (autoResetMs != config['auto_reset_ms']) {
        values['auto_reset_ms'] = '$autoResetMs';
      }
      if (extendedEffects != config['extended_effects']) {
        values['extended_effects'] = '$extendedEffects';
      }
      if (config['chase_period_ms'] is int &&
          chasePeriodMs != config['chase_period_ms']) {
        values['chase_period_ms'] = '$chasePeriodMs';
      }
      if (config['chase_neighbors'] is bool &&
          chaseNeighbors != config['chase_neighbors']) {
        values['chase_neighbors'] = '$chaseNeighbors';
      }
      if (config['chase_invert'] is bool &&
          chaseInvert != config['chase_invert']) {
        values['chase_invert'] = '$chaseInvert';
      }
      for (final entry in colors.entries) {
        final chosen = int.tryParse(entry.value.text, radix: 16);
        if (chosen != null && chosen != config[entry.key]) {
          values[entry.key] = '$chosen';
        }
      }
    }
    for (final controller in colors.values) {
      controller.dispose();
    }
    if (result != null &&
        values.isNotEmpty &&
        mounted &&
        ready &&
        widget.status?.json['board_id'] == board &&
        widget.status?.json['boot_id'] == boot &&
        widget.status?.peer.role == 'first-wins') {
      final current = widget.status?.json['key_game'];
      if (current is! Map ||
          values.keys.any((key) => current[key] != config[key])) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text(
              'Game settings changed on the board. Review the current values before saving.',
            ),
          ),
        );
        return;
      }
      await send('/api/game/settings', values);
    }
  }

  bool validGameConfig(Object? raw) =>
      raw is Map &&
      raw['enabled_mask'] is int &&
      raw['enabled_mask'] >= 1 &&
      raw['enabled_mask'] <= 511 &&
      raw['extended_effects'] is bool &&
      raw['auto_reset_ms'] is int &&
      raw['auto_reset_ms'] >= 0 &&
      raw['auto_reset_ms'] <= 86400000 &&
      const ['idle_rgb', 'win_rgb', 'lose_rgb'].every(
        (key) => raw[key] is int && raw[key] >= 0 && raw[key] <= 0xffffff,
      );

  bool keyEnabled(int key) {
    final config = widget.status?.json['key_game'];
    final mask = config is Map ? config['enabled_mask'] : null;
    return mask is int && mask & (1 << (key - 1)) != 0;
  }

  Color keyColor(int key) {
    final config = widget.status?.json['key_game'];
    final winner = widget.status?.winner ?? 0;
    final field = winner == 0
        ? 'idle_rgb'
        : winner == key
        ? 'win_rgb'
        : 'lose_rgb';
    final fallback = winner == 0
        ? 0xffffff
        : winner == key
        ? 0x00ff00
        : 0xff0000;
    final value = config is Map ? config[field] : null;
    return Color(0xff000000 | (value is int ? value : fallback));
  }

  Widget card(String id, int index) {
    final status = widget.status;
    final backend = status == null
        ? null
        : LedBackendState.fromStatus(status.json);
    final (title, icon) = switch (id) {
      'scene' => ('Scene', PakarIcons.gridFour),
      'output' => ('LED output', PakarIcons.gauge),
      _ => ('Pixel diagnostics', PakarIcons.circuitry),
    };
    return WorkspaceSection(
      key: ValueKey('output-card-$id'),
      title: title,
      icon: icon,
      trailing: SizedBox(
        width: 40,
        height: 40,
        child: arranging
            ? ReorderableDragStartListener(
                index: index,
                child: const Tooltip(
                  message: 'Drag to reorder panel',
                  child: Icon(Icons.drag_indicator),
                ),
              )
            : null,
      ),
      children: switch (id) {
        'scene' => [
          if (status?.peer.role == 'first-wins')
            Padding(
              padding: const EdgeInsets.only(bottom: 12),
              child: Text(
                status?.winner == 0
                    ? 'Ready for first press'
                    : 'Winner · Box ${status?.winner}',
                style: Theme.of(context).textTheme.titleMedium,
              ),
            ),
          if (status?.peer.role == 'first-wins')
            Row(
              children: [
                FilterChip(
                  avatar: const Icon(Icons.open_with, size: 18),
                  label: const Text('Arrange boxes'),
                  selected: arrangingKeys,
                  onSelected: (value) => setState(() => arrangingKeys = value),
                ),
                const SizedBox(width: 8),
                if (arrangingKeys)
                  TextButton.icon(
                    icon: const Icon(Icons.restart_alt),
                    label: const Text('Reset positions'),
                    onPressed: () => setState(workspace.resetKeyPositions),
                  ),
              ],
            ),
          if (status?.peer.role == 'first-wins')
            LayoutBuilder(
              builder: (context, bounds) {
                const size = 88.0, height = 300.0;
                final width = bounds.maxWidth.isFinite
                    ? bounds.maxWidth
                    : 360.0;
                final maxX = width > size ? width - size : 0.0;
                const maxY = height - size;
                return Container(
                  key: const ValueKey('key-layout-canvas'),
                  height: height,
                  decoration: BoxDecoration(
                    color: Theme.of(context).colorScheme.surfaceContainerLowest,
                    borderRadius: BorderRadius.circular(20),
                    border: Border.all(
                      color: arrangingKeys
                          ? Theme.of(context).colorScheme.primary
                          : Theme.of(context).dividerColor,
                    ),
                  ),
                  child: Stack(
                    clipBehavior: Clip.none,
                    children: [
                      for (var key = 1; key <= 9; key++)
                        AnimatedPositioned(
                          key: ValueKey('game-key-position-$key'),
                          duration: arrangingKeys
                              ? Duration.zero
                              : const Duration(milliseconds: 160),
                          curve: Curves.easeOutCubic,
                          left: workspace.keyPosition(key).dx * maxX,
                          top: workspace.keyPosition(key).dy * maxY,
                          width: size,
                          height: size,
                          child: Semantics(
                            label: 'Key $key',
                            selected: status?.winner == key,
                            child: GestureDetector(
                              key: ValueKey('game-key-drag-$key'),
                              behavior: HitTestBehavior.opaque,
                              onTap:
                                  !arrangingKeys &&
                                      ready &&
                                      keyEnabled(key) &&
                                      status?.peer.has('game-press') == true &&
                                      status?.avrOnline == true &&
                                      status?.json['key_mask_applied'] == true
                                  ? () =>
                                        send('/api/game/press', {'key': '$key'})
                                  : null,
                              onPanUpdate: !arrangingKeys
                                  ? null
                                  : (details) {
                                      final current = workspace.keyPosition(
                                        key,
                                      );
                                      workspace.moveKey(
                                        key,
                                        Offset(
                                          maxX == 0
                                              ? 0
                                              : current.dx +
                                                    details.delta.dx / maxX,
                                          current.dy + details.delta.dy / maxY,
                                        ),
                                      );
                                      setState(() {});
                                    },
                              child: MouseRegion(
                                cursor: arrangingKeys
                                    ? SystemMouseCursors.move
                                    : MouseCursor.defer,
                                child: Opacity(
                                  opacity: keyEnabled(key) ? 1 : 0.3,
                                  child: AnimatedContainer(
                                    key: ValueKey('game-key-$key'),
                                    duration: const Duration(milliseconds: 120),
                                    alignment: Alignment.center,
                                    decoration: BoxDecoration(
                                      color: Color.alphaBlend(
                                        keyColor(key).withValues(alpha: 0.3),
                                        Theme.of(context)
                                            .colorScheme
                                            .surfaceContainerHighest,
                                      ),
                                      borderRadius: BorderRadius.circular(20),
                                      border: Border.all(
                                        width: arrangingKeys ? 2 : 3,
                                        color: arrangingKeys
                                            ? Theme.of(context)
                                                  .colorScheme
                                                  .outlineVariant
                                            : status?.peer.has('inputs') ==
                                                      true &&
                                                  (key <= 8
                                                      ? ((status?.activeInputs ??
                                                                    0) &
                                                                (1 <<
                                                                    (key -
                                                                        1))) !=
                                                            0
                                                      : status?.json['input_9'] ==
                                                            true)
                                            ? Theme.of(context)
                                                  .colorScheme
                                                  .primary
                                            : Colors.transparent,
                                      ),
                                    ),
                                    child: Column(
                                      mainAxisAlignment:
                                          MainAxisAlignment.center,
                                      children: [
                                        if (arrangingKeys)
                                          const Icon(
                                            Icons.drag_indicator,
                                            size: 16,
                                          ),
                                        Text(
                                          '$key',
                                          style: Theme.of(context)
                                              .textTheme
                                              .headlineSmall,
                                        ),
                                        for (var group = 0; group < 2; group++)
                                          SizedBox(
                                            height: 6,
                                            child: Row(
                                              mainAxisAlignment:
                                                  MainAxisAlignment.center,
                                              children: [
                                                for (final rgb
                                                    in keyPixelGroup(
                                                          status,
                                                          key,
                                                          group,
                                                        ) ??
                                                        const <int>[])
                                                  Expanded(
                                                    child: Container(
                                                      height: 4,
                                                      margin:
                                                          const EdgeInsets.only(
                                                            right: 1,
                                                          ),
                                                      color: Color(
                                                        0xff000000 | rgb,
                                                      ),
                                                    ),
                                                  ),
                                              ],
                                            ),
                                          ),
                                      ],
                                    ),
                                  ),
                                ),
                              ),
                            ),
                          ),
                        ),
                    ],
                  ),
                );
              },
            ),
          if (modes.length > 1)
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                for (final mode in modes)
                  ChoiceChip(
                    label: Text(mode.label),
                    selected: status?.mode == mode.id,
                    onSelected:
                        ready &&
                            status?.peer.modes.any((m) => m.id == mode.id) ==
                                true
                        ? (_) => send('/api/mode', {'value': mode.id})
                        : null,
                  ),
              ],
            ),
          if (modes.any((m) => m.id == 'first-wins'))
            Align(
              alignment: AlignmentDirectional.centerStart,
              child: FilledButton.icon(
                icon: const Icon(PakarIcons.arrowCounterClockwise),
                label: const Text('Reset to white'),
                onPressed:
                    ready &&
                        status?.peer.modes.any((m) => m.id == 'first-wins') ==
                            true &&
                        status?.avrOnline == true &&
                        status?.json['key_mask_applied'] == true
                    ? () => send('/api/game/reset')
                    : null,
              ),
            ),
          if (status?.peer.has('key-settings') == true &&
              validGameConfig(status?.json['key_game']))
            Align(
              alignment: AlignmentDirectional.centerStart,
              child: TextButton.icon(
                icon: const Icon(Icons.tune),
                label: const Text('Game settings'),
                onPressed: ready ? gameSettings : null,
              ),
            ),
          if (status?.peer.role == 'first-wins' &&
              status?.json['key_mask_applied'] == false)
            Text(
              'Waiting for AVR to apply enabled boxes',
              style: Theme.of(context).textTheme.bodySmall,
            ),
        ],
        'output' => [
          if (backend != null)
            LedBackendControl(
              state: backend,
              enabled: ledsReady,
              onChanged: (value) => send('/api/led/output', {'backend': value}),
            )
          else if (driver != null)
            Text(driver!, style: Theme.of(context).textTheme.bodySmall),
          if (outputEnabled != null)
            SwitchListTile.adaptive(
              contentPadding: EdgeInsets.zero,
              title: const Text('LED output service'),
              value: status?.ledOutputEnabled ?? outputEnabled!,
              onChanged: ledsReady && status?.ledOutputEnabled != null
                  ? (value) => send('/api/led/output', {'enabled': '$value'})
                  : null,
            ),
          Align(
            alignment: AlignmentDirectional.centerStart,
            child: Tooltip(
              message: 'Send black and stop LED output',
              child: TextButton.icon(
                key: const ValueKey('output-off'),
                icon: const Icon(Icons.power_settings_new),
                label: const Text('Off'),
                onPressed: ledsReady && status?.ledOutputEnabled != null
                    ? () => send('/api/led/output', {'enabled': 'false'})
                    : null,
              ),
            ),
          ),
          if (outputMode != null)
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                for (final mode in const ['static', 'effects'])
                  ChoiceChip(
                    label: Text(mode == 'static' ? 'Static' : 'Effects'),
                    selected: (status?.ledOutputMode ?? outputMode) == mode,
                    onSelected: ledsReady && status?.ledOutputMode != null
                        ? (_) => send('/api/led/output', {'mode': mode})
                        : null,
                  ),
                OutlinedButton.icon(
                  icon: const Icon(PakarIcons.broadcast),
                  onPressed: ledsReady && status?.ledOutputEnabled == true
                      ? () => send('/api/effect', {'value': 'rainbow'})
                      : null,
                  label: const Text('Rainbow test'),
                ),
                for (final (effect, label, icon) in const [
                  ('none', 'State colors', Icons.palette_outlined),
                  ('fire', 'Fire', Icons.local_fire_department_outlined),
                ])
                  ChoiceChip(
                    avatar: Icon(icon, size: 18),
                    label: Text(label),
                    selected: status?.effect == effect,
                    onSelected: ledsReady && status?.ledOutputMode != null
                        ? (_) => send('/api/effect', {'value': effect})
                        : null,
                  ),
              ],
            ),
          if (status?.peer.has('leds') == true &&
              status?.json['brightness'] is int) ...[
            const SizedBox(height: 12),
            WorkspaceValueRow(
              label: 'Brightness',
              value:
                  '${((brightnessDraft ?? status!.brightness.toDouble()) / 255 * 100).round()}%',
            ),
            Slider(
              key: const ValueKey('output-brightness'),
              value: (brightnessDraft ?? status!.brightness.toDouble()).clamp(
                1,
                255,
              ),
              min: 1,
              max: 255,
              divisions: 254,
              label:
                  '${(brightnessDraft ?? status!.brightness.toDouble()).round()}',
              onChanged: ledsReady
                  ? (value) => setState(() => brightnessDraft = value)
                  : null,
              onChangeEnd: !ledsReady
                  ? null
                  : (value) async {
                      try {
                        await send('/api/brightness', {
                          'value': '${value.round()}',
                        });
                      } finally {
                        if (mounted) setState(() => brightnessDraft = null);
                      }
                    },
            ),
          ],
          SizedBox(
            height: 24,
            child: pending ? const Text('Waiting for device…') : null,
          ),
        ],
        _ => [
          Text(
            status?.peer.role == 'first-wins'
                ? 'Single pixel or 12-pixel group'
                : 'Single pixel or six-pixel group',
            style: Theme.of(context).textTheme.bodySmall,
          ),
          const SizedBox(height: 12),
          Align(
            alignment: AlignmentDirectional.centerStart,
            child: FilledButton.tonalIcon(
              icon: const Icon(PakarIcons.circuitry),
              label: const Text('Open pixel diagnostics'),
              onPressed:
                  ready &&
                      supportsPixelDiagnostic(status) &&
                      status?.peer.has('command-ack') == true
                  ? widget.openDiagnostics
                  : null,
            ),
          ),
        ],
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    final visibleCards = workspace.outputCards
        .where((id) => id != 'scene' || hasSceneContent)
        .toList();
    return Align(
      alignment: Alignment.topCenter,
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 1120),
        child: ReorderableListView.builder(
          key: const PageStorageKey('output-panel-scroll'),
          padding: const EdgeInsets.all(20),
          buildDefaultDragHandles: false,
          header: WorkspaceHeader(
            title: 'Output',
            icon: PakarIcons.gauge,
            actions: [
              FilterChip(
                label: const Text('Arrange panels'),
                selected: arranging,
                onSelected: (value) => setState(() => arranging = value),
              ),
            ],
          ),
          itemCount: visibleCards.length,
          itemBuilder: (_, i) => card(visibleCards[i], i),
          onReorderItem: (oldIndex, newIndex) {
            if (!arranging) return;
            final storedCards = workspace.outputCards;
            setState(
              () => workspace.reorderOutputCard(
                storedCards.indexOf(visibleCards[oldIndex]),
                storedCards.indexOf(visibleCards[newIndex]),
              ),
            );
          },
        ),
      ),
    );
  }
}
