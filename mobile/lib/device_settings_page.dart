import 'dart:async';

import 'package:flutter/material.dart';

import 'api.dart';
import 'cell_search.dart';
import 'device_settings.dart';
import 'firmware_page.dart';
import 'icons.dart';
import 'workspace_widgets.dart';
import 'models.dart';
import 'peer_context.dart';
import 'status_rgb.dart';

/// Actual Pealayer PreferencesTab contract-driven sections, grouped cards,
/// labeled typed controls, contextual options, visible save/load status adapted
/// to Flutter. Pakar device writes are explicit (not donor optimistic autosave).
class DeviceSettingsPage extends StatefulWidget {
  const DeviceSettingsPage({
    super.key,
    required this.api,
    required this.status,
    required this.authorized,
    required this.navigate,
    required this.adoptConnection,
    this.active = true,
    this.workspace,
  });
  final PakarApi api;
  final DeviceStatus status;
  final bool active;
  final DeviceSettingsWorkspace? workspace;
  final bool Function(String feature) authorized;
  final void Function(String page) navigate;
  final Future<void> Function(String endpoint, String user, String password)
  adoptConnection;
  @override
  State<DeviceSettingsPage> createState() => _DeviceSettingsPageState();
}

class _DeviceSettingsPageState extends State<DeviceSettingsPage> {
  late final workspace = widget.workspace ?? DeviceSettingsWorkspace();
  late final session = DeviceSettingsSession(
    widget.api,
    authorized: () => widget.authorized('network'),
    workspace: workspace,
  );
  late final scroll = ScrollController(
    initialScrollOffset: workspace.vertical,
    keepScrollOffset: false,
  );
  late final find = TextEditingController(text: workspace.search);
  final fields = <String, TextEditingController>{};
  final host = TextEditingController(), name = TextEditingController();
  final login = TextEditingController(),
      current = TextEditingController(),
      replacement = TextEditingController(),
      confirmation = TextEditingController();
  final ownerReads = <String, int>{};
  Map<String, dynamic>? identity, access;
  List<String> ssids = [];
  String get page => workspace.page;
  set page(String value) => workspace.page = value;
  String get search => workspace.search;
  set search(String value) => workspace.search = value;
  String info = '';
  bool identityConflict = false, assigningIdentity = false;
  bool accessEnabled = false, accessConflict = false;
  bool get accessDirty =>
      access != null &&
      (accessEnabled != access!['enabled'] ||
          login.text != access!['username'] ||
          current.text.isNotEmpty ||
          replacement.text.isNotEmpty ||
          confirmation.text.isNotEmpty);
  bool working = false;
  bool rgbLocked = false;
  bool maintenanceActive = false;
  Timer? refresh;
  StreamSubscription<int>? configurationEvents;
  bool configurationDirty = false;
  int operation = 0;
  @override
  void initState() {
    super.initState();
    scroll.addListener(() {
      if (scroll.hasClients) workspace.vertical = scroll.offset;
    });
    host.addListener(identityEdited);
    name.addListener(identityEdited);
    session.addListener(changed);
    configurationEvents = widget.api.configurationUpdates.listen((_) {
      configurationDirty = true;
      unawaited(load());
    });
    refresh = Timer.periodic(const Duration(seconds: 15), (_) => load());
    unawaited(load());
  }

  void changed() {
    if (mounted) setState(() {});
  }

  void identityEdited() {
    if (assigningIdentity || identity == null) return;
    final original = workspace.identity;
    workspace.captureIdentity(
      original?['before_host'] ?? identity!['hostname'] as String,
      original?['before_name'] ?? identity!['display_name'] as String,
      host.text,
      name.text,
    );
  }

