import 'dart:async';

import 'package:flutter/material.dart';

import 'api.dart';
import 'controller_dialog.dart';
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
    this.initialProbe,
    this.onPixelLongPress,
  });
  final PakarApi api;
  final DeviceStatus? Function() observed;
  final void Function(bool) busyChanged;
  final void Function(DeviceStatus)? onObserved;
  final PixelDiagnosticWorkspace? workspace;
  final CellMap? Function()? mapping;

  /// A deliberate navigation target, never permission to send on opening.
  final PixelProbe? initialProbe;

  /// Opens a peer-fenced action menu for one zero-based physical pixel. Merely
  /// opening the menu is not permission to send a probe or save a correction.
  final Future<void> Function(int physicalIndex)? onPixelLongPress;
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
    final requested = widget.initialProbe;
    if (requested != null &&
        requested.valid &&
        (initial == null ||
            requested.pixels == diagnosticPixelCount(initial) &&
                requested.groupSize == diagnosticGroupSize(initial) &&
                (initial.peer.has('pixel-group-layout')
                    ? requested.layout != null &&
                          initial.pixelGroups?.sameAs(requested.layout!) == true
                    : requested.layout == null))) {
      probe = requested.copyWith(enabled: true);
      position.text = '${probe.first + 1}';
    }
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
      final actual = await widget.api.settledStatus();
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
    final unavailable = next.unavailableReason(widget.api.freshStatus);
    if (unavailable != null) {
      setState(() => message = '$unavailable Nothing was sent.');
      return;
    }
    FocusScope.of(context).unfocus();
    setState(() {
      working = true;
      message = !next.enabled
          ? 'Restoring the board scene…'
          : off
          ? next.overlay
                ? 'Masking ${next.range} off…'
                : 'Clearing all ${next.pixels} pixels…'
          : 'Showing ${next.range}…';
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
          message = !next.enabled
              ? 'Board scene restored'
              : off && next.overlay
              ? 'Device confirmed ${next.range} masked off'
              : off
              ? 'Device confirmed all ${next.pixels} pixels off'
              : 'Device confirmed ${next.range} · ${next.effectLabel} · #${next.hex}';
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
    next = next.copyWith(enabled: true);
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
    if (index == null || index < 1 || index > probe.limit) {
      setState(
        () =>
            message = 'Enter a physical pixel number from 1 to ${probe.limit}.',
      );
      return null;
    }
    return probe.copyWith(index: index - 1, enabled: true);
  }

  Future<void> editTiming() async {
    // PCController effect_editor's numberDrafts/numberError pattern: typing
    // remains local, bounded whole numbers are validated before one commit,
    // and live status cannot overwrite the focused draft or reset a slider.
    final draftProbe = probe;
    final fields = [
      ('period_ms', 'Period · ms', draftProbe.periodMs, 100, 10000),
      ('duty_percent', 'Flash on · %', draftProbe.dutyPercent, 1, 99),
      ('min_brightness', 'Minimum · 0–255', draftProbe.minBrightness, 0, 255),
      ('max_brightness', 'Maximum · 0–255', draftProbe.maxBrightness, 0, 255),
    ];
    final drafts = {for (final field in fields) field.$1: '${field.$3}'};
    final form = GlobalKey<FormState>();
    final result = await showDialog<PixelProbe>(
      context: context,
      builder: (context) => ControllerDialog(
        title: 'Diagnostic timing',
        icon: PakarIcons.clock,
        maxWidth: 520,
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          FilledButton(
            key: const Key('diagnostic-timing-apply'),
            onPressed: () {
              if (form.currentState!.validate()) {
                Navigator.pop(
                  context,
                  draftProbe.copyWith(
                    periodMs: int.parse(drafts['period_ms']!.trim()),
                    dutyPercent: int.parse(drafts['duty_percent']!.trim()),
                    minBrightness: int.parse(drafts['min_brightness']!.trim()),
                    maxBrightness: int.parse(drafts['max_brightness']!.trim()),
                    enabled: true,
                  ),
                );
              }
            },
            child: const Text('Apply'),
          ),
        ],
        child: Form(
          key: form,
          child: LayoutBuilder(
            builder: (context, bounds) => Wrap(
              spacing: 12,
              runSpacing: 14,
              children: [
                for (final field in fields)
                  SizedBox(
                    width: bounds.maxWidth < 360
                        ? bounds.maxWidth
                        : (bounds.maxWidth - 12) / 2,
                    child: TextFormField(
                      key: Key('diagnostic-${field.$1}'),
                      initialValue: drafts[field.$1],
                      keyboardType: TextInputType.number,
                      onChanged: (value) => drafts[field.$1] = value,
                      decoration: InputDecoration(
                        labelText: field.$2,
                        helperText: '${field.$4}–${field.$5}',
                      ),
                      validator: (text) {
                        final value = int.tryParse(text?.trim() ?? '');
                        if (value == null ||
                            value < field.$4 ||
                            value > field.$5) {
                          return 'Enter ${field.$4}–${field.$5}';
                        }
                        if (field.$1 == 'max_brightness' &&
                            value <
                                (int.tryParse(drafts['min_brightness']!) ??
                                    0)) {
                          return 'Must be at least minimum';
                        }
                        return null;
                      },
                    ),
                  ),
              ],
            ),
          ),
        ),
      ),
    );
    if (result != null && mounted) change(result);
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
    final peer = (widget.api.freshStatus ?? widget.observed())?.peer;
    final effectCapable = peer?.has('pixel-diagnostic-effects') == true;
    final overlayCapable = peer?.has('pixel-diagnostic-overlay') == true;
    final correctionsCapable = peer?.has('pixel-corrections') == true;
    final map = widget.mapping?.call();
    final group = probe.physicalGroup;
    final logical = probe.pixels == 480 && map?.valid == true
        ? map!.physical.indexOf(group) + 1
        : null;
    final first = probe.groupFirst;
    return WorkspaceBody(
      controller: scroll,
      children: [
        WorkspaceHeader(
          title: 'Pixel diagnostics',
          icon: PakarIcons.circuitry,
          subtitle: probe.overlay
              ? 'Physical wire order · scene overlay'
              : 'Physical wire order · solo output',
        ),
        WorkspaceSection(
          title: probe.group
              ? '${probe.currentGroupSize}-pixel probe'
              : 'Single-pixel probe',
          icon: PakarIcons.gridFour,
          children: [
            SegmentedButton<bool>(
              segments: [
                const ButtonSegment(value: false, label: Text('1 pixel')),
                ButtonSegment(
                  value: true,
                  label: Text('${probe.currentGroupSize}-pixel group'),
                ),
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
              'Physical group $group of ${probe.groupCount}${logical != null ? ' · logical cell $logical' : ''}',
              style: TextStyle(color: colours.onSurfaceVariant),
            ),
            const SizedBox(height: 12),
            // Adapt the donor EffectEditor's available-width Wrap: each row
            // fills its actual card, not a fixed 44px strip on a wide tablet.
            // Larger physical groups wrap without shrinking touch targets;
            // the original 25px circles stay fixed while their slots expand.
            LayoutBuilder(
              key: const Key('pixel-sector-strip'),
              builder: (context, bounds) {
                const gap = 8.0;
                final scale = MediaQuery.textScalerOf(context).scale(14) / 14;
                final minimum = 44.0 * scale.clamp(1.0, 1.6);
                final maximumColumns =
                    ((bounds.maxWidth + gap) / (minimum + gap)).floor().clamp(
                      1,
                      12,
                    );
                final rows = (probe.currentGroupSize / maximumColumns).ceil();
                final columns = (probe.currentGroupSize / rows).ceil();
                final width = (bounds.maxWidth - gap * (columns - 1)) / columns;
                final color = Color(0xff000000 | probe.rgb);
                return Directionality(
                  textDirection: TextDirection.ltr,
                  child: Wrap(
                    spacing: gap,
                    runSpacing: gap,
                    children: [
                      for (
                        var sector = 0;
                        sector < probe.currentGroupSize;
                        sector++
                      )
                        SizedBox(
                          width: width,
                          child: Semantics(
                            button: true,
                            selected:
                                probe.group || first + sector == probe.index,
                            label: 'Physical pixel ${first + sector + 1}',
                            child: Tooltip(
                              message: 'Physical pixel ${first + sector + 1}',
                              child: Material(
                                color:
                                    !allOff &&
                                        (probe.group ||
                                            first + sector == probe.index)
                                    ? color.withValues(alpha: .10)
                                    : colours.surfaceContainerLow,
                                shape: RoundedRectangleBorder(
                                  borderRadius: BorderRadius.circular(12),
                                  side: BorderSide(
                                    color: colours.outlineVariant.withValues(
                                      alpha: .45,
                                    ),
                                  ),
                                ),
                                clipBehavior: Clip.antiAlias,
                                child: InkWell(
                                  key: Key('pixel-sector-$sector'),
                                  onTap: enabled
                                      ? () => send(
                                          probe.copyWith(
                                            index: first + sector,
                                            group: false,
                                            rgb: probe.rgb,
                                          ),
                                        )
                                      : null,
                                  onLongPress:
                                      enabled &&
                                          correctionsCapable &&
                                          widget.onPixelLongPress != null
                                      ? () => widget.onPixelLongPress!(
                                          first + sector,
                                        )
                                      : null,
                                  child: Padding(
                                    padding: const EdgeInsets.symmetric(
                                      horizontal: 6,
                                      vertical: 10,
                                    ),
                                    child: Column(
                                      mainAxisSize: MainAxisSize.min,
                                      children: [
                                        AnimatedContainer(
                                          key: Key(
                                            'pixel-sector-circle-$sector',
                                          ),
                                          duration: const Duration(
                                            milliseconds: 120,
                                          ),
                                          height: 25,
                                          width: 25,
                                          decoration: BoxDecoration(
                                            shape: BoxShape.circle,
                                            color:
                                                !allOff &&
                                                    (probe.group ||
                                                        first + sector ==
                                                            probe.index)
                                                ? color
                                                : colours.surfaceContainerHigh,
                                            border: Border.all(
                                              color: colours.outlineVariant,
                                            ),
                                          ),
                                        ),
                                        const SizedBox(height: 4),
                                        Text(
                                          '${first + sector + 1}',
                                          maxLines: 1,
                                          style: Theme.of(context)
                                              .textTheme
                                              .labelSmall
                                              ?.copyWith(
                                                fontFeatures: const [
                                                  FontFeature.tabularFigures(),
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
                    label: Text(
                      probe.group && probe.layout != null
                          ? 'Previous group'
                          : '−${probe.count}',
                    ),
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
                    label: Text(
                      probe.group && probe.layout != null
                          ? 'Next group'
                          : '+${probe.count}',
                    ),
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
                      labelText: 'Physical pixel · 1–${probe.limit}',
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
              label: Text(
                probe.overlay
                    ? 'Mask off · ${probe.range}'
                    : 'All off · clear ${probe.pixels} pixels',
              ),
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
          title: 'Probe effect',
          icon: Icons.auto_awesome_outlined,
          children: [
            LayoutBuilder(
              builder: (context, bounds) => Wrap(
                spacing: 12,
                runSpacing: 12,
                crossAxisAlignment: WrapCrossAlignment.center,
                children: [
                  SizedBox(
                    width: bounds.maxWidth < 460 ? bounds.maxWidth : 220,
                    child: DropdownButtonFormField<String>(
                      key: ValueKey('diagnostic-effect-${probe.effect}'),
                      initialValue: probe.effect,
                      isExpanded: true,
                      decoration: const InputDecoration(labelText: 'Effect'),
                      items: [
                        for (final effect in diagnosticEffects)
                          DropdownMenuItem(
                            value: effect,
                            enabled: effect == 'solid' || effectCapable,
                            child: Row(
                              children: [
                                Icon(switch (effect) {
                                  'blink' => Icons.flash_on_outlined,
                                  'breathe' => Icons.waves,
                                  'rainbow' => Icons.gradient,
                                  _ => Icons.circle_outlined,
                                }, size: 18),
                                const SizedBox(width: 10),
                                Text(PixelProbe(effect: effect).effectLabel),
                              ],
                            ),
                          ),
                      ],
                      onChanged: enabled
                          ? (value) {
                              if (value == null) return;
                              change(
                                probe.copyWith(
                                  effect: value,
                                  enabled: true,
                                  periodMs: effectCapable
                                      ? probe.periodMs
                                      : 1000,
                                  dutyPercent: effectCapable
                                      ? probe.dutyPercent
                                      : 50,
                                  minBrightness: effectCapable
                                      ? probe.minBrightness
                                      : 0,
                                  maxBrightness: effectCapable
                                      ? probe.maxBrightness
                                      : 255,
                                ),
                              );
                            }
                          : null,
                    ),
                  ),
                  SegmentedButton<bool>(
                    key: const Key('diagnostic-composite'),
                    segments: [
                      const ButtonSegment(
                        value: false,
                        icon: Icon(Icons.center_focus_strong_outlined),
                        label: Text('Solo'),
                      ),
                      ButtonSegment(
                        value: true,
                        enabled: overlayCapable,
                        icon: const Icon(Icons.layers_outlined),
                        label: const Text('Overlay'),
                      ),
                    ],
                    selected: {probe.overlay},
                    onSelectionChanged: enabled
                        ? (values) =>
                              change(probe.copyWith(overlay: values.single))
                        : null,
                  ),
                ],
              ),
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 10,
              runSpacing: 8,
              crossAxisAlignment: WrapCrossAlignment.center,
              children: [
                OutlinedButton.icon(
                  key: const Key('diagnostic-timing'),
                  onPressed: enabled && effectCapable ? editTiming : null,
                  icon: const Icon(Icons.tune),
                  label: const Text('Timing & brightness'),
                ),
                if (effectCapable)
                  Text(
                    '${probe.periodMs} ms · ${probe.dutyPercent}% on · ${probe.minBrightness}–${probe.maxBrightness}',
                    style: TextStyle(color: colours.onSurfaceVariant),
                  ),
                if (effectCapable)
                  TextButton.icon(
                    key: const Key('diagnostic-restore'),
                    onPressed: enabled
                        ? () => send(probe.copyWith(enabled: false))
                        : null,
                    icon: const Icon(Icons.undo),
                    label: const Text('Restore scene'),
                  ),
              ],
            ),
            if (!effectCapable || !overlayCapable)
              Padding(
                padding: const EdgeInsets.only(top: 8),
                child: Text(
                  !effectCapable && !overlayCapable
                      ? 'This firmware supports solid solo diagnostics. Update it for effects and overlay.'
                      : !effectCapable
                      ? 'Diagnostic effects are not available on this firmware.'
                      : 'Overlay is not available on this firmware.',
                  style: TextStyle(color: colours.onSurfaceVariant),
                ),
              ),
            if (probe.unavailableReason(widget.api.freshStatus)
                case final String problem)
              Padding(
                padding: const EdgeInsets.only(top: 8),
                child: Text(problem, style: TextStyle(color: colours.error)),
              ),
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
            Text(
              effectCapable
                  ? 'Solo clears surrounding output; Overlay keeps the scene running outside this span. Changes apply after Show. Restore scene ends the temporary test. Closing does not send a command.'
                  : 'Show clears every other pixel. After Show, colour and group changes apply immediately. All off keeps your cursor. Closing leaves the last frame on the board.',
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
