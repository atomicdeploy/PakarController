import 'dart:async';

import 'package:flutter/material.dart';

import 'api.dart';
import 'workspace_widgets.dart';

class NetworkToolsPage extends StatefulWidget {
  const NetworkToolsPage({super.key, required this.api});
  final PakarApi api;
  @override
  State<NetworkToolsPage> createState() => _NetworkToolsPageState();
}

class _NetworkToolsPageState extends State<NetworkToolsPage> {
  final ntp = TextEditingController(),
      date = TextEditingController(),
      ip = TextEditingController();
  final interval = TextEditingController(),
      target = TextEditingController(text: 'pool.ntp.org');
  late final owner = widget.api.connectionRevision;
  Map<String, dynamic>? snapshot;
  Map<String, dynamic> baseline = {};
  bool enabled = true,
      dhcp = true,
      dateAuto = false,
      ipAuto = false,
      busy = false,
      dirty = false;
  String? error;
  StreamSubscription<int>? changes;
  StreamSubscription<void>? connection;
  StreamSubscription<Map<String, dynamic>>? observations;
  Timer? tick;
  final observationAge = Stopwatch();
  int readGeneration = 0;
  bool get editable => current && !busy && snapshot != null;
  bool get current =>
      mounted &&
      widget.api.connectionRevision == owner &&
      widget.api.connectionWanted &&
      widget.api.appForeground;

  @override
  void initState() {
    super.initState();
    connection = widget.api.connectionUpdates.listen((_) {
      if (mounted) setState(() {});
    });
    changes = widget.api.configurationUpdates.listen((_) {
      if (current && !busy) unawaited(load());
    });
    observations = widget.api.networkObservations.listen((_) {
      if (current && !busy) unawaited(load());
    });
    unawaited(load());
    tick = Timer.periodic(const Duration(seconds: 1), (_) {
      if (current && snapshot != null) setState(() {});
    });
  }

  void accept(Map<String, dynamic> value, {bool replaceDraft = false}) {
    if (!current) return;
    final settings = value['settings'];
    if (settings is! Map ||
        value['clock'] is! Map ||
        !const [
          'ntp_enabled',
          'ntp_dhcp',
          'http_date_automatic',
          'public_ip_automatic',
        ].every((key) => settings[key] is bool) ||
        !const [
          'ntp_server',
          'http_date_url',
          'public_ip_url',
        ].every((key) => settings[key] is String) ||
        settings['refresh_seconds'] is! int) {
      throw const FormatException('Incomplete network tools snapshot');
    }
    setState(() {
      snapshot = value;
      observationAge
        ..reset()
        ..start();
      if (!dirty || replaceDraft) {
        baseline = Map<String, dynamic>.from(settings);
        enabled = settings['ntp_enabled'] == true;
        dhcp = settings['ntp_dhcp'] == true;
        dateAuto = settings['http_date_automatic'] == true;
        ipAuto = settings['public_ip_automatic'] == true;
        ntp.text = '${settings['ntp_server']}';
        date.text = '${settings['http_date_url']}';
        ip.text = '${settings['public_ip_url']}';
        interval.text = '${settings['refresh_seconds']}';
        dirty = false;
      }
      error = null;
    });
  }

  Future<void> load() async {
    final generation = ++readGeneration;
    try {
      final value = await widget.api.getJson('/api/network/tools');
      if (generation == readGeneration) accept(value);
    } on Object catch (e) {
      if (current && generation == readGeneration) setState(() => error = '$e');
    }
  }

  Future<void> run(Future<void> Function() action) async {
    if (!editable) return;
    readGeneration++;
    setState(() {
      busy = true;
      error = null;
    });
    try {
      await action();
    } on Object catch (e) {
      if (current) setState(() => error = '$e');
    } finally {
      if (mounted) setState(() => busy = false);
    }
  }