  @override
  void didUpdateWidget(covariant DeviceSettingsPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.active && !oldWidget.active) unawaited(load());
    if (maintenanceActive &&
        !widget.status.deviceSettingsPages.any(
          (entry) => entry['id'] == 'firmware',
        )) {
      // Revocation destroys the maintenance view/owner. Do not strand unrelated
      // settings behind its confirmation lock; no firmware action is resumed.
      working = maintenanceActive = false;
    }
    if (widget.api.connectionRevision != session.owner) {
      session.invalidate();
      access = null;
      accessEnabled = accessConflict = false;
      current.clear();
      replacement.clear();
      confirmation.clear();
    }
    if (widget.api.connectionRevision != session.owner) {
      identity = null;
      host.clear();
      name.clear();
    }
  }

  @override
  void dispose() {
    ++operation;
    refresh?.cancel();
    unawaited(configurationEvents?.cancel());
    session.removeListener(changed);
    session.dispose();
    scroll.dispose();
    find.dispose();
    if (widget.workspace == null) workspace.dispose();
    for (final controller in [
      ...fields.values,
      host,
      name,
      login,
      current,
      replacement,
      confirmation,
    ]) {
      controller.dispose();
    }
    super.dispose();
  }

  void check(String feature) {
    if (!mounted ||
        !widget.authorized(feature) ||
        widget.api.connectionRevision != session.owner) {
      throw const StaleConnection();
    }
  }

  Future<void> load() async {
    if (!mounted ||
        !widget.active ||
        !widget.api.appForeground ||
        working ||
        session.pending) {
      return;
    }
    final settingsChanged = configurationDirty;
    configurationDirty = false;
    if (widget.authorized('network') &&
        widget.api.supportsRoute('GET', '/api/network')) {
      await session.load(SettingsGroup.network);
      if (!mounted || !widget.active || !widget.authorized('network')) return;
      if (widget.api.supportsRoute('GET', '/api/cloud')) {
        await session.load(SettingsGroup.cloud);
      }
      if (mounted &&
          widget.status.json['ethernet'] is Map &&
          widget.status.json['ethernet']['configured'] == true &&
          widget.api.supportsRoute('GET', '/api/network/ethernet')) {
        await session.load(SettingsGroup.ethernet);
      }
    }
    if (mounted &&
        (identity == null || settingsChanged) &&
        widget.authorized('identity') &&
        widget.api.supportsRoute('GET', '/api/device/name')) {
      await loadIdentity();
    }
    if (mounted &&
        (access == null || settingsChanged) &&
        widget.authorized('network') &&
        widget.api.connectionMethod == ConnectionMethod.network) {
      await loadAccess();
    }
  }

  Future<void> readOwned(
    String feature,
    String path,
    void Function(Map<String, dynamic>) accept,
  ) async {
    final id = (ownerReads[path] ?? 0) + 1, captured = operation;
    ownerReads[path] = id;
    try {
      check(feature);
      final value = await widget.api.getJson(path);
      check(feature);
      if (captured != operation || ownerReads[path] != id || working) return;
      setState(() => accept(value));
    } catch (error) {
      if (mounted && captured == operation) {
        setState(() => info = error.toString());
      }
    }
  }

  Future<void> loadIdentity() =>
      readOwned('identity', '/api/device/name', (value) {
        if (value['hostname'] is! String ||
            !validHostname(value['hostname']) ||
            !validText(value['display_name'], 63)) {
          throw const FormatException('Device identity unavailable');
        }
        final draft = workspace.identity;
        identityConflict =
            draft != null &&
            (draft['before_host'] != value['hostname'] ||
                draft['before_name'] != value['display_name']);
        if (identityConflict) info = 'Another client changed identity. Draft retained; discard and reload before saving.';
        // Reloading observed identity must not replace a name draft.
        if (identity == null ||
            (host.text == identity!['hostname'] &&
                name.text == identity!['display_name'])) {
          assigningIdentity = true;
          host.text = draft?['host'] ?? value['hostname'];
          name.text = draft?['name'] ?? value['display_name'];
          assigningIdentity = false;
        }
        identity = value;
      });
  Future<void> loadAccess() => readOwned('network', '/api/auth/info', (value) {
    if (value['ready'] is! bool ||
        value['enabled'] is! bool ||
        value['default_credentials'] is! bool ||
        value['username'] is! String) {
      throw const FormatException('Device access information unavailable');
    }
    if (accessDirty &&
        (value['enabled'] != access!['enabled'] ||
            value['username'] != access!['username'])) {
      accessConflict = true;
      info = 'Device access changed. Draft retained; discard and reload before saving.';
      return;
    }
    if (!accessDirty) {
      login.text = value['username'];
      accessEnabled = value['enabled'] as bool;
    }
    access = value;
  });
  Future<void> configureBluetooth(bool enabled) async {
    if (working || session.pending) return;
    final owner = widget.api.connectionRevision;
    final overBluetooth = widget.api.isBluetooth;
    if (!enabled &&
        overBluetooth &&
        !await confirm(
          'Disable Bluetooth?',
          'This board will disconnect. Use Network to enable it again.',
        )) {
      return;
    }
    if (!mounted || owner != widget.api.connectionRevision) return;
    setState(() => working = true);
    try {
      final actual = await widget.api.postJson(
        '/api/ble',
        {'enabled': '$enabled'},
        authorize: () =>
            mounted &&
            owner == widget.api.connectionRevision &&
            widget.api.connectionWanted,
      );
      if (actual['enabled'] != enabled ||
          owner != widget.api.connectionRevision) {
        throw const OperationUncertain();
      }
      if (!enabled && overBluetooth) {
        widget.api.disconnect();
      } else {
        final observed = await widget.api.getJson('/api/ble');
        if (observed['enabled'] != enabled ||
            owner != widget.api.connectionRevision) {
          throw const OperationUncertain();
        }
        await widget.api.status();
      }
      if (mounted) {
        setState(
          () => info = enabled ? 'Bluetooth enabled' : 'Bluetooth disabled',
        );
      }
    } on Object catch (error) {
      if (mounted) setState(() => info = '$error');
    } finally {
      if (mounted) setState(() => working = false);
    }
  }

  Future<bool> confirm(String title, String detail) async =>
      await showDialog<bool>(
        context: context,
        builder: (context) => AlertDialog(
          title: Text(title),
          content: Text(detail),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(context, true),
              child: const Text('Continue'),
            ),
          ],
        ),
      ) ??
      false;
  Future<void> reload() async {
    if (working || session.pending) return;
    final identityDirty =
        identity != null &&
        (host.text != identity!['hostname'] ||
            name.text != identity!['display_name']);
    if ((session.dirty || identityDirty || accessDirty) &&
        !await confirm(
          'Discard and reload?',
          'Discard unsaved device and secret edits, then read the selected board again?',
        )) {
      return;
    }
    if (!mounted) return;
    for (final group in SettingsGroup.values) {
      workspace.discard(group);
    }
    workspace.discardIdentity();
    session.invalidate();
    identity = access = null;
    identityConflict = false;
    accessEnabled = accessConflict = false;
    current.clear();
    replacement.clear();
    confirmation.clear();
    await load();
  }

  Future<void> saveGroup(SettingsGroup group) async {
    if (working || session.pending) return;
    try {
      session.write(group);
    } catch (error) {
      setState(() => info = error.toString());
      return;
    }
    if (group != SettingsGroup.cloud &&
        !await confirm(
          'Save network policy?',
          'Connectivity may change. No uncertain write is retried on another route. Keep a working recovery path.',
        )) {
      return;
    }
    if (mounted) {
      setState(() => info = '');
      await session.save(group);
    }
  }

  Future<void> runWrite(String feature, Future<void> Function() action) async {
    if (!mounted || working || session.pending) return;
    ++operation;
    setState(() {
      working = true;
      info = '';
    });
    try {
      check(feature);
      await action();
    } catch (error) {
      if (mounted) setState(() => info = '$error No write was replayed.');
    } finally {
      if (mounted) setState(() => working = false);
    }
  }

  Future<void> saveIdentity() async {
    if (identityConflict) {
      setState(
        () => info = 'Identity changed since this draft. Discard and reload before saving.',
      );
      return;
    }
    final next = host.text.toLowerCase().replaceFirst(RegExp(r'\.local$'), '');
    final label = name.text;
    if (!validHostname(next) ||
        !validText(label, 63) ||
        label.trim() != label) {
      setState(
        () => info = 'Use a valid hostname and a 1–63 UTF-8 byte board name without edge spaces or controls',
      );
      return;
    }
    if (identity == null ||
        !await confirm(
          'Save board identity?',
          'Save $label as $next.local. If the hostname changes, this client will follow it using its existing credentials.',
        )) {
      return;
    }
    await runWrite('identity', () async {
      final prior = await widget.api.getJson('/api/device/name');
      check('identity');
      if (prior['hostname'] != identity!['hostname'] ||
          prior['display_name'] != identity!['display_name']) {
        throw const FormatException(
          'Another client changed identity; reload before saving',
        );
      }
      final ack = await widget.api.postJson(
        '/api/device/name',
        {'hostname': next, 'display_name': label},
        authorize: () =>
            mounted &&
            widget.authorized('identity') &&
            widget.api.connectionRevision == session.owner,
      );
      if (widget.api.connectionRevision != session.owner) {
        throw const StaleConnection();
      }
      if (ack['accepted'] != true ||
          ack['persisted'] != true ||
          ack['hostname'] != next ||
          ack['display_name'] != label) {
        throw const FormatException(
          'Device did not confirm persisted identity',
        );
      }
      if (ack['apply_pending'] == true) {
        workspace.discardIdentity();
        await widget.adoptConnection(
          'http://$next.local',
          widget.api.user,
          widget.api.password,
        );
      } else {
        final actual = await widget.api.getJson('/api/device/name');
        check('identity');
        if (actual['hostname'] != next || actual['display_name'] != label) {
          throw const FormatException('Saved identity readback differs');
        }
        identity = actual;
        host.text = next;
        workspace.discardIdentity();
        info = 'Saved identity verified without Wi-Fi restart';
      }
    });
  }

  Future<void> saveAccess() async {
    final baseline = access;
    if (baseline?['ready'] != true || accessConflict) {
      setState(() => info = 'Read current device access before saving.');
      return;
    }
    final enabled = accessEnabled, wasEnabled = baseline!['enabled'] == true;
    final replaceCredentials =
        enabled &&
        (!wasEnabled ||
            login.text != baseline['username'] ||
            replacement.text.isNotEmpty ||
            confirmation.text.isNotEmpty);
    if (enabled == wasEnabled && !replaceCredentials) {
      setState(() => info = 'Device access is already configured this way.');
      return;
    }
    final username = replaceCredentials
        ? login.text
        : baseline['username'] as String;
    final password = replaceCredentials ? replacement.text : '';
    final currentPassword = wasEnabled
        ? (current.text.isNotEmpty ? current.text : widget.api.password)
        : '';
    if (wasEnabled && currentPassword.isEmpty) {
      setState(
        () => info =
            'Enter the current password to change protected device access.',
      );
      return;
    }
    if (replaceCredentials &&
        (!RegExp(r'^[A-Za-z0-9_.-]{1,32}$').hasMatch(username) ||
            !RegExp(r'^[\x20-\x7e]{8,64}$').hasMatch(password) ||
            password != confirmation.text)) {
      setState(
        () => info = 'Enter a valid username and matching new password (8–64 printable ASCII characters)',
      );
      return;
    }
    if (!await confirm(
      enabled ? 'Save device access?' : 'Disable authentication?',
      enabled
          ? 'Existing sessions will be revoked. This client will reconnect using the new login; secrets stay in platform encrypted storage.'
          : 'Clients will no longer need a login. This client will reconnect without saved credentials.',
    )) {
      return;
    }
    await runWrite('network', () async {
      final prior = await widget.api.getJson('/api/auth/info');
      check('network');
      if (prior['ready'] != true ||
          prior['enabled'] != baseline['enabled'] ||
          prior['username'] != baseline['username']) {
        accessConflict = true;
        throw const FormatException(
          'Device access changed; discard and reload before saving',
        );
      }
      final ack = await widget.api.postJson(
        '/api/auth',
        {
          'enabled': '$enabled',
          'current_password': currentPassword,
          'username': replaceCredentials ? username : '',
          'password': password,
        },
        authorize: () =>
            mounted &&
            widget.authorized('network') &&
            widget.api.connectionRevision == session.owner,
      );
      if (widget.api.connectionRevision != session.owner) {
        throw const StaleConnection();
      }
      if (ack['updated'] != true ||
          ack['enabled'] != enabled ||
          ack['reauthenticate'] != enabled) {
        throw const FormatException('Device did not confirm credential update');
      }
      // Public readback still works with the retired credentials. Follow only
      // this owner's exact ACK; a disposed old view may still finish handoff.
      final actual = await widget.api.getJson('/api/auth/info');
      if (widget.api.connectionRevision != session.owner) {
        throw const StaleConnection();
      }
      if (actual['ready'] != true ||
          actual['enabled'] != enabled ||
          actual['username'] != username) {
        throw const FormatException(
          'Access save acknowledged but readback differs; no write replayed',
        );
      }
      if (mounted) {
        current.clear();
        replacement.clear();
        confirmation.clear();
      }
      await widget.adoptConnection(
        widget.api.baseUrl,
        enabled ? username : '',
        enabled ? password : '',
      );
    });
  }

  Future<void> scan() => runWrite('network', () async {
    var value = await widget.api.getJson('/api/wifi/scan');
    for (var attempt = 0; value['running'] == true && attempt < 30; attempt++) {
      await Future<void>.delayed(const Duration(milliseconds: 500));
      check('network');
      value = await widget.api.getJson('/api/wifi/scan?results=true');
    }
    check('network');
    if (value['running'] == true ||
        value['error'] != null ||
        value['networks'] is! List) {
      throw const FormatException('Wi-Fi scan unavailable');
    }
    ssids =
        (value['networks'] as List)
            .whereType<Map>()
            .map((v) => v['ssid'])
            .whereType<String>()
            .where((v) => validText(v, 32))
            .toSet()
            .toList()
          ..sort();
    info = '${ssids.length} SSIDs reported by this board';
  });
  TextEditingController controller(String id, String value) {
    final field = fields.putIfAbsent(
      id,
      () => TextEditingController(text: value),
    );
    if (field.text != value) {
      field.value = TextEditingValue(
        text: value,
        selection: TextSelection.collapsed(offset: value.length),
      );
    }
    return field;
  }

  Widget textField(
    String id,
    String label,
    String value,
    void Function(String) edit, {
    bool secret = false,
    bool enabled = true,
    bool number = false,
  }) => Padding(
    padding: const EdgeInsets.only(bottom: 12),
    child: TextField(
      key: ValueKey(id),
      controller: controller(id, value),
      enabled: enabled && !working && !session.pending,
      obscureText: secret,
      enableSuggestions: !secret,
      autocorrect: !secret,
      keyboardType: number ? TextInputType.number : TextInputType.text,
      decoration: InputDecoration(
        labelText: label,
        border: const OutlineInputBorder(),
      ),
      onChanged: edit,
    ),
  );
  Widget card(String title, List<Widget> children) =>
      WorkspaceSection(title: title, icon: PakarIcons.gear, children: children);
  Widget publicField(SettingsGroup group, String key, String label) {
    final draft = session.drafts[group]!;
    if (!draft.value.containsKey(key)) return const SizedBox.shrink();
    final type = group.fields[key]?.replaceFirst('optional:', '');
    if (type == 'boolean') {
      return SwitchListTile.adaptive(
        contentPadding: EdgeInsets.zero,
        title: Text(label),
        value: draft.value[key] == true,
        onChanged: working || session.pending
            ? null
            : (value) => session.set(group, key, value),
      );
    }
    final numeric = ['port', 'interval'].contains(type);
    return textField(
      '$group:$key',
      label,
      '${draft.value[key]}',
      (value) {
        session.set(
          group,
          key,
          numeric ? int.tryParse(latinDigits(value)) ?? value : value,
        );
      },
      number: numeric,
      secret: key == 'ntfy_topic',
    );
  }

  Widget secretField(
    String key,
    String label,
    SecretEdit edit,
    bool presence,
  ) => Column(
    children: [
      DropdownButtonFormField<SecretMode>(
        isExpanded: true,
        icon: const Icon(PakarIcons.caretDown),
        key: ValueKey('$key:${edit.mode.name}'),
        initialValue: edit.mode,
        decoration: InputDecoration(labelText: '$label handling'),
        items: [
          DropdownMenuItem(
            value: SecretMode.keep,
            child: Text(
              presence ? 'Keep stored $label' : 'Leave $label unset',
              overflow: TextOverflow.ellipsis,
            ),
          ),
          const DropdownMenuItem(
            value: SecretMode.replace,
            child: Text('Replace'),
          ),
          const DropdownMenuItem(
            value: SecretMode.clear,
            child: Text('Clear stored value'),
          ),
        ],
        onChanged: working || session.pending
            ? null
            : (mode) => setState(() {
                edit.mode = mode!;
                if (mode != SecretMode.replace) edit.value = '';
              }),
      ),
      const SizedBox(height: 12),
      if (edit.mode == SecretMode.replace)
        textField(key, 'Replacement $label', edit.value, (value) {
          edit.value = value;
          session.changed();
        }, secret: true),
    ],
  );
  List<Widget> addressFields(SettingsGroup group) {
    final draft = session.drafts[group];
    if (draft == null || !draft.value.containsKey('dhcp')) return const [];
    return [
      publicField(group, 'dhcp', 'Automatic IPv4 addressing (DHCP)'),
      if (draft.value['dhcp'] == false)
        for (final entry in const {
          'address': 'IPv4 address',
          'gateway': 'Gateway',
          'subnet': 'Subnet mask',
          'dns1': 'Primary DNS',
          'dns2': 'Secondary DNS',
        }.entries)
          publicField(group, entry.key, entry.value),
    ];
  }

  Widget ethernetEditor() {
    const group = SettingsGroup.ethernet;
    final draft = session.drafts[group];
    final runtime = widget.status.json['ethernet'] as Map;
    return card('Wired Ethernet', [
      Text(
        runtime['ipv4_ready'] == true
            ? 'IPv4 ready · ${runtime['ip'] ?? 'Address unknown'}'
            : runtime['link_up'] == true
            ? 'Link up · waiting for IPv4'
            : 'Link down',
      ),
      if (runtime['speed_mbps'] is num)
        Text(
          '${runtime['speed_mbps']} Mbps · ${runtime['full_duplex'] == true
              ? 'full duplex'
              : runtime['full_duplex'] == false
              ? 'half duplex'
              : 'duplex unknown'}',
        ),
      if (draft == null)
        OutlinedButton(
          onPressed: working || session.pending
              ? null
              : () => session.load(group),
          child: const Text('Read Ethernet settings'),
        )
      else ...[
        publicField(group, 'enabled', 'Enable Ethernet'),
        ...addressFields(group),
        publicField(group, 'lldp_enabled', 'Advertise link identity (LLDP)'),
        publicField(
          group,
          'lldp_receive',
          'Advanced: receive LLDP neighbors · promiscuous Ethernet',
        ),
        if (draft.conflict)
          const Text(
            'Ethernet settings changed remotely. Discard and reload before saving.',
          ),
        Align(
          alignment: Alignment.centerRight,
          child: FilledButton.icon(
            onPressed:
                working || session.pending || draft.conflict || !draft.dirty
                ? null
                : () => saveGroup(group),
            icon: const Icon(PakarIcons.floppyDisk),
            label: const Text('Save Ethernet and verify'),
          ),
        ),
      ],
    ]);
  }

  Widget groupEditor(SettingsGroup group) {
    final draft = session.drafts[group];
    if (draft == null) {
      return card('Saved settings unavailable', [
        const Text('No editable defaults. Reload to read the real device.'),
      ]);
    }
    final cloud = group == SettingsGroup.cloud, mqtt = page == 'mqtt';
    return card(
      cloud
          ? mqtt
                ? 'MQTT broker'
                : 'Outbound WebSocket'
          : 'Wi-Fi policy',
      [
        if (!cloud) ...[
          publicField(group, 'station_enabled', 'Station enabled'),
          publicField(group, 'ap_enabled', 'Recovery AP enabled'),
          const SizedBox(height: 8),
          publicField(group, 'station_ssid', 'Station SSID'),
          if (widget.api.supportsRoute('GET', '/api/wifi/scan'))
            OutlinedButton.icon(
              onPressed: working || session.pending ? null : scan,
              icon: const Icon(PakarIcons.wifiHigh),
              label: const Text('Scan Wi-Fi'),
            ),
          if (ssids.isNotEmpty)
            DropdownButtonFormField<String>(
              isExpanded: true,
              icon: const Icon(PakarIcons.caretDown),
              decoration: const InputDecoration(labelText: 'Scanned SSIDs'),
              items: ssids
                  .map(
                    (ssid) => DropdownMenuItem(
                      value: ssid,
                      child: Text(ssid, overflow: TextOverflow.ellipsis),
                    ),
                  )
                  .toList(),
              onChanged: working || session.pending
                  ? null
                  : (value) => session.set(group, 'station_ssid', value!),
            ),
          const SizedBox(height: 12),
          textField(
            'wifi-password',
            'Replacement password · blank keeps stored',
            session.wifiPassword,
            (value) {
              session.wifiPassword = value;
              session.changed();
            },
            secret: true,
            enabled: !session.openNetwork,
          ),
          SwitchListTile.adaptive(
            contentPadding: EdgeInsets.zero,
            title: const Text('Open station network · no password'),
            value: session.openNetwork,
            onChanged: working || session.pending
                ? null
                : (value) => setState(() => session.openNetwork = value),
          ),
          const Text(
            'Blank keeps the stored password for the unchanged SSID. Wi-Fi can be fully disabled only after Ethernet has a working IPv4 link.',
          ),
          ...addressFields(group),
        ] else ...[
          publicField(
            group,
            mqtt ? 'mqtt_enabled' : 'websocket_enabled',
            mqtt ? 'Enable MQTT' : 'Enable outbound WebSocket',
          ),
          publicField(group, mqtt ? 'mqtt_tls' : 'websocket_tls', 'TLS'),
          const SizedBox(height: 8),
          publicField(
            group,
            mqtt ? 'mqtt_host' : 'websocket_host',
            mqtt ? 'Broker hostname' : 'Server hostname',
          ),
          publicField(group, mqtt ? 'mqtt_port' : 'websocket_port', 'Port'),
          if (mqtt) ...[
            publicField(group, 'mqtt_user', 'Username'),
            publicField(group, 'mqtt_base_topic', 'Base topic'),
          ] else
            publicField(group, 'websocket_path', 'Path'),
          secretField(
            mqtt ? 'mqtt-password' : 'websocket-token',
            mqtt ? 'password' : 'token',
            mqtt ? session.mqttPassword : session.websocketToken,
            draft.value[mqtt ? 'mqtt_password_set' : 'websocket_token_set'] ==
                true,
          ),
          publicField(
            group,
            'telemetry_seconds',
            'Telemetry interval · seconds',
          ),
          const Text(
            'MQTT and WebSocket share one saved cloud configuration. Both public domains and secret-presence flags are verified on save.',
          ),
        ],
        if (draft.conflict)
          const Padding(
            padding: EdgeInsets.symmetric(vertical: 12),
            child: Text(
              'Conflict: another client changed this configuration. Your draft is retained; discard and reload before saving.',
            ),
          ),
        const SizedBox(height: 16),
        Align(
          alignment: Alignment.centerRight,
          child: FilledButton.icon(
            onPressed: working || session.pending || draft.conflict
                ? null
                : () => saveGroup(group),
            icon: const Icon(PakarIcons.floppyDisk),
            label: const Text('Save and verify'),
          ),
        ),
      ],
    );
  }

  Widget detail({bool fallback = false}) => switch (fallback
      ? 'overview'
      : page) {
    'wifi' => Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        groupEditor(SettingsGroup.network),
        if (widget.status.json['ethernet'] is Map &&
            widget.status.json['ethernet']['configured'] == true)
          ethernetEditor(),
      ],
    ),
    'mqtt' || 'websocket' => groupEditor(SettingsGroup.cloud),
    'identity' => card('Board identity', [
      TextField(
        controller: host,
        enabled: identity != null && !working,
        decoration: const InputDecoration(
          labelText: 'Hostname · optional .local',
        ),
      ),
      const SizedBox(height: 12),
      TextField(
        controller: name,
        enabled: identity != null && !working,
        decoration: const InputDecoration(labelText: 'Board display name'),
      ),
      const SizedBox(height: 12),
      Text(
        'Current application: ${widget.status.peer.application?.title ?? 'Not advertised'}',
      ),
      const Text(
        'Application title and artwork come from the running firmware; they are not fabricated client settings.',
      ),
      const SizedBox(height: 16),
      FilledButton.icon(
        onPressed: identity == null || working || session.pending
            ? null
            : saveIdentity,
        icon: const Icon(PakarIcons.floppyDisk),
        label: const Text('Save identity'),
      ),
    ]),
    'access' => card('Device access', [
      if (access == null)
        const Text('Access information unavailable')
      else
        Text(
          access!['ready'] != true
              ? 'Storage is not ready; physical USB recovery is required.'
              : access!['enabled'] != true
              ? 'Authentication is disabled.'
              : access!['default_credentials'] == true
              ? 'Default credentials active. Change them here.'
              : 'Configured device credentials active.',
        ),
      SwitchListTile.adaptive(
        key: const ValueKey('device-access-enabled'),
        contentPadding: EdgeInsets.zero,
        title: const Text('Require authentication'),
        value: accessEnabled,
        onChanged:
            access?['ready'] != true ||
                working ||
                session.pending ||
                accessConflict
            ? null
            : (enabled) => setState(() => accessEnabled = enabled),
      ),
      if (accessConflict)
        Text(
          'Device access changed. Discard and reload before saving.',
          style: TextStyle(color: Theme.of(context).colorScheme.error),
        ),
      if (access?['enabled'] == true) ...[
        TextField(
          controller: current,
          enabled: !working && !session.pending && !accessConflict,
          obscureText: true,
          enableSuggestions: false,
          autocorrect: false,
          decoration: InputDecoration(
            labelText: 'Current password',
            helperText: widget.api.password.isNotEmpty
                ? 'Leave blank to use the current connection credentials'
                : null,
          ),
        ),
        const SizedBox(height: 12),
      ],
      if (accessEnabled) ...[
        TextField(
          controller: login,
          enabled: !working && !session.pending && !accessConflict,
          decoration: const InputDecoration(labelText: 'Username'),
        ),
        const SizedBox(height: 12),
        for (final entry in [
          (replacement, 'New password'),
          (confirmation, 'Confirm new password'),
        ]) ...[
          TextField(
            controller: entry.$1,
            enabled: !working && !session.pending && !accessConflict,
            obscureText: true,
            enableSuggestions: false,
            autocorrect: false,
            decoration: InputDecoration(labelText: entry.$2),
          ),
          const SizedBox(height: 12),
        ],
      ],
      const SizedBox(height: 16),
      FilledButton.icon(
        onPressed:
            access?['ready'] != true ||
                working ||
                session.pending ||
                accessConflict
            ? null
            : saveAccess,
        icon: const Icon(PakarIcons.lock),
        label: const Text('Save device access'),
      ),
    ]),
    'leds' || 'front-panel' || 'avr' => card('Actual board controls', [
      const Text(
        'Open the existing live controls; no cloned settings or build-only switches.',
      ),
      const SizedBox(height: 16),
      FilledButton.icon(
        onPressed: () => widget.navigate(
          page == 'leds'
              ? widget.status.peer.tableSelection
                    ? 'table'
                    : 'output'
              : page == 'avr'
              ? 'hardware'
              : 'front-panel',
        ),
        icon: const Icon(PakarIcons.link),
        label: const Text('Open live controls'),
      ),
    ]),
    'firmware' => Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        if (session.drafts[SettingsGroup.cloud]?.value.containsKey(
              'ntfy_enabled',
            ) ==
            true)
          card('Firmware notifications · ntfy', [
            publicField(
              SettingsGroup.cloud,
              'ntfy_enabled',
              'Send firmware-update notifications (opt in)',
            ),
            publicField(SettingsGroup.cloud, 'ntfy_host', 'HTTPS server'),
            publicField(SettingsGroup.cloud, 'ntfy_topic', 'Private topic'),
            secretField(
              'ntfy-token',
              'token',
              session.ntfyToken,
              session.drafts[SettingsGroup.cloud]!.value['ntfy_token_set'] ==
                  true,
            ),
            const Text(
              'Disabled by default. Major stages and results only; flashing never waits for delivery. No ntfy message can trigger an update.',
            ),
            FilledButton.icon(
              onPressed: working || session.pending
                  ? null
                  : () => saveGroup(SettingsGroup.cloud),
              icon: const Icon(PakarIcons.floppyDisk),
              label: const Text('Save notifications and verify'),
            ),
          ]),
        FirmwarePage(
          key: const ValueKey('board-firmware-settings'),
          api: widget.api,
          authorized: widget.authorized,
          busyChanged: (value) {
            if (mounted) {
              setState(() {
                working = value;
                maintenanceActive = value;
              });
            }
          },
        ),
      ],
    ),
    _ => card('Observed device', [
      Text(widget.status.peer.name),
      Text('${widget.status.hostname}.local'),
      if (widget.api.latestStatus?.json['ble'] case final Map ble
          when ble['configured'] == true && ble['enabled'] is bool)
        SwitchListTile.adaptive(
          contentPadding: EdgeInsets.zero,
          title: const Text('Board Bluetooth LE'),
          value: ble['enabled'] as bool,
          onChanged: working || session.pending || !widget.api.connectionWanted
              ? null
              : configureBluetooth,
        ),
      for (final entry in widget.status.observedBuildInformation.entries)
        Text('${entry.key}: ${entry.value}'),
      const SizedBox(height: 12),
      const Text(
        'UART mode, AVR clock/fuses, pins, chipset/channel order and pixel count are build parameters, not ineffective runtime switches.',
      ),
    ]),
  };
  IconData pageIcon(String icon) => switch (icon) {
    'GLOBE' => PakarIcons.globe,
    'WIFI_HIGH' => PakarIcons.wifiHigh,
    'BROADCAST' => PakarIcons.broadcast,
    'LINK' => PakarIcons.link,
    'LOCK' => PakarIcons.lock,
    'GRID_FOUR' => PakarIcons.gridFour,
    'GAUGE' => PakarIcons.gauge,
    'CPU' => PakarIcons.cpu,
    'FLOPPY_DISK' => PakarIcons.floppyDisk,
    _ => PakarIcons.circuitry,
  };
  @override
  Widget build(BuildContext context) {
    final pages = widget.status.deviceSettingsPages
        .where((entry) => !widget.api.isBluetooth || entry['id'] != 'access')
        .where(
          (entry) =>
              !widget.api.isUsb ||
              {'overview', 'leds', 'front-panel', 'avr'}.contains(entry['id']),
        )
        .toList();
    final pageAvailable = pages.any((p) => p['id'] == page);
    final visible = pages
        .where(
          (p) => '${p['label']} ${p['keywords']}'.toLowerCase().contains(
            search.toLowerCase(),
          ),
        )
        .toList();
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.all(16),
          child: Row(
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'Device settings',
                      style: Theme.of(context).textTheme.headlineSmall,
                    ),
                    Text(
                      '${widget.status.peer.name} · ${widget.status.hostname}.local',
                    ),
                  ],
                ),
              ),
              IconButton(
                tooltip: 'Discard and reload device settings',
                onPressed: working || session.pending ? null : reload,
                icon: const Icon(PakarIcons.arrowClockwise),
              ),
            ],
          ),
        ),
        if ((working && !maintenanceActive) || session.pending)
          const LinearProgressIndicator(),
        Expanded(
          child: LayoutBuilder(
            builder: (context, bounds) {
              final wide = bounds.maxWidth >= 760;
              final navigation = Column(
                children: [
                  TextField(
                    controller: find,
                    decoration: const InputDecoration(
                      labelText: 'Find setting',
                      prefixIcon: Icon(PakarIcons.gear),
                    ),
                    onChanged: (value) => setState(() => search = value),
                  ),
                  const SizedBox(height: 12),
                  if (!wide)
                    Wrap(
                      spacing: 8,
                      runSpacing: 4,
                      children: [
                        for (final p in visible)
                          ChoiceChip(
                            avatar: Icon(
                              pageIcon(p['icon'] as String),
                              size: 18,
                            ),
                            label: Text(p['label'] as String),
                            selected: page == p['id'],
                            showCheckmark: false,
                            onSelected: working || session.pending
                                ? null
                                : (_) =>
                                      setState(() => page = p['id'] as String),
                          ),
                      ],
                    )
                  else
                    for (final p in visible)
                      ListTile(
                        dense: true,
                        leading: Icon(pageIcon(p['icon'] as String)),
                        title: Text(p['label'] as String),
                        selected: page == p['id'],
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(8),
                        ),
                        onTap: working || session.pending
                            ? null
                            : () => setState(() => page = p['id'] as String),
                      ),
                ],
              );
              final body = ListView(
                controller: scroll,
                padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
                children: [
                  if (!wide) ...[navigation, const SizedBox(height: 12)],
                  detail(fallback: !pageAvailable),
                  Visibility(
                    visible: page == 'overview' || page == 'leds',
                    maintainState: true,
                    child: StatusRgbSettings(
                      key: const ValueKey('onboard-status-rgb'),
                      api: widget.api,
                      status: widget.status,
                      authorized: widget.authorized,
                      blocked:
                          session.pending ||
                          maintenanceActive ||
                          (working && !rgbLocked),
                      lockChanged: (value) {
                        if (mounted) {
                          setState(() {
                            rgbLocked = value;
                            working = value;
                          });
                        }
                      },
                    ),
                  ),
                  const SizedBox(height: 12),
                  Semantics(
                    liveRegion: true,
                    child: Text(info.isNotEmpty ? info : session.message),
                  ),
                ],
              );
              return wide
                  ? Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        SizedBox(
                          width: 210,
                          child: SingleChildScrollView(
                            padding: const EdgeInsets.only(left: 16),
                            child: navigation,
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(child: body),
                      ],
                    )
                  : body;
            },
          ),
        ),
      ],
    );
  }
}
