import 'dart:async';

import 'package:flutter/material.dart';

import 'api.dart';
import 'board_sound.dart';
import 'workspace_widgets.dart';
import 'icons.dart';

class BoardSoundPreferences extends StatefulWidget {
  const BoardSoundPreferences({
    super.key,
    required this.api,
    this.active = true,
  });
  final PakarApi api;
  final bool active;
  @override
  State<BoardSoundPreferences> createState() => _BoardSoundPreferencesState();
}

class _BoardSoundPreferencesState extends State<BoardSoundPreferences> {
  BoardSound? observed;
  bool available = false, busy = false, programming = false, fresh = false;
  int? owner;
  String? message;
  Timer? refresh;
  @override
  void initState() {
    super.initState();
    unawaited(load());
    refresh = Timer.periodic(const Duration(seconds: 5), (_) {
      if (widget.active && message == null && !busy) unawaited(load());
    });
  }

  @override
  void didUpdateWidget(covariant BoardSoundPreferences oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.active && !oldWidget.active) unawaited(load());
  }

  @override
  void dispose() {
    refresh?.cancel();
    super.dispose();
  }

  Future<void> load({bool? silent}) async {
    if (!widget.active ||
        !widget.api.appForeground ||
        busy ||
        !widget.api.connectionWanted ||
        widget.api.connectionMethod != ConnectionMethod.network) {
      return;
    }
    setState(() {
      busy = true;
      fresh = false;
      message = null;
    });
    final revision = widget.api.connectionRevision;
    try {
      final status = widget.api.freshStatus ?? await widget.api.status();
      if (!mounted ||
          !widget.active ||
          !widget.api.appForeground ||
          revision != widget.api.connectionRevision) {
        return;
      }
      final supported =
          status.json['avr_online'] == true &&
          status.peer.has('settings') &&
          status.peer.has('buzzer');
      setState(() {
        available = supported;
        programming = status.json['programming_latch'] == true;
        owner = revision;
        if (!supported) observed = null;
      });
      if (!supported) return;
      final value = await widget.api.boardSound(silent: silent);
      if (!mounted ||
          !widget.active ||
          !widget.api.appForeground ||
          revision != widget.api.connectionRevision) {
        return;
      }
      setState(() {
        observed = value;
        fresh = true;
      });
    } on Exception catch (error) {
      if (mounted && revision == widget.api.connectionRevision) {
        setState(() {
          fresh = false;
          message = error.toString();
        });
      }
    } finally {
      if (mounted) setState(() => busy = false);
    }
  }

  Future<void> showDetails() => showDialog<void>(
    context: context,
    builder: (context) => AlertDialog(
      title: const Text('Board sound details'),
      content: SelectableText(message ?? ''),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Close'),
        ),
      ],
    ),
  );

  @override
  Widget build(BuildContext context) {
    if (!available ||
        !widget.api.connectionWanted ||
        widget.api.connectionMethod != ConnectionMethod.network) {
      return const SizedBox.shrink();
    }
    final value = owner == widget.api.connectionRevision ? observed : null;
    final verified = fresh && value != null && widget.api.freshStatus != null;
    return WorkspaceSection(
      title: 'Board sound',
      icon: PakarIcons.musicNote,
      children: [
        SizedBox(
          height: 80,
          child: SwitchListTile.adaptive(
            contentPadding: EdgeInsets.zero,
            title: const Text('Silent mode'),
            value: value?.silent ?? false,
            subtitle: Text(
              value == null
                  ? 'Unknown'
                  : '${verified ? '' : 'Last observed · '}${value.silent ? 'On' : 'Off'}',
            ),
            onChanged: busy || programming || !verified || !widget.active
                ? null
                : (value) => unawaited(load(silent: value)),
          ),
        ),
        const Text(
          'Mutes the physical board buzzer, including welcome and menu sounds. Saved on the board, not on this phone.',
        ),
        SizedBox(
          height: 20,
          child: Text(
            programming ? 'Programming safety keeps the board muted.' : '',
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
          ),
        ),
        SizedBox(
          height: 4,
          child: busy ? const LinearProgressIndicator() : null,
        ),
        SizedBox(
          height: 48,
          child: Row(
            children: [
              Expanded(
                child: Text(
                  message == null
                      ? (busy ? 'Reading board sound…' : '')
                      : 'Board sound could not be confirmed.',
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: TextStyle(
                    color: message == null
                        ? Theme.of(context).colorScheme.onSurfaceVariant
                        : Theme.of(context).colorScheme.error,
                  ),
                ),
              ),
              if (message != null)
                TextButton(
                  onPressed: showDetails,
                  child: const Text('Details'),
                ),
            ],
          ),
        ),
        SizedBox(
          height: 20,
          child: Text(
            value == null
                ? 'EEPROM state unknown'
                : value.persisted
                ? '${verified ? 'Verified' : 'Last verified'} · saved in EEPROM'
                : 'Not persisted',
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
          ),
        ),
        Align(
          alignment: Alignment.centerRight,
          child: TextButton.icon(
            onPressed: busy ? null : () => unawaited(load()),
            icon: const Icon(PakarIcons.arrowClockwise),
            label: const Text('Refresh'),
          ),
        ),
      ],
    );
  }
}