  Future<void> save() => run(() async {
    final seconds = int.tryParse(interval.text);
    if (seconds == null || seconds < 60 || seconds > 86400) {
      throw const FormatException('Refresh interval must be 60–86400 seconds');
    }
    final requested = <String, dynamic>{
      'ntp_enabled': enabled,
      'ntp_dhcp': dhcp,
      'ntp_server': ntp.text.trim(),
      'http_date_url': date.text.trim(),
      'http_date_automatic': dateAuto,
      'public_ip_url': ip.text.trim(),
      'public_ip_automatic': ipAuto,
      'refresh_seconds': seconds,
    };
    final patch = <String, dynamic>{
      for (final entry in requested.entries)
        if (baseline[entry.key] != entry.value) entry.key: entry.value,
    };
    if (patch.isEmpty) {
      accept(snapshot!, replaceDraft: true);
      return;
    }
    accept(
      await widget.api.networkToolsWrite('/api/network/tools', patch),
      replaceDraft: true,
    );
  });

  Future<void> probe(String kind) => run(() async {
    accept(
      await widget.api.networkToolsWrite('/api/network/probe', {
        'kind': kind,
        if (kind == 'dns' || kind == 'ping') 'target': target.text.trim(),
      }),
    );
    final revision = (snapshot?['job'] as Map?)?['revision'];
    // Events finish the job on WebSocket. Bounded read-only polling supports
    // USB and an HTTP-only connection; it never resends the probe command.
    for (var n = 0; n < 24 && current; n++) {
      if ((snapshot?['job'] as Map?)?['busy'] != true) return;
      await Future<void>.delayed(const Duration(milliseconds: 500));
      if (!current) return;
      accept(await widget.api.getJson('/api/network/tools'));
      if ((snapshot?['job'] as Map?)?['revision'] != revision) {
        throw const FormatException(
          'Another diagnostic replaced this result; no probe was replayed',
        );
      }
    }
  });

  @override
  void dispose() {
    tick?.cancel();
    unawaited(changes?.cancel());
    unawaited(connection?.cancel());
    unawaited(observations?.cancel());
    for (final c in [ntp, date, ip, interval, target]) {
      c.dispose();
    }
    super.dispose();
  }

  Widget field(
    TextEditingController controller,
    String label, {
    bool number = false,
  }) => Padding(
    padding: const EdgeInsets.only(bottom: 12),
    child: TextField(
      controller: controller,
      enabled: editable,
      keyboardType: number ? TextInputType.number : TextInputType.url,
      decoration: InputDecoration(labelText: label),
      onChanged: (_) => setState(() => dirty = true),
    ),
  );

