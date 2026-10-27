import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'icons.dart';

/// Host networking, not board configuration or board authentication.
class HostWifi {
  static const channel = MethodChannel('ir.digitalogic.pakar/host-wifi');
  static bool get supported =>
      !kIsWeb && defaultTargetPlatform == TargetPlatform.android;
  static List<String> knownNetworks(Map<String, dynamic>? status) => {
    for (final key in ['ap_ssid', 'station_ssid'])
      if (status?[key] is String &&
          validate(status![key] as String, '', true) == null)
        status[key] as String,
  }.toList();
  static String? validate(String ssid, String password, bool open) {
    if (ssid.isEmpty ||
        utf8.encode(ssid).length > 32 ||
        RegExp(r'[\x00-\x1f\x7f]').hasMatch(ssid)) {
      return 'Enter a Wi-Fi name of 1–32 bytes, without control characters.';
    }
    if (!open &&
        (password.length < 8 ||
            password.length > 63 ||
            password.codeUnits.any((c) => c < 32 || c > 126))) {
      return 'Enter the network’s 8–63 character WPA2 password.';
    }
    return null;
  }

  static Future<String> request(
    String method,
    String ssid,
    String password,
    bool open,
  ) async {
    final error = validate(ssid, password, open);
    if (error != null) throw FormatException(error);
    return await channel.invokeMethod<String>(method, {
          'ssid': ssid,
          'password': open ? '' : password,
          'open': open,
        }) ??
        'cancelled';
  }
}

Future<void> showHostWifi(
  BuildContext context, {
  String scope = 'manual',
  List<String> knownNetworks = const [],
}) => showDialog<void>(
  context: context,
  barrierDismissible: false,
  builder: (_) => _HostWifiDialog(scope: scope, knownNetworks: knownNetworks),
);

class _HostWifiDialog extends StatefulWidget {
  const _HostWifiDialog({required this.scope, required this.knownNetworks});
  final String scope;
  final List<String> knownNetworks;
  @override
  State<_HostWifiDialog> createState() => _HostWifiDialogState();
}

class _HostWifiDialogState extends State<_HostWifiDialog> {
  final ssid = TextEditingController(), password = TextEditingController();
  bool open = false, busy = false;
  String message = '';
  String get hintKey => 'pakar.host-wifi.${Uri.encodeComponent(widget.scope)}';
  @override
  void initState() {
    super.initState();
    restore();
  }

  Future<void> restore() async {
    final prefs = await SharedPreferences.getInstance();
    if (mounted && ssid.text.isEmpty) {
      ssid.text = prefs.getString(hintKey) ?? '';
    }
  }

  @override
  void dispose() {
    ssid.dispose();
    password.dispose();
    super.dispose();
  }

  Future<void> run(String action) async {
    setState(() {
      busy = true;
      message = '';
    });
    try {
      if (action == 'settings' || action == 'release') {
        await HostWifi.channel.invokeMethod<void>(action);
        if (mounted) {
          setState(
            () => message = action == 'release'
                ? 'App-only Wi-Fi released. Android manages the network again.'
                : 'Choose the board’s network in Android Wi-Fi settings, then return and Discover.',
          );
        }
      } else {
        final result = await HostWifi.request(
          action,
          ssid.text,
          password.text,
          open,
        );
        if (!mounted) return;
        // Remember only the explicitly entered name. Never store Wi-Fi passwords.
        if (result == 'available' || result == 'saved') {
          final prefs = await SharedPreferences.getInstance();
          await prefs.setString(hintKey, ssid.text);
        }
        if (!mounted) return;
        setState(
          () => message = switch (result) {
            'available' => 'Wi-Fi available to this app. Close, then Discover or connect to verify the board. Other app traffic also uses this local network until released.',
            'saved' => 'Network saved in Android. Use Wi-Fi settings to select it, then Discover.',
            _ => 'Wi-Fi request cancelled. No board commands were sent.',
          },
        );
      }
    } on Object catch (error) {
      if (mounted) {
        setState(
          () => message = error is PlatformException
              ? error.message ?? 'Wi-Fi operation unavailable'
              : error is FormatException
              ? error.message
              : 'Wi-Fi operation failed. Use system Wi-Fi settings.',
        );
      }
    } finally {
      if (mounted) setState(() => busy = false);
    }
  }

