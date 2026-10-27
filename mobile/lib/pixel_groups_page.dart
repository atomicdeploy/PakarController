import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';

import 'api.dart';
import 'controller_dialog.dart';
import 'models.dart';
import 'pixel_corrections_editor.dart';
import 'pixel_diagnostic.dart';
import 'pixel_groups.dart';
import 'workspace_widgets.dart';

/// PCController pwm_control's owned draft/explicit commit/readback discipline;
/// Pealayer NumericValueControl's bounded steppers, shared with corrections.
class PixelGroupsPage extends StatefulWidget {
  const PixelGroupsPage({
    super.key,
    required this.api,
    required this.busyChanged,
    this.workspace,
    this.mapping,
  });
  final PakarApi api;
  final ValueChanged<bool> busyChanged;
  final PixelDiagnosticWorkspace? workspace;
  final CellMap? Function()? mapping;
  @override
  State<PixelGroupsPage> createState() => _PixelGroupsPageState();
}

class _PixelGroupsPageState extends State<PixelGroupsPage>
    with AutomaticKeepAliveClientMixin {
  late final owner = widget.api.connectionRevision;
  late final board = widget.api.freshStatus?.json['board_id'];
  late final boot = widget.api.freshStatus?.json['boot_id'];
  PixelGroupLayout? saved;
  Map<int, int> overrides = {};
  bool busy = false, dirty = false, stale = false;
  String? error, message;
  StreamSubscription<DeviceStatus>? updates;
  bool get current {
    final status = widget.api.freshStatus;
    return owner == widget.api.connectionRevision &&
        board != null &&
        boot != null &&
        status?.json['board_id'] == board &&
        status?.json['boot_id'] == boot &&
        status?.peer.has('pixel-group-layout') == true &&
        widget.api.connectionWanted &&
        widget.api.appForeground;
  }

  PixelGroupLayout? get draft {
    try {
      return saved?.draft(overrides);
    } on FormatException {
      return null;
    }
  }

  @override
  bool get wantKeepAlive => true;
  @override
  void initState() {
    super.initState();
    updates = widget.api.statusUpdates.listen((status) {
      if (!mounted || busy) return;
      if (saved == null) {
        setState(() {});
        return;
      }
      final observed = status.pixelGroups;
      if (!current ||
          status.json['board_id'] != board ||
          status.json['boot_id'] != boot ||
          observed == null ||
          observed.revision > saved!.revision ||
          observed.revision == saved!.revision && !observed.sameAs(saved!)) {
        setState(() => stale = true);
      }
    });
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) load();
    });
  }

  @override
  void dispose() {
    updates?.cancel();
    super.dispose();
  }

  Future<void> load({bool save = false}) async {
    if (busy || !current || save && (stale || !dirty || draft == null)) return;
    final desired = draft;
    setState(() {
      busy = true;
      error = null;
      message = null;
    });
    widget.busyChanged(true);
    try {
      if (save) {
        await widget.api.postJson('/api/pixels/groups', {
          'configuration': jsonEncode(desired!.configuration()),
        }, authorize: () => current);
        if (!current) throw const StaleConnection();
      }
      final json = await widget.api.getJson('/api/pixels/groups');
      if (!mounted || !current) throw const StaleConnection();
      if (json['partial'] == true) {
        throw const FormatException(
          'Editable group configuration is incomplete',
        );
      }
      final actual = PixelGroupLayout.fromJson(
        json,
        keys: widget.api.freshStatus!.peer.role == 'first-wins',
      );
      if (save &&
          (actual.revision < saved!.revision ||
              jsonEncode(actual.counts) != jsonEncode(desired!.counts) ||
              jsonEncode(actual.starts) != jsonEncode(desired.starts))) {
        throw const FormatException(
          'Group configuration readback differs. No write was replayed.',
        );
      }
      setState(() {
        saved = actual;
        overrides = Map.of(actual.overrides);
        dirty = false;
        stale = false;
        message = save ? 'Saved and verified on this board' : null;
      });
      // A read is safe; refresh the observed layout only after the committed
      // configuration has been verified. Never mutate local status optimistically.
      if (save) await widget.api.settledStatus();
    } catch (failure) {
      if (mounted) setState(() => error = '$failure');
    } finally {
      if (mounted) setState(() => busy = false);
      widget.busyChanged(false);
    }
  }

  String name(int group) {
    if (saved?.groupCount == 18) {
      return 'Box ${(group + 1) ~/ 2} · ${group.isOdd ? 'Button' : 'Indicator'}';
    }
    final map = widget.mapping?.call();
    final logical = map?.valid == true ? map!.physical.indexOf(group) + 1 : 0;
    return logical > 0
        ? 'Cell $logical · physical group $group'
        : 'Physical group $group';
  }

  Future<void> edit(int group) async {
    if (!current || busy || stale || saved == null) return;
    var value = overrides[group] ?? 0;
    var valid = true;
    final result = await showDialog<int>(
      context: context,
      builder: (context) => StatefulBuilder(
        builder: (context, update) => ControllerDialog(
          title: name(group),
          icon: Icons.format_list_numbered,
          maxWidth: 460,
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: valid ? () => Navigator.pop(context, value) : null,
              child: const Text('Use in draft'),
            ),
          ],
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              PixelCorrectionNumberControl(
                label: 'Pixels · 0 inherits ${saved!.defaultCount}',
                value: value,
                minimum: 0,
                maximum: 255,
                enabled: true,
                onChanged: (n) => update(() => value = n),
                onValidityChanged: (v) => update(() => valid = v),
              ),
              const SizedBox(height: 12),
              Text(
                'Default ${saved!.defaultCount} · changes shift following physical groups.',
              ),
            ],
          ),
        ),
      ),
    );
    if (result == null || !mounted || !current || stale) return;
    setState(() {
      if (result == 0) {
        overrides.remove(group);
      } else {
        overrides[group] = result;
      }
      dirty = true;
      message = null;
    });
  }

  @override
  Widget build(BuildContext context) {
    super.build(context);
    final colors = Theme.of(context).colorScheme;
    final preview = draft;
    final selected = saved?.groupAt(widget.workspace?.probe.index ?? 0);
    return WorkspaceBody(
      children: [
        WorkspaceHeader(
          title: 'Pixel groups',
          icon: Icons.format_list_numbered,
          subtitle: 'Physical group lengths · saved on this board',
        ),
        WorkspaceSection(
          title: 'Wire allocation',
          icon: Icons.linear_scale,
          children: [
            Wrap(
              spacing: 8,
              runSpacing: 8,
              crossAxisAlignment: WrapCrossAlignment.center,
              children: [
                if (saved != null)
                  Text(
                    '${preview?.usedPixels ?? 'Over capacity'} / ${saved!.capacity} pixels',
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                if (saved != null)
                  Chip(label: Text('Default ${saved!.defaultCount}')),
                if (dirty) const Chip(label: Text('Unsaved draft')),
              ],
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                FilledButton.icon(
                  key: const Key('pixel-groups-save'),
                  onPressed:
                      current && !busy && dirty && !stale && preview != null
                      ? () => load(save: true)
                      : null,
                  icon: const Icon(Icons.save_outlined),
                  label: const Text('Save to board'),
                ),
                OutlinedButton.icon(
                  onPressed: current && !busy ? () => load() : null,
                  icon: const Icon(Icons.refresh),
                  label: Text(dirty ? 'Discard & reload' : 'Reload'),
                ),
                if (selected != null)
                  TextButton.icon(
                    onPressed: current && !busy && !stale
                        ? () => edit(selected)
                        : null,
                    icon: const Icon(Icons.my_location),
                    label: Text('Diagnostic group $selected'),
                  ),
              ],
            ),
            if (busy) const LinearProgressIndicator(),
            if (error != null || stale || !current || dirty && preview == null)
              Padding(
                padding: const EdgeInsets.only(top: 12),
                child: Text(
                  error ??
                      (!current
                          ? 'Connection changed or unavailable. Reopen this editor.'
                          : stale
                          ? 'Layout changed on the board. Reload before saving.'
                          : 'The draft exceeds the wire capacity.'),
                  style: TextStyle(color: colors.error),
                ),
              ),
            if (message != null)
              Padding(
                padding: const EdgeInsets.only(top: 8),
                child: Text(message!),
              ),
          ],
        ),
        if (saved != null)
          WorkspaceSection(
            title: 'Groups',
            icon: Icons.view_module_outlined,
            children: [
              for (var group = 1; group <= saved!.groupCount; group++)
                ListTile(
                  key: ValueKey('pixel-group-$group'),
                  dense: true,
                  leading: CircleAvatar(radius: 17, child: Text('$group')),
                  title: Text(name(group)),
                  subtitle: Text(
                    preview == null
                        ? 'Draft allocation exceeds capacity'
                        : 'Pixels ${preview.start(group) + 1}–${preview.start(group) + preview.count(group)}',
                  ),
                  trailing: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Text(
                        '${overrides[group] ?? saved!.defaultCount}',
                        style: Theme.of(context).textTheme.titleMedium,
                      ),
                      const SizedBox(width: 8),
                      Icon(
                        overrides.containsKey(group)
                            ? Icons.edit_outlined
                            : Icons.chevron_right,
                      ),
                    ],
                  ),
                  onTap: current && !busy && !stale ? () => edit(group) : null,
                ),
            ],
          ),
      ],
    );
  }
}
