import 'dart:async';

import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

enum SignalDisplay { icon, value, both }

/// Client-local display preference. Never changes the board or connection.
/// Retains the imported workspace chrome's bounded 48px InkWell target.
class SignalIndicator extends StatefulWidget {
  const SignalIndicator({super.key, required this.rssi, this.onConnection});
  final int rssi;
  final VoidCallback? onConnection;
  static const preferenceKey = 'status_signal_display';

  @override
  State<SignalIndicator> createState() => _SignalIndicatorState();
}

class _SignalIndicatorState extends State<SignalIndicator> {
  SignalDisplay display = SignalDisplay.icon;
  bool saving = false;

  @override
  void initState() {
    super.initState();
    unawaited(restore());
  }

  Future<void> restore() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final saved = prefs.getString(SignalIndicator.preferenceKey);
      if (!mounted) return;
      setState(
        () => display = SignalDisplay.values.firstWhere(
          (value) => value.name == saved,
          orElse: () => SignalDisplay.icon,
        ),
      );
    } on Object {
      // An unavailable local preference store must not affect live telemetry.
    }
  }

  Future<void> choose(BuildContext anchor) async {
    if (saving) return;
    final box = anchor.findRenderObject() as RenderBox?;
    final overlay = Overlay.of(anchor).context.findRenderObject() as RenderBox?;
    if (box == null || overlay == null) return;
    final rect = box.localToGlobal(Offset.zero, ancestor: overlay) & box.size;
    final chosen = await showMenu<SignalDisplay>(
      context: context,
      position: RelativeRect.fromRect(rect, Offset.zero & overlay.size),
      semanticLabel: 'Signal display',
      items: [
        for (final mode in SignalDisplay.values)
          CheckedPopupMenuItem(
            value: mode,
            checked: display == mode,
            child: Row(
              children: [
                Icon(switch (mode) {
                  SignalDisplay.icon => Icons.wifi,
                  SignalDisplay.value => Icons.numbers,
                  SignalDisplay.both => Icons.view_week_outlined,
                }, size: 20),
                const SizedBox(width: 12),
                Flexible(
                  child: Text(
                    switch (mode) {
                      SignalDisplay.icon => 'Icon only',
                      SignalDisplay.value => 'Value only',
                      SignalDisplay.both => 'Icon and value',
                    },
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                  ),
                ),
              ],
            ),
          ),
      ],
    );
    if (!mounted || chosen == null || chosen == display) return;
    setState(() => saving = true);
    try {
      final prefs = await SharedPreferences.getInstance();
      if (!await prefs.setString(SignalIndicator.preferenceKey, chosen.name)) {
        throw StateError('Preference write failed');
      }
      if (mounted) setState(() => display = chosen);
    } on Object {
      if (mounted) {
        ScaffoldMessenger.maybeOf(context)?.showSnackBar(
          const SnackBar(
            content: Text('Could not save the signal display preference.'),
          ),
        );
      }
    } finally {
      if (mounted) setState(() => saving = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final rssi = widget.rssi;
    final valid = rssi < 0 && rssi >= -127;
    final level = !valid
        ? 0
        : rssi >= -55
        ? 4
        : rssi >= -67
        ? 3
        : rssi >= -75
        ? 2
        : rssi >= -85
        ? 1
        : 0;
    final quality = !valid
        ? 'Unavailable'
        : ['Very weak', 'Weak', 'Fair', 'Good', 'Excellent'][level];
    final label = 'Wi-Fi signal: $quality${valid ? ' ($rssi dBm)' : ''}';
    final icon = [
      Icons.signal_wifi_0_bar,
      Icons.network_wifi_1_bar,
      Icons.network_wifi_2_bar,
      Icons.network_wifi_3_bar,
      Icons.signal_wifi_4_bar,
    ][level];
    final colors = Theme.of(context).colorScheme;
    final color = !valid
        ? colors.onSurfaceVariant
        : level <= 1
        ? colors.tertiary
        : colors.secondary;
    return Builder(
      builder: (anchor) => Tooltip(
        message: '$label. Long-press to change display.',
        triggerMode: TooltipTriggerMode.manual,
        child: InkWell(
          key: const Key('status-signal'),
          borderRadius: BorderRadius.circular(8),
          onTap: widget.onConnection,
          onLongPress: saving ? null : () => unawaited(choose(anchor)),
          child: Semantics(
            label: label,
            excludeSemantics: true,
            child: SizedBox(
              height: 48,
              width: switch (display) {
                SignalDisplay.icon => 48,
                SignalDisplay.value => 88,
                SignalDisplay.both => 116,
              },
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: 6),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    if (display != SignalDisplay.value)
                      Icon(icon, size: 20, color: color),
                    if (display == SignalDisplay.both) const SizedBox(width: 6),
                    if (display != SignalDisplay.icon)
                      Flexible(
                        child: Text(
                          valid ? '$rssi dBm' : '—',
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: Theme.of(context).textTheme.bodySmall
                              ?.copyWith(color: color),
                        ),
                      ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
