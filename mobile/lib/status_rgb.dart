import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';

import 'api.dart';
import 'models.dart';

/// Actual Pealayer status_led_display_color: mirror emitted bytes directly,
/// never synthesize a second breathing engine or apply brightness twice.
Color? statusRgbColor(DeviceStatus? status, {required bool fresh}) {
  final value = status?.json['status_rgb'];
  if (!fresh ||
      status?.peer.has('status-rgb') != true ||
      value is! Map ||
      value['available'] != true ||
      value['have_output'] != true) {
    return null;
  }
  final rgb = value['rgb'];
  if (rgb is! List ||
      rgb.length != 3 ||
      rgb.any((v) => v is! int || v < 0 || v > 255)) {
    return null;
  }
  return Color.fromARGB(255, rgb[0] as int, rgb[1] as int, rgb[2] as int);
}

Map<String, Object> statusRgbConfiguration(Map<String, dynamic> value) {
  final enabled = value['enabled'],
      pin = value['pin'],
      confirmed = value['pin_confirmed'],
      brightness = value['brightness'];
  if (enabled is! bool ||
      confirmed is! bool ||
      pin is! int ||
      brightness is! int ||
      brightness < 0 ||
      brightness > 255 ||
      !(pin == -1 && !enabled && !confirmed ||
          const [38, 48].contains(pin) && (!enabled || confirmed))) {
    throw const FormatException(
      'Read complete RGB settings; explicitly confirm GPIO38 or GPIO48 before enabling',
    );
  }
  return {
    'enabled': enabled,
    'pin': pin,
    'pin_confirmed': confirmed,
    'brightness': brightness,
  };
}

class StatusRgbPreview extends StatelessWidget {
  const StatusRgbPreview({
    super.key,
    required this.status,
    required this.fresh,
    this.api,
  });
  final DeviceStatus? status;
  final bool fresh;
  final PakarApi? api;
  @override
  Widget build(BuildContext context) {
    if (api?.isBluetooth == true) {
      return StatusRgbPreview(
        status: api!.latestStatus ?? status,
        fresh: fresh && api!.liveFresh,
      );
    } // BLE keeps its actual 2s status sample.
    if (api != null) {
      return StreamBuilder<DeviceStatus>(
        stream: api!.rgbUpdates,
        initialData: api!.latestStatus ?? status,
        builder: (context, frame) {
          final current = frame.data;
          final same =
              current?.json['board_id'] == status?.json['board_id'] &&
              current?.json['boot_id'] == status?.json['boot_id'];
          return StatusRgbPreview(
            status: same ? current : status,
            fresh: fresh && api!.liveFresh,
          );
        },
      );
    }
    final color = statusRgbColor(status, fresh: fresh);
    if (color == null) return const SizedBox.shrink();
    final value = status!.json['status_rgb'] as Map;
    final state = value['state'] is String
        ? value['state'] as String
        : 'Unknown';
    return Semantics(
      label: 'Physical status RGB · $state',
      child: Tooltip(
        message:
            'Emitted RGB ${(value['rgb'] as List).join(', ')} · brightness ${value['brightness'] ?? 'Unknown'} · revision ${value['revision'] ?? 'Unknown'}',
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(
              width: 12,
              height: 12,
              decoration: BoxDecoration(color: color, shape: BoxShape.circle),
            ),
            const SizedBox(width: 8),
            Text('RGB · $state', style: Theme.of(context).textTheme.bodySmall),
          ],
        ),
      ),
    );
  }
}

/// Reuses the existing PakarApi selected-route transport/fences. No TCP fallback
/// is added for Bluetooth or USB, and uncertain writes are never retried.
class StatusRgbSettings extends StatefulWidget {
  const StatusRgbSettings({
    super.key,
    required this.api,
    required this.status,
    required this.authorized,
    required this.lockChanged,
    this.blocked = false,
  });
  final PakarApi api;
  final DeviceStatus status;
  final bool Function(String) authorized;
  final ValueChanged<bool> lockChanged;
  final bool blocked;
  @override
  State<StatusRgbSettings> createState() => _StatusRgbSettingsState();
}

class _StatusRgbSettingsState extends State<StatusRgbSettings> {
  Map<String, Object>? saved, draft;
  bool pending = false, conflict = false;
  String message = 'RGB settings: Unknown';
  int epoch = 0, owner = -1;
  bool get dirty => saved != null && !mapEquals(saved, draft);
  bool get available =>
      widget.authorized('status-rgb') &&
      widget.api.supportsRoute('GET', '/api/status-rgb') &&
      widget.api.supportsRoute('POST', '/api/status-rgb');
  void update() {
    widget.lockChanged(pending || dirty);
    setState(() {});
  }

