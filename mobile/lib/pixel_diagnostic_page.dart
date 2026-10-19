import 'dart:async';

import 'package:flutter/material.dart';

import 'api.dart';
import 'icons.dart';
import 'models.dart';
import 'pixel_diagnostic.dart';
import 'workspace_widgets.dart';

/// Uses the same Studio workspace components as settings/firmware/bridge.
/// No startup writes, automatic chase, hidden restore, or reconnect replay.
class PixelDiagnosticPage extends StatefulWidget {
  const PixelDiagnosticPage({
    super.key,
    required this.api,
    required this.observed,
    required this.busyChanged,
    this.onObserved,
    this.workspace,
    this.mapping,
  });
  final PakarApi api;
  final DeviceStatus? Function() observed;
  final void Function(bool) busyChanged;
  final void Function(DeviceStatus)? onObserved;
  final PixelDiagnosticWorkspace? workspace;
  final CellMap? Function()? mapping;
  @override
  State<PixelDiagnosticPage> createState() => _PixelDiagnosticPageState();
}

class _PixelDiagnosticPageState extends State<PixelDiagnosticPage> {
  late final workspace = widget.workspace ?? PixelDiagnosticWorkspace();
  PixelProbe get probe => workspace.probe;
  set probe(PixelProbe value) => workspace.probe = value;
  late int revision = widget.api.connectionRevision;
  late Object? board = widget.observed()?.json['board_id'];
  late Object? boot = widget.observed()?.json['boot_id'];
  final position = TextEditingController(text: '1');
  late final scroll = ScrollController(
    initialScrollOffset: workspace.vertical,
    keepScrollOffset: false,
  );
  final colorText = TextEditingController();
  final positionFocus = FocusNode();
  StreamSubscription<DeviceStatus>? statusSubscription;
  StreamSubscription<void>? connectionSubscription;
  bool working = false;
  String message =
      'Choose a position and colour, then Show. Nothing is sent automatically.';
  String? applied;
  bool allOff = false;
  @override
  void initState() {
    super.initState();
    // Eagerly capture the owner before navigation or network state can change.
    revision;
    board;
    boot;
    position.text = workspace.position;
    position.addListener(() => workspace.position = position.text);
    scroll.addListener(() {
      if (scroll.hasClients) workspace.vertical = scroll.offset;
    });
    workspace.addListener(workspaceChanged);
    final initial = widget.api.latestStatus ?? widget.observed();
    if (initial != null) observe(initial);
    statusSubscription = widget.api.statusUpdates.listen(observe);
    connectionSubscription = widget.api.connectionUpdates.listen((_) {
      if (mounted) {
        setState(() {
          applied = null;
        });
      }
    });
  }

  void workspaceChanged() {
    if (!mounted) return;
    // A remote observation does not replace an in-progress numeric draft.
    if (!positionFocus.hasFocus && position.text != workspace.position) {
      position.text = workspace.position;
    }
    setState(() {});
  }

  void observe(DeviceStatus status) {
    if (!mounted) return;
    if (revision != widget.api.connectionRevision ||
        status.json['board_id'] != board ||
        status.json['boot_id'] != boot) {
      setState(() => applied = null);
      return;
    }
    if (workspace.observe(status)) {
      setState(() {
        applied = null;
        allOff = workspace.observation?.off == true;
      });
    }
  }

  String? get blockedReason {
    final status = widget.observed();
    if (!widget.api.connectionWanted) {
      return 'Disconnected. Connect a board first.';
    }
    if (!widget.api.appForeground) {
      return 'Return to the app before sending a pixel command.';
    }
    if (status == null) {
      return 'Waiting for fresh board state. No command was sent.';
    }
    if (revision != widget.api.connectionRevision ||
        board != status.json['board_id'] ||
        boot != status.json['boot_id']) {
      return 'The connection or board restarted. Use the current connection to resume testing; no command will be replayed.';
    }
    if (board == null || boot == null) return 'Board identity is unavailable.';
    if (status.json['programming_latch'] == true) {
      return 'AVR programming currently owns the board. Wait for it to finish.';
    }
    if ((status.json['remote_firmware'] as Map?)?['busy'] == true) {
      return 'A firmware update currently owns the board. Wait for it to finish.';
    }
    if (!supportsPixelDiagnostic(status) || !status.peer.has('command-ack')) {
      return 'This board has not acknowledged pixel diagnostics and command receipts.';
    }
    return null;
  }