  @override
  Widget build(BuildContext context) {
    final clock = snapshot?['clock'] as Map?,
        observed = snapshot?['observed'] as Map?;
    final job = snapshot?['job'] as Map?;
    final epoch = clock?['utc_epoch'];
    final utc = epoch is int && clock?['valid'] == true
        ? DateTime.fromMillisecondsSinceEpoch(
            epoch * 1000 + observationAge.elapsedMilliseconds,
            isUtc: true,
          ).toIso8601String()
        : 'Not synchronized';
    return PopScope(
      canPop: !dirty && !busy,
      onPopInvokedWithResult: (didPop, result) async {
        if (didPop || busy) return;
        final choice = await showDialog<String>(
          context: context,
          builder: (context) => AlertDialog(
            title: const Text('Unsaved network settings'),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(context, 'cancel'),
                child: const Text('Cancel'),
              ),
              TextButton(
                onPressed: () => Navigator.pop(context, 'discard'),
                child: const Text('Discard'),
              ),
              FilledButton(
                onPressed: () => Navigator.pop(context, 'save'),
                child: const Text('Save'),
              ),
            ],
          ),
        );
        if (choice == 'save') await save();
        if (mounted && (choice == 'discard' || choice == 'save' && !dirty)) {
          setState(() => dirty = false);
          WidgetsBinding.instance.addPostFrameCallback((_) {
            if (mounted) Navigator.pop(context);
          });
        }
      },
      child: Column(
        children: [
          Padding(
            padding: const EdgeInsets.all(12),
            child: Row(
              children: [
                Expanded(
                  child: Text(
                    busy
                        ? 'Working…'
                        : dirty
                        ? 'Unsaved changes'
                        : 'Board network tools',
                  ),
                ),
                IconButton(
                  tooltip: 'Refresh observed values',
                  onPressed: busy ? null : load,
                  icon: const Icon(Icons.refresh),
                ),
                FilledButton.icon(
                  onPressed: dirty && editable ? save : null,
                  icon: const Icon(Icons.save_outlined),
                  label: const Text('Save'),
                ),
              ],
            ),
          ),
          if (error != null)
            Padding(
              padding: const EdgeInsets.all(12),
              child: Text(
                error!,
                style: TextStyle(color: Theme.of(context).colorScheme.error),
              ),
            ),
          Expanded(
            child: ListView(
              padding: const EdgeInsets.all(16),
              children: [
                WorkspaceSection(
                  title: 'Board time',
                  icon: Icons.schedule,
                  children: [
                    SelectableText(utc),
                    Text(
                      'Source: ${clock?['source'] ?? 'Unknown'} · read ${observationAge.elapsed.inSeconds}s ago',
                    ),
                    SwitchListTile.adaptive(
                      title: const Text('NTP synchronization'),
                      value: enabled,
                      onChanged: !editable
                          ? null
                          : (v) => setState(() {
                              enabled = v;
                              dirty = true;
                            }),
                    ),
                    SwitchListTile.adaptive(
                      title: const Text('Accept DHCP NTP servers'),
                      value: dhcp,
                      onChanged: !editable
                          ? null
                          : (v) => setState(() {
                              dhcp = v;
                              dirty = true;
                            }),
                    ),
                    field(ntp, 'NTP server / fallback'),
                    field(date, 'HTTP Date service (HTTPS)'),
                    SwitchListTile.adaptive(
                      title: const Text('Automatically verify HTTP time'),
                      value: dateAuto,
                      onChanged: !editable
                          ? null
                          : (v) => setState(() {
                              dateAuto = v;
                              dirty = true;
                            }),
                    ),
                    OutlinedButton.icon(
                      onPressed: editable ? () => probe('http-date') : null,
                      icon: const Icon(Icons.fact_check_outlined),
                      label: const Text('Verify HTTP date'),
                    ),
                  ],
                ),
                const SizedBox(height: 16),
                WorkspaceSection(
                  title: 'Internet address',
                  icon: Icons.public,
                  children: [
                    SelectableText(
                      observed?['public_ip_observed'] == true
                          ? '${observed?['public_ip']}'
                          : 'Not observed',
                    ),
                    if (observed?['public_ip_observed'] == true)
                      Text(
                        'Observation age: ${((observed?['public_ip_age_ms'] as num? ?? 0) + observationAge.elapsedMilliseconds) ~/ 1000} s',
                      ),
                    field(ip, 'Public IPv4 lookup (HTTPS)'),
                    field(
                      interval,
                      'Automatic refresh interval (seconds)',
                      number: true,
                    ),
                    SwitchListTile.adaptive(
                      title: const Text('Automatically refresh public IP'),
                      value: ipAuto,
                      onChanged: !editable
                          ? null
                          : (v) => setState(() {
                              ipAuto = v;
                              dirty = true;
                            }),
                    ),
                    OutlinedButton.icon(
                      onPressed: editable ? () => probe('public-ip') : null,
                      icon: const Icon(Icons.travel_explore),
                      label: const Text('Look up public IP'),
                    ),
                  ],
                ),
                const SizedBox(height: 16),
                WorkspaceSection(
                  title: 'Network diagnostics',
                  icon: Icons.network_check,
                  children: [
                    field(target, 'Hostname or IPv4 address'),
                    Wrap(
                      spacing: 12,
                      runSpacing: 8,
                      children: [
                        OutlinedButton.icon(
                          onPressed: editable ? () => probe('dns') : null,
                          icon: const Icon(Icons.dns_outlined),
                          label: const Text('DNS query'),
                        ),
                        OutlinedButton.icon(
                          onPressed: editable ? () => probe('ping') : null,
                          icon: const Icon(Icons.speed),
                          label: const Text('Ping ×3'),
                        ),
                      ],
                    ),
                    if (job != null) ...[
                      const SizedBox(height: 12),
                      Text('${job['kind']} · ${job['phase']}'),
                      if ('${job['value'] ?? ''}'.isNotEmpty)
                        SelectableText('${job['value']}'),
                      if ('${job['error'] ?? ''}'.isNotEmpty)
                        Text(
                          '${job['error']}',
                          style: TextStyle(
                            color: Theme.of(context).colorScheme.error,
                          ),
                        ),
                      if (job['kind'] == 'ping')
                        Text(
                          '${job['ping_received']}/${job['ping_sent']} replies · ${job['ping_last_rtt_ms']} ms',
                        ),
                    ],
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