  @override
  void didUpdateWidget(covariant StatusRgbSettings old) {
    super.didUpdateWidget(old);
    if (owner >= 0 && owner != widget.api.connectionRevision) {
      ++epoch;
      saved = draft = null;
      pending = conflict = false;
      owner = -1;
      message = 'Board changed; read its RGB settings again';
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) widget.lockChanged(false);
      });
    }
  }

  Future<void> operation({bool save = false}) async {
    if (!available || pending || widget.blocked) return;
    final revision = widget.api.connectionRevision, captured = ++epoch;
    final board = widget.status.json['board_id'],
        boot = widget.status.json['boot_id'];
    bool current() =>
        mounted &&
        available &&
        widget.api.appForeground &&
        revision == widget.api.connectionRevision &&
        captured == epoch &&
        widget.api.latestStatus?.json['board_id'] == board &&
        widget.api.latestStatus?.json['boot_id'] == boot;
    void check() {
      if (!current()) throw const StaleConnection();
    }

    Map<String, Object>? desired;
    try {
      if (board is! String || boot is! String) throw const StaleConnection();
      if (save) {
        desired = statusRgbConfiguration(Map<String, dynamic>.from(draft!));
      }
    } catch (error) {
      setState(() => message = error.toString());
      return;
    }
    pending = true;
    update();
    try {
      await widget.api.status();
      check();
      final before = statusRgbConfiguration(
        await widget.api.getJson('/api/status-rgb'),
      );
      check();
      if (save) {
        if (!mapEquals(saved, before)) {
          conflict = true;
          throw const FormatException(
            'RGB settings changed on another client; discard and read before saving',
          );
        }
        final ack = await widget.api.postJson('/api/status-rgb', {
          for (final entry in desired!.entries)
            entry.key: entry.value.toString(),
        }, authorize: current);
        check();
        if (ack['saved'] != true) {
          throw const FormatException('RGB save was not acknowledged');
        }
        final read = await widget.api.getJson('/api/status-rgb');
        check();
        final actual = statusRgbConfiguration(read);
        await widget.api.status();
        check();
        if (read['persisted'] != true || !mapEquals(desired, actual)) {
          throw const FormatException(
            'RGB save accepted but persisted readback differs; not replayed',
          );
        }
        saved = Map.of(actual);
        draft = Map.of(actual);
        message =
            'Status LED settings saved on device';
      } else {
        await widget.api.status();
        check();
        saved = Map.of(before);
        draft = Map.of(before);
        conflict = false;
        message = 'Read from this board; no defaults invented';
      }
      owner = revision;
    } catch (error) {
      if (mounted && captured == epoch) {
        message = '$error Uncertain writes are not retried.';
      }
    } finally {
      if (mounted && captured == epoch) {
        pending = false;
        update();
      }
    }
  }

  @override
  void dispose() {
    ++epoch;
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (widget.status.peer.has('status-rgb') != true) {
      return const SizedBox.shrink();
    }
    final editable =
        available && !pending && !widget.blocked && !conflict && draft != null;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Onboard semantic status RGB',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 8),
            StatusRgbPreview(
              api: widget.api,
              status: widget.status,
              fresh: widget.authorized('status-rgb'),
            ),
            const Text(
              'Separate from GPIO4 matrix. The board owns semantic colors and animation; this preview mirrors emitted RGB.',
            ),
            if (!available)
              const Text(
                'This selected transport does not advertise RGB configuration. No network fallback is used.',
              ),
            if (draft != null) ...[
              DropdownButtonFormField<int>(
                initialValue: draft!['pin'] as int,
                decoration: const InputDecoration(
                  labelText: 'Onboard pixel GPIO',
                ),
                items: const [
                  DropdownMenuItem(value: -1, child: Text('Unassigned')),
                  DropdownMenuItem(
                    value: 48,
                    child: Text('GPIO48 · common DevKit'),
                  ),
                  DropdownMenuItem(
                    value: 38,
                    child: Text('GPIO38 · alternate DevKit'),
                  ),
                ],
                onChanged: editable
                    ? (value) {
                        draft!['pin'] = value!;
                        draft!['pin_confirmed'] = false;
                        if (value == -1) draft!['enabled'] = false;
                        update();
                      }
                    : null,
              ),
              CheckboxListTile(
                contentPadding: EdgeInsets.zero,
                title: const Text('Confirm the onboard addressable RGB pin'),
                value: draft!['pin_confirmed'] as bool,
                onChanged: editable
                    ? (value) {
                        draft!['pin_confirmed'] = value!;
                        update();
                      }
                    : null,
              ),
              SwitchListTile.adaptive(
                contentPadding: EdgeInsets.zero,
                title: const Text('Enable semantic status output'),
                value: draft!['enabled'] as bool,
                onChanged: editable
                    ? (value) {
                        draft!['enabled'] = value;
                        update();
                      }
                    : null,
              ),
              Text('Brightness ${draft!['brightness']} /255'),
              Slider(
                value: (draft!['brightness'] as int).toDouble(),
                min: 0,
                max: 255,
                divisions: 255,
                onChanged: editable
                    ? (value) {
                        draft!['brightness'] = value.round();
                        update();
                      }
                    : null,
              ),
            ],
            if (pending) const LinearProgressIndicator(),
            Text(message),
            Wrap(
              spacing: 8,
              children: [
                if (dirty)
                  OutlinedButton(
                    onPressed: !pending
                        ? () {
                            draft = Map.of(saved!);
                            conflict = false;
                            update();
                          }
                        : null,
                    child: const Text('Discard RGB draft'),
                  ),
                OutlinedButton(
                  onPressed: available && !pending && !widget.blocked && !dirty
                      ? () => operation()
                      : null,
                  child: const Text('Read RGB settings'),
                ),
                FilledButton(
                  onPressed: editable && dirty
                      ? () => operation(save: true)
                      : null,
                  child: const Text('Save onboard RGB'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