  bool get authorized => mounted && blockedReason == null;

  bool get canUseCurrentConnection {
    final current = widget.observed();
    return !working &&
        widget.api.connectionWanted &&
        widget.api.appForeground &&
        current?.json['board_id'] is String &&
        current?.json['boot_id'] is String &&
        supportsPixelDiagnostic(current) &&
        current?.peer.has('command-ack') == true &&
        (revision != widget.api.connectionRevision ||
            board != current?.json['board_id'] ||
            boot != current?.json['boot_id']);
  }

  void useCurrentConnection() {
    if (!canUseCurrentConnection) return;
    final current = widget.observed()!;
    setState(() {
      revision = widget.api.connectionRevision;
      board = current.json['board_id'];
      boot = current.json['boot_id'];
      applied = null;
      message =
          'Current connection selected. Press Show to send your pixel test.';
    });
    observe(current);
  }

  @override
  void dispose() {
    unawaited(statusSubscription?.cancel());
    unawaited(connectionSubscription?.cancel());
    workspace.removeListener(workspaceChanged);
    positionFocus.dispose();
    position.dispose();
    colorText.dispose();
    scroll.dispose();
    if (widget.workspace == null) workspace.dispose();
    super.dispose();
  }

  Future<void> send(PixelProbe next, {bool off = false}) async {
    if (working || !authorized) {
      setState(
        () => message = working
            ? 'A pixel command is still pending. Nothing else was sent.'
            : blockedReason ?? 'Pixel testing is unavailable.',
      );
      return;
    }
    FocusScope.of(context).unfocus();
    setState(() {
      working = true;
      message = off ? 'Clearing all 480 pixels…' : 'Showing ${next.range}…';
      applied = null;
    });
    widget.busyChanged(true);
    try {
      final actual = await widget.api.pixelDiagnostic(
        next,
        off: off,
        authorize: () => authorized,
      );
      if (mounted) widget.onObserved?.call(actual);
      if (mounted && authorized) {
        observe(actual);
        setState(() {
          // A newer other-client observation wins over this late ACK.
          if (workspace.observation == null) {
            probe = next;
            allOff = off;
            position.text = '${next.first + 1}';
          }
          applied = workspace.observation != null
              ? null
              : off
              ? 'Last verified: all 480 pixels off'
              : 'Last verified: ${next.range} · #${next.hex}';
          message =
              'Board RGB readback matched. Please check the physical LEDs.';
        });
      }
    } catch (error) {
      if (mounted) {
        setState(() {
          applied = null;
          message = '$error No frame was replayed.';
          position.text = '${probe.first + 1}';
        });
      }
    } finally {
      if (mounted) {
        setState(() => working = false);
        widget.busyChanged(false);
      }
    }
  }

  void change(PixelProbe next) {
    if ((applied != null || workspace.observation?.active == true) && !allOff) {
      unawaited(send(next));
    } else {
      setState(() {
        probe = next;
        position.text = '${next.first + 1}';
      });
    }
  }

  PixelProbe? readPosition() {
    final index = int.tryParse(position.text.trim());
    if (index == null || index < 1 || index > diagnosticPixels) {
      setState(() => message = 'Enter a physical pixel number from 1 to 480.');
      return null;
    }
    return PixelProbe(index: index - 1, group: probe.group, rgb: probe.rgb);
  }

