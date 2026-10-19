import 'package:flutter/material.dart';

import 'icons.dart';
import 'models.dart';
import 'peer_context.dart';
import 'pixel_diagnostic.dart';
import 'table_workspace.dart';
import 'workspace_widgets.dart';

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
  });
  final DeviceStatus? status;
  final bool connected;
  final Future<void> Function(String, [Map<String, String>]) command;
  final VoidCallback openDiagnostics;
  final TableWorkspace? workspace;
  @override
  State<OutputPanel> createState() => _OutputPanelState();
}

class _OutputPanelState extends State<OutputPanel> {
  late final workspace = widget.workspace ?? TableWorkspace();
  bool arranging = false, pending = false;
  final modes = <PeerMode>[];
  String? driver, outputMode;
  bool? outputEnabled;
  String? presentedOwner;

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

  Widget card(String id, int index) {
    final status = widget.status;
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
                          status?.peer.modes.any((m) => m.id == mode.id) == true
                      ? (_) => send('/api/mode', {'value': mode.id})
                      : null,
                ),
            ],
          ),
          if (modes.any((m) => m.id == 'first-wins'))
            Align(
              alignment: AlignmentDirectional.centerStart,
              child: TextButton.icon(
                icon: const Icon(PakarIcons.arrowCounterClockwise),
                label: const Text('Reset round'),
                onPressed:
                    ready &&
                        status?.peer.modes.any((m) => m.id == 'first-wins') ==
                            true
                    ? () => send('/api/game/reset')
                    : null,
              ),
            ),
        ],
        'output' => [
          if (driver != null)
            Text(driver!, style: Theme.of(context).textTheme.bodySmall),
          if (outputEnabled != null)
            SwitchListTile.adaptive(
              contentPadding: EdgeInsets.zero,
              title: const Text('LED output service'),
              value: status?.ledOutputEnabled ?? outputEnabled!,
              onChanged: ready && status?.ledOutputEnabled != null
                  ? (value) => send('/api/led/output', {'enabled': '$value'})
                  : null,
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
                    onSelected: ready && status?.ledOutputMode != null
                        ? (_) => send('/api/led/output', {'mode': mode})
                        : null,
                  ),
                OutlinedButton.icon(
                  icon: const Icon(PakarIcons.broadcast),
                  onPressed: ready && status?.ledOutputMode != null
                      ? () => send('/api/effect', {'value': 'rainbow'})
                      : null,
                  label: const Text('Rainbow test'),
                ),
              ],
            ),
          SizedBox(
            height: 24,
            child: pending ? const Text('Waiting for device…') : null,
          ),
        ],
        _ => [
          Text(
            'Single pixel or six-pixel group',
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
  Widget build(BuildContext context) => Align(
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
        itemCount: workspace.outputCards.length,
        itemBuilder: (_, i) => card(workspace.outputCards[i], i),
        onReorderItem: (oldIndex, newIndex) {
          if (!arranging) return;
          setState(() => workspace.reorderOutputCard(oldIndex, newIndex));
        },
      ),
    ),
  );
}
