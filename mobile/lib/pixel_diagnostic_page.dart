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
  late Object? board =
      (widget.api.freshStatus ?? widget.observed())?.json['board_id'];
  late Object? boot =
      (widget.api.freshStatus ?? widget.observed())?.json['boot_id'];
  final position = TextEditingController(text: '1');
  late final scroll = ScrollController(
    initialScrollOffset: workspace.vertical,
    keepScrollOffset: false,
  );
  final colorText = TextEditingController();
  final positionFocus = FocusNode();
  StreamSubscription<DeviceStatus>? statusSubscription;
  StreamSubscription<void>? connectionSubscription;
  Timer? authorityExpiry;
  void Function()? releaseUsbPixels;
  bool working = false;
  bool refreshing = false;
  String message =
      'Choose a position and colour, then Show. Nothing is sent automatically.';
  String? applied;
  bool allOff = false;
  @override
  void initState() {
    super.initState();
    releaseUsbPixels = widget.api.retainUsbPixels();
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
    final initial = widget.api.freshStatus ?? widget.observed();
    if (initial != null) observe(initial);
    statusSubscription = widget.api.statusUpdates.listen(observe);
    connectionSubscription = widget.api.connectionUpdates.listen((_) {
      if (mounted) {
        setState(() {
          applied = null;
        });
        armAuthorityExpiry();
      }
    });
  }

  void armAuthorityExpiry() {
    authorityExpiry?.cancel();
    final age = widget.api.statusObservationAge;
    if (widget.api.freshStatus == null || age == null) return;
    final remaining = const Duration(seconds: 6) - age;
    authorityExpiry = Timer(
      remaining.isNegative ? Duration.zero : remaining,
      () {
        if (mounted) setState(() => applied = null);
      },
    );
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
    // Adapt PeerSession's generation-fenced refresh: the first full, verified
    // observation may arrive after this route opens. It binds an empty owner,
    // never replaces an existing board/boot or replays a diagnostic command.
    final fresh = widget.api.freshStatus;
    if (board == null &&
        boot == null &&
        revision == widget.api.connectionRevision &&
        fresh != null &&
        fresh.json['board_id'] is String &&
        fresh.json['boot_id'] is String &&
        (fresh.json['board_id'] as String).isNotEmpty &&
        (fresh.json['boot_id'] as String).isNotEmpty &&
        fresh.json['board_id'] == status.json['board_id'] &&
        fresh.json['boot_id'] == status.json['boot_id']) {
      board = fresh.json['board_id'];
      boot = fresh.json['boot_id'];
    }
    if (revision != widget.api.connectionRevision ||
        status.json['board_id'] != board ||
        status.json['boot_id'] != boot) {
      setState(() => applied = null);
      armAuthorityExpiry();
      return;
    }
    final changed = workspace.observe(status);
    final observation = workspace.observation;
    final observedOff = observation != null
        ? observation.active && observation.off
        : changed
        ? false
        : allOff;
    // A retained workspace may already contain this exact revision. A newly
    // opened page still needs its off state, without replaying any action.
    // Every full status can restore/retire command authority even when the
    // diagnostic revision is unchanged. A covered parent route is not the
    // authority source and need not rebuild this page for recovery.
    setState(() {
      if (changed) applied = null;
      allOff = observedOff;
    });
    armAuthorityExpiry();
  }

  String? get blockedReason {
    final status = widget.api.freshStatus;
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
    final current = widget.api.freshStatus;
    return !working &&
        !refreshing &&
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
    final current = widget.api.freshStatus!;
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

  Future<void> refreshState() async {
    if (refreshing ||
        working ||
        !widget.api.connectionWanted ||
        !widget.api.appForeground) {
      return;
    }
    final owner = widget.api.connectionRevision;
    setState(() => refreshing = true);
    try {
      // Join the API's bounded, coalesced read; this is not a command retry,
      // discovery, firmware job or permission to adopt a replacement peer.
      final actual = await widget.api.status();
      if (!mounted ||
          owner != widget.api.connectionRevision ||
          !widget.api.connectionWanted ||
          !widget.api.appForeground) {
        return;
      }
      observe(actual);
      widget.onObserved?.call(actual);
      setState(
        () => message = authorized
            ? 'Fresh board state received. No pixel command was sent.'
            : blockedReason ?? 'Pixel testing is unavailable.',
      );
    } catch (error) {
      if (mounted &&
          owner == widget.api.connectionRevision &&
          widget.api.connectionWanted &&
          widget.api.appForeground) {
        setState(
          () => message =
              'State refresh failed: $error No pixel command was sent.',
        );
      }
    } finally {
      if (mounted) setState(() => refreshing = false);
    }
  }

  @override
  void dispose() {
    releaseUsbPixels?.call();
    unawaited(statusSubscription?.cancel());
    unawaited(connectionSubscription?.cancel());
    authorityExpiry?.cancel();
    workspace.removeListener(workspaceChanged);
    positionFocus.dispose();
    position.dispose();
    colorText.dispose();
    scroll.dispose();
    if (widget.workspace == null) workspace.dispose();
    super.dispose();
  }

  Future<void> send(PixelProbe next, {bool off = false}) async {
    if (working || refreshing || !authorized) {
      setState(
        () => message = working
            ? 'A pixel command is still pending. Nothing else was sent.'
            : refreshing
            ? 'State refresh is pending. No pixel command was sent.'
            : blockedReason ?? 'Pixel testing is unavailable.',
      );
      return;
    }
    FocusScope.of(context).unfocus();
    setState(() {
      working = true;
      message = off ? 'Clearing all ${next.pixels} pixels…' : 'Showing ${next.range}…';
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
              ? 'Last verified: all ${next.pixels} pixels off'
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
    if (index == null || index < 1 || index > probe.pixels) {
      setState(() => message = 'Enter a physical pixel number from 1 to ${probe.pixels}.');
      return null;
    }
    return probe.copyWith(index: index - 1);
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
      change(probe.copyWith(rgb: result));
    }
  }

  @override
  Widget build(BuildContext context) {
    final enabled = authorized && !working && !refreshing;
    final colours = Theme.of(context).colorScheme;
    final map = widget.mapping?.call();
    final group = probe.first ~/ probe.groupSize + 1;
    final logical = probe.pixels == 480 && map?.valid == true
        ? map!.physical.indexOf(group) + 1
        : null;
    final first = probe.index ~/ probe.groupSize * probe.groupSize;
    return WorkspaceBody(
      controller: scroll,
      children: [
        const WorkspaceHeader(
          title: 'Pixel diagnostics',
          icon: PakarIcons.circuitry,
          subtitle: 'Physical wire order · all other pixels cleared',
        ),
        WorkspaceSection(
          title: probe.group ? '${probe.groupSize}-pixel probe' : 'Single-pixel probe',
          icon: PakarIcons.gridFour,
          children: [
            SegmentedButton<bool>(
              segments: [
                const ButtonSegment(value: false, label: Text('1 pixel')),
                ButtonSegment(value: true, label: Text('${probe.groupSize}-pixel group')),
              ],
              selected: {probe.group},
              onSelectionChanged: enabled
                  ? (choice) => change(
                      probe.copyWith(
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
              'Physical group $group of ${probe.pixels ~/ probe.groupSize}${logical != null ? ' · logical cell $logical' : ''}',
              style: TextStyle(color: colours.onSurfaceVariant),
            ),
            const SizedBox(height: 12),
            // Tap a sector to inspect any of the six physical pixels directly.
            Row(
              children: [
                for (var sector = 0; sector < probe.groupSize; sector++)
                  Expanded(
                    child: Tooltip(
                      message: 'Physical pixel ${first + sector + 1}',
                      child: InkWell(
                        key: Key('pixel-sector-$sector'),
                        borderRadius: BorderRadius.circular(8),
                        onTap: enabled
                            ? () => send(
                                probe.copyWith(
                                  index: first + sector,
                                  group: false,
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
                    decoration: InputDecoration(
                      labelText: 'Physical pixel · 1–${probe.pixels}',
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
                            probe.copyWith(
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
              label: Text('All off · clear ${probe.pixels} pixels'),
              onPressed: enabled ? () => send(probe, off: true) : null,
            ),
            const SizedBox(height: 12),
            // Fixed height: pending/confirmed/error never shifts the controls.
            SizedBox(
              height: 4,
              child: working || refreshing
                  ? const LinearProgressIndicator()
                  : const SizedBox(),
            ),
            const SizedBox(height: 10),
            if (!authorized)
              Text(
                blockedReason ?? 'Pixel testing is unavailable.',
                style: TextStyle(color: colours.error),
              ),
            TextButton.icon(
              key: const Key('pixel-refresh-state'),
              onPressed:
                  !working &&
                      !refreshing &&
                      widget.api.connectionWanted &&
                      widget.api.appForeground
                  ? refreshState
                  : null,
              icon: const Icon(Icons.refresh),
              label: Text(refreshing ? 'Refreshing state…' : 'Refresh state'),
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