  Future<void> chooseColor() async {
    final text = colorText..text = probe.hex;
    var value = probe.rgb;
    final result = await showDialog<int>(
      context: context,
      builder: (context) => StatefulBuilder(
        builder: (context, update) => AlertDialog(
          scrollable: true,
          title: const Text('Diagnostic colour'),
          content: SizedBox(
            width: 340,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Container(
                  height: 44,
                  decoration: BoxDecoration(
                    color: Color(0xff000000 | value),
                    borderRadius: BorderRadius.circular(8),
                  ),
                ),
                const SizedBox(height: 16),
                TextField(
                  controller: text,
                  maxLength: 7,
                  decoration: const InputDecoration(
                    labelText: 'RGB hex',
                    hintText: 'FF0000',
                  ),
                  onChanged: (input) {
                    final hex = input.replaceFirst('#', '');
                    if (RegExp(r'^[0-9a-fA-F]{6}$').hasMatch(hex)) {
                      update(() => value = int.parse(hex, radix: 16));
                    }
                  },
                ),
                for (final channel in [(16, 'Red'), (8, 'Green'), (0, 'Blue')])
                  Row(
                    children: [
                      SizedBox(width: 50, child: Text(channel.$2)),
                      Expanded(
                        child: Slider(
                          value: ((value >> channel.$1) & 255).toDouble(),
                          max: 255,
                          divisions: 255,
                          onChanged: (v) => update(() {
                            value =
                                value & ~(255 << channel.$1) |
                                v.round() << channel.$1;
                            text.text = value
                                .toRadixString(16)
                                .padLeft(6, '0')
                                .toUpperCase();
                          }),
                        ),
                      ),
                    ],
                  ),
              ],
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () {
                final hex = text.text.replaceFirst('#', '');
                if (RegExp(r'^[0-9a-fA-F]{6}$').hasMatch(hex)) {
                  Navigator.pop(context, int.parse(hex, radix: 16));
                }
              },
              child: const Text('Use colour'),
            ),
          ],
        ),
      ),
    );
    if (result != null && mounted) {
      change(PixelProbe(index: probe.index, group: probe.group, rgb: result));
    }
  }

  @override
  Widget build(BuildContext context) {
    final enabled = authorized && !working;
    final colours = Theme.of(context).colorScheme;
    final map = widget.mapping?.call();
    final group = probe.first ~/ 6 + 1;
    final logical = map?.valid == true
        ? map!.physical.indexOf(group) + 1
        : null;
    final first = probe.index ~/ 6 * 6;
    return WorkspaceBody(
      controller: scroll,
      children: [
        const WorkspaceHeader(
          title: 'Pixel diagnostics',
          icon: PakarIcons.circuitry,
          subtitle: 'Physical wire order · all other pixels cleared',
        ),
        WorkspaceSection(
          title: probe.group ? 'Six-pixel probe' : 'Single-pixel probe',
          icon: PakarIcons.gridFour,
          children: [
            SegmentedButton<bool>(
              segments: const [
                ButtonSegment(value: false, label: Text('1 pixel')),
                ButtonSegment(value: true, label: Text('6-pixel group')),
              ],
              selected: {probe.group},
              onSelectionChanged: enabled
                  ? (choice) => change(
                      PixelProbe(
                        index: probe.index,
                        group: choice.single,
                        rgb: probe.rgb,
                      ),
                    )
                  : null,
            ),
            const SizedBox(height: 16),
            Text(
              probe.range,
              style: Theme.of(context).textTheme.headlineMedium,
            ),
            Text(
              'Physical group $group of 80${logical != null ? ' · logical cell $logical' : ''}',
              style: TextStyle(color: colours.onSurfaceVariant),
            ),
            const SizedBox(height: 12),
            // Tap a sector to inspect any of the six physical pixels directly.
            Row(
              children: [
                for (var sector = 0; sector < 6; sector++)
                  Expanded(
                    child: Tooltip(
                      message: 'Physical pixel ${first + sector + 1}',
                      child: InkWell(
                        key: Key('pixel-sector-$sector'),
                        borderRadius: BorderRadius.circular(8),
                        onTap: enabled
                            ? () => send(
                                PixelProbe(
                                  index: first + sector,
                                  rgb: probe.rgb,
                                ),
                              )
                            : null,
                        child: Padding(
                          padding: const EdgeInsets.symmetric(vertical: 10),
                          child: Column(
                            children: [
                              AnimatedContainer(
                                duration: const Duration(milliseconds: 120),
                                height: 25,
                                width: 25,
                                decoration: BoxDecoration(
                                  shape: BoxShape.circle,
                                  color:
                                      !allOff &&
                                          (probe.group ||
                                              first + sector == probe.index)
                                      ? Color(0xff000000 | probe.rgb)
                                      : colours.surfaceContainerHigh,
                                  border: Border.all(color: colours.outline),
                                ),
                              ),
                              const SizedBox(height: 4),
                              Text(
                                '${first + sector + 1}',
                                style: Theme.of(context).textTheme.labelSmall,
                              ),
                            ],
                          ),
                        ),
                      ),
                    ),
                  ),
              ],
            ),
            const SizedBox(height: 8),
            Row(
              children: [
                Expanded(
                  child: OutlinedButton.icon(
                    key: const Key('pixel-previous'),
                    onPressed: enabled && probe.canPrevious
                        ? () => send(probe.move(-1))
                        : null,
                    icon: const Icon(Icons.arrow_back),
                    label: Text('−${probe.count}'),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: FilledButton.icon(
                    key: const Key('pixel-next'),
                    onPressed: enabled && probe.canNext
                        ? () => send(probe.move(1))
                        : null,
                    icon: const Icon(Icons.arrow_forward),
                    label: Text('+${probe.count}'),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 14),
            Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Expanded(
                  child: TextField(
                    key: const Key('pixel-position'),
                    controller: position,
                    focusNode: positionFocus,
                    enabled: enabled,
                    style: TextStyle(color: colours.onSurface),
                    keyboardType: TextInputType.number,
                    decoration: const InputDecoration(
                      labelText: 'Physical pixel · 1–480',
                    ),
                    onSubmitted: enabled
                        ? (_) {
                            final next = readPosition();
                            if (next != null) send(next);
                          }
                        : null,
                  ),
                ),
                const SizedBox(width: 10),
                FilledButton(
                  key: const Key('pixel-show'),
                  onPressed: enabled
                      ? () {
                          final next = readPosition();
                          if (next != null) send(next);
                        }
                      : null,
                  child: const Text('Show'),
                ),
              ],
            ),
            const SizedBox(height: 14),
            Wrap(
              spacing: 6,
              runSpacing: 6,
              children: [
                for (final item in [
                  (0xffffff, 'White'),
                  (0xff0000, 'Red'),
                  (0x00ff00, 'Green'),
                  (0x0000ff, 'Blue'),
                ])
                  ChoiceChip(
                    label: Text(item.$2),
                    selected: probe.rgb == item.$1,
                    onSelected: enabled
                        ? (_) => change(
                            PixelProbe(
                              index: probe.index,
                              group: probe.group,
                              rgb: item.$1,
                            ),
                          )
                        : null,
                  ),
                ActionChip(
                  avatar: CircleAvatar(
                    backgroundColor: Color(0xff000000 | probe.rgb),
                  ),
                  label: Text('#${probe.hex}'),
                  onPressed: enabled ? chooseColor : null,
                ),
              ],
            ),
            const SizedBox(height: 12),
            OutlinedButton.icon(
              key: const Key('pixel-off'),
              icon: const Icon(PakarIcons.circle),
              label: const Text('All off · clear 480 pixels'),
              onPressed: enabled ? () => send(probe, off: true) : null,
            ),
            const SizedBox(height: 12),
            // Fixed height: pending/confirmed/error never shifts the controls.
            SizedBox(
              height: 4,
              child: working
                  ? const LinearProgressIndicator()
                  : const SizedBox(),
            ),
            const SizedBox(height: 10),
            if (!authorized)
              Text(
                blockedReason ?? 'Pixel testing is unavailable.',
                style: TextStyle(color: colours.error),
              ),
            if (canUseCurrentConnection)
              TextButton.icon(
                key: const Key('pixel-use-current-connection'),
                onPressed: useCurrentConnection,
                icon: const Icon(PakarIcons.link),
                label: const Text('Use current connection'),
              ),
            if (applied != null && authorized)
              Text(applied!, style: TextStyle(color: colours.secondary)),
            if (workspace.observation != null && authorized)
              Text(
                workspace.observation!.label,
                style: TextStyle(color: colours.secondary),
              ),
            Text(message, style: Theme.of(context).textTheme.bodySmall),
          ],
        ),
        WorkspaceSection(
          title: 'Wire reference',
          icon: PakarIcons.circuitry,
          children: [
            Text(
              'Zero-based index ${probe.first}${probe.group ? '–${probe.last}' : ''} · step ${probe.count}',
            ),
            Text(
              'RGB #${probe.hex} · R ${probe.rgb >> 16 & 255} · G ${probe.rgb >> 8 & 255} · B ${probe.rgb & 255}',
            ),
            const SizedBox(height: 8),
            const Text(
              'Show clears every other pixel. After Show, colour and group changes apply immediately. All off keeps your cursor. Closing leaves the last frame on the board.',
            ),
            if (widget.observed()?.peer.has('pixel-diagnostic') != true)
              const Padding(
                padding: EdgeInsets.only(top: 8),
                child: Text(
                  'Older board firmware: WebSocket streaming is required. Update firmware for atomic diagnostics and HTTP fallback.',
                ),
              ),
          ],
        ),
      ],
    );
  }
}