  @override
  Widget build(BuildContext context) => PopScope(
    canPop: !busy,
    child: AlertDialog(
      title: const Row(
        children: [
          Icon(PakarIcons.wifiHigh),
          SizedBox(width: 12),
          Expanded(child: Text('This device’s Wi-Fi')),
        ],
      ),
      content: SizedBox(
        width: 430,
        child: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text(
                'Join the board’s access point or the same router. Android asks for confirmation. App-only joins also route this app’s cloud traffic over that network; board settings are unchanged.',
              ),
              const SizedBox(height: 16),
              if (widget.knownNetworks.isNotEmpty)
                Wrap(
                  spacing: 8,
                  children: widget.knownNetworks
                      .toSet()
                      .map(
                        (name) => ActionChip(
                          avatar: const Icon(PakarIcons.wifiHigh, size: 16),
                          label: Text(name),
                          onPressed: busy
                              ? null
                              : () {
                                  ssid.text = name;
                                  password.clear();
                                },
                        ),
                      )
                      .toList(),
                ),
              TextField(
                controller: ssid,
                enabled: !busy,
                autocorrect: false,
                decoration: const InputDecoration(
                  labelText: 'Wi-Fi name (SSID)',
                  prefixIcon: Icon(PakarIcons.wifiHigh),
                ),
              ),
              const SizedBox(height: 12),
              if (!open)
                TextField(
                  controller: password,
                  enabled: !busy,
                  obscureText: true,
                  enableSuggestions: false,
                  autocorrect: false,
                  decoration: const InputDecoration(
                    labelText: 'Wi-Fi password',
                    prefixIcon: Icon(PakarIcons.lock),
                  ),
                ),
              CheckboxListTile(
                contentPadding: EdgeInsets.zero,
                title: const Text('Open network'),
                value: open,
                onChanged: busy ? null : (v) => setState(() => open = v!),
              ),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  FilledButton.icon(
                    onPressed: busy ? null : () => run('join'),
                    icon: const Icon(PakarIcons.link),
                    label: const Text('Join for this app'),
                  ),
                  OutlinedButton.icon(
                    onPressed: busy ? null : () => run('save'),
                    icon: const Icon(PakarIcons.floppyDisk),
                    label: const Text('Save in Android'),
                  ),
                  TextButton.icon(
                    onPressed: busy ? null : () => run('settings'),
                    icon: const Icon(PakarIcons.gear),
                    label: const Text('Wi-Fi settings'),
                  ),
                  TextButton(
                    onPressed: busy ? null : () => run('release'),
                    child: const Text('Release app-only Wi-Fi'),
                  ),
                ],
              ),
              if (busy)
                const Padding(
                  padding: EdgeInsets.only(top: 12),
                  child: LinearProgressIndicator(),
                ),
              if (message.isNotEmpty)
                Padding(
                  padding: const EdgeInsets.only(top: 12),
                  child: Semantics(liveRegion: true, child: Text(message)),
                ),
            ],
          ),
        ),
      ),
      actions: [
        if (busy)
          TextButton(
            onPressed: () async {
              try {
                await HostWifi.channel.invokeMethod<void>('cancel');
              } on PlatformException catch (error) {
                if (mounted) {
                  setState(
                    () => message = error.message ?? 'Cancellation failed',
                  );
                }
              }
            },
            child: const Text('Cancel request'),
          ),
        TextButton(
          onPressed: busy ? null : () => Navigator.pop(context),
          child: const Text('Close'),
        ),
      ],
    ),
  );
}
