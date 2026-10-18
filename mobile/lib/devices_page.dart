import 'package:flutter/material.dart';

import 'device_registry.dart';
import 'discovery.dart';
import 'icons.dart';
import 'api.dart';
import 'workspace_widgets.dart';

class DeviceListEntry {
  const DeviceListEntry(this.peer, this.saved, this.available);
  final DiscoveredDevice peer;
  final SavedDevice? saved;
  final bool available;
  String get name => saved?.name ?? peer.name;
}

List<DeviceListEntry> unifiedDevices(
  List<SavedDevice> saved,
  List<DiscoveredDevice> found,
  DiscoveryCatalog catalog,
) =>
    coalesceDevices([...saved.map((d) => d.identity), ...found])
        .map(
          (peer) => DeviceListEntry(
            peer,
            saved.where((d) => d.identity.sameBoard(peer)).firstOrNull,
            catalog.seenThisScan(peer),
          ),
        )
        .toList()
      ..sort((a, b) {
        final order = (b.available ? 1 : 0).compareTo(a.available ? 1 : 0);
        return order != 0
            ? order
            : a.name.toLowerCase().compareTo(b.name.toLowerCase());
      });

class DevicesPage extends StatefulWidget {
  const DevicesPage({
    super.key,
    required this.select,
    this.registry,
    this.discovery,
    this.apiFactory,
  });
  final Future<void> Function(SavedDevice, String) select;
  final DeviceRegistry? registry;
  final DeviceDiscovery? discovery;
  final PakarApi Function(SavedDevice, String)? apiFactory;
  @override
  State<DevicesPage> createState() => _DevicesPageState();
}

class _DevicesPageState extends State<DevicesPage> {
  late final registry = widget.registry ?? DeviceRegistry();
  late final discovery = widget.discovery ?? DeviceDiscovery();
  List<SavedDevice> saved = [];
  List<DiscoveredDevice> found = [];
  bool busy = false, connecting = false, authPromptActive = false;
  String search = '', filter = 'All';
  String message = '';
  @override
  void initState() {
    super.initState();
    scan();
  }

  Future<void> reload() async {
    try {
      final v = await registry.load();
      if (mounted) setState(() => saved = v);
    } on Object {
      if (mounted) {
        setState(
          () => message =
              'Saved devices unavailable; existing data was not replaced.',
        );
      }
    }
  }

  Future<void> scan() async {
    if (busy) return;
    setState(() => busy = true);
    try {
      await reload();
      final v = await discovery.scan(seeds: saved.map((d) => d.identity));
      if (mounted) {
        setState(() {
          found = v;
          message = discovery.errors.isEmpty
              ? ''
              : 'Scan incomplete: ${discovery.errors.keys.join(', ')}';
        });
      }
    } on Object {
      if (mounted) {
        setState(
          () => message =
              'Unable to refresh devices. Saved profiles are unchanged.',
        );
      }
    } finally {
      if (mounted) setState(() => busy = false);
    }
  }

  Future<void> edit({SavedDevice? device, DiscoveredDevice? discovered}) async {
    final name = TextEditingController(
      text: device?.name ?? discovered?.name ?? 'New device',
    );
    final endpoint = TextEditingController(
      text: device?.endpoint ?? discovered?.endpoint ?? 'http://ledboard.local',
    );
    final user = TextEditingController(text: device?.user ?? 'admin');
    final password = TextEditingController();
    var location = device?.location ?? DeviceLocation.local;
    var error = '', writing = false, clearPassword = false;
    await showDialog<void>(
      context: context,
      builder: (context) => StatefulBuilder(
        builder: (context, update) => PopScope(
          canPop: !writing,
          child: AlertDialog(
            title: Text(device == null ? 'Add device' : 'Device profile'),
            content: SizedBox(
              width: 480,
              child: SingleChildScrollView(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    TextField(
                      controller: name,
                      decoration: const InputDecoration(
                        labelText: 'Display name',
                      ),
                    ),
                    const SizedBox(height: 12),
                    TextField(
                      controller: endpoint,
                      decoration: const InputDecoration(
                        labelText: 'Hostname / HTTP(S) origin',
                      ),
                    ),
                    const SizedBox(height: 12),
                    DropdownButtonFormField<DeviceLocation>(
                      initialValue: location,
                      items: [
                        for (final l in DeviceLocation.values)
                          DropdownMenuItem(value: l, child: Text(l.name)),
                      ],
                      onChanged: writing
                          ? null
                          : (v) => update(() => location = v!),
                      decoration: const InputDecoration(
                        labelText: 'Local or remote transport',
                      ),
                    ),
                    if (device != null) ...[
                      const SizedBox(height: 12),
                      TextField(
                        controller: user,
                        decoration: const InputDecoration(
                          labelText: 'Username',
                        ),
                      ),
                      const SizedBox(height: 12),
                      TextField(
                        controller: password,
                        obscureText: true,
                        decoration: InputDecoration(
                          labelText: 'New password (optional)',
                          helperText: 'Leave blank to keep the saved password',
                        ),
                      ),
                      SwitchListTile.adaptive(
                        contentPadding: EdgeInsets.zero,
                        title: const Text('Remove saved password'),
                        value: clearPassword,
                        onChanged: writing
                            ? null
                            : (v) => update(() => clearPassword = v),
                      ),
                    ],
                    if (error.isNotEmpty) Text(error),
                  ],
                ),
              ),
            ),
            actions: [
              TextButton(
                onPressed: writing ? null : () => Navigator.pop(context),
                child: const Text('Cancel'),
              ),
              FilledButton(
                onPressed: writing
                    ? null
                    : () async {
                        update(() => writing = true);
                        try {
                          var next = SavedDevice(
                            id: device?.id ?? randomIdentifier(),
                            name: name.text.trim(),
                            endpoint: endpoint.text,
                            location: location,
                            user: user.text.trim(),
                          );
                          final identity = discovered ?? device?.identity;
                          if (identity != null &&
                              identity.endpoint == next.endpoint) {
                            next = next.observed(identity);
                          }
                          final secret =
                              clearPassword || device?.endpoint != next.endpoint
                              ? ''
                              : password.text.isEmpty
                              ? null
                              : password.text;
                          await registry.save(next, secret);
                          if (context.mounted) Navigator.pop(context);
                        } on Object {
                          update(() {
                            error = 'Device settings could not be saved; check origin and credentials.';
                            writing = false;
                          });
                        }
                      },
                child: const Text('Save'),
              ),
            ],
          ),
        ),
      ),
    );
    await Future<void>.delayed(const Duration(milliseconds: 250));
    name.dispose();
    endpoint.dispose();
    user.dispose();
    password.dispose();
    await reload();
  }

  Future<(String, String)?> credentials(SavedDevice device) async {
    setState(() => authPromptActive = true);
    final user = TextEditingController(text: device.user),
        password = TextEditingController();
    final result = await showDialog<(String, String)>(
      context: context,
      builder: (dialog) => AlertDialog(
        title: const Text('Sign in'),
        content: SizedBox(
          width: 400,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Text(device.name, style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 16),
              TextField(
                controller: user,
                autofillHints: const [AutofillHints.username],
                decoration: const InputDecoration(labelText: 'Username'),
              ),
              const SizedBox(height: 16),
              TextField(
                controller: password,
                obscureText: true,
                autofocus: true,
                enableSuggestions: false,
                autocorrect: false,
                autofillHints: const [AutofillHints.password],
                decoration: const InputDecoration(labelText: 'Password'),
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(dialog),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () =>
                Navigator.pop(dialog, (user.text.trim(), password.text)),
            child: const Text('Sign in'),
          ),
        ],
      ),
    );
    await Future<void>.delayed(const Duration(milliseconds: 250));
    user.dispose();
    password.dispose();
    if (mounted) setState(() => authPromptActive = false);
    return result;
  }

  Future<void> connect(DeviceListEntry entry) async {
    if (connecting) return;
    setState(() {
      connecting = true;
      message = '';
    });
    var device =
        entry.saved ??
        SavedDevice(
          id: randomIdentifier(),
          name: entry.name,
          endpoint: entry.peer.endpoint,
          location: DeviceLocation.local,
        ).observed(entry.peer);
    try {
      var secret = entry.saved == null
          ? ''
          : await registry.password(device) ?? '';
      for (var attempt = 0; attempt < 2; ++attempt) {
        final api =
            widget.apiFactory?.call(device, secret) ??
            PakarApi(
              baseUrl: device.endpoint,
              user: device.user,
              password: secret,
            );
        try {
          final status = await api.status();
          if (!status.peer.known ||
              !RegExp(r'^[!-~]{1,80}$')
                  .hasMatch('${status.json['board_id'] ?? ''}') ||
              !RegExp(r'^[!-~]{1,80}$')
                  .hasMatch('${status.json['boot_id'] ?? ''}')) {
            throw const FormatException(
              'Not a compatible PakarController board',
            );
          }
          final boardId = status.json['board_id'] as String? ?? '';
          if (device.boardId.isNotEmpty && device.boardId != boardId) {
            throw const FormatException(
              'The endpoint belongs to another board',
            );
          }
          if (!mounted) return;
          device = device.observed(
            DiscoveredDevice(
              status.peer.name,
              device.endpoint,
              entry.peer.protocol,
              boardId: boardId,
              mac: entry.peer.mac,
              udn: entry.peer.udn,
              addresses: entry.peer.ips,
              aliases: entry.peer.origins,
              verified: true,
            ),
          );
          device = await registry.save(device, secret);
          if (mounted) await widget.select(device, secret);
          return;
        } on AuthenticationRequired {
          if (!mounted) return;
          if (attempt > 0) {
            setState(
              () => message =
                  'Sign-in rejected. Check the username and password.',
            );
            return;
          }
        } finally {
          await api.close();
        }
        if (!mounted) return;
        final login = await credentials(device);
        if (login == null || !mounted) return;
        secret = login.$2;
        device = SavedDevice(
          id: device.id,
          name: device.name,
          endpoint: device.endpoint,
          location: device.location,
          user: login.$1,
        ).observed(device.identity);
      }
    } on Object catch (error) {
      if (mounted) {
        setState(
          () => message = error is FormatException
              ? error.message
              : 'Could not connect. Check the device address and network.',
        );
      }
    } finally {
      if (mounted) setState(() => connecting = false);
    }
  }

  Future<void> forget(SavedDevice device) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (dialog) => AlertDialog(
        title: const Text('Forget device?'),
        content: Text(device.name),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(dialog, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(dialog, true),
            child: const Text('Forget'),
          ),
        ],
      ),
    );
    if (confirmed == true) {
      try {
        await registry.remove(device);
        await reload();
      } on Object {
        if (mounted) {
          setState(() => message = 'Could not remove the saved profile.');
        }
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final entries = unifiedDevices(saved, found, discovery.catalog);
    final visible = entries.where(
      (entry) =>
          (filter != 'Saved' || entry.saved != null) &&
          (filter != 'Available' || entry.available) &&
          ('${entry.name} ${entry.peer.origins.join(' ')} ${entry.peer.mac}')
              .toLowerCase()
              .contains(search.toLowerCase()),
    );
    final count = entries.where((d) => d.available).length;
    final savedCount = entries.where((d) => d.saved != null).length;
    return WorkspaceBody(
      children: [
        WorkspaceHeader(
          title: 'Devices',
          icon: PakarIcons.circuitry,
          subtitle: '$count available · $savedCount saved',
          actions: [
            OutlinedButton.icon(
              onPressed: busy || connecting ? null : scan,
              icon: const Icon(PakarIcons.arrowClockwise),
              label: const Text('Discover'),
            ),
            const SizedBox(width: 8),
            FilledButton.icon(
              onPressed: connecting ? null : () => edit(),
              icon: const Icon(PakarIcons.link),
              label: const Text('Add device'),
            ),
          ],
        ),
        TextField(
          onChanged: (v) => setState(() => search = v),
          decoration: const InputDecoration(
            hintText: 'Find a device',
            prefixIcon: Icon(Icons.search),
          ),
        ),
        const SizedBox(height: 12),
        Wrap(
          spacing: 8,
          children: [
            for (final item in ['All', 'Available', 'Saved'])
              ChoiceChip(
                label: Text(item),
                selected: item == filter,
                showCheckmark: false,
                onSelected: (_) => setState(() => filter = item),
              ),
          ],
        ),
        const SizedBox(height: 12),
        SizedBox(
          height: 3,
          child: busy || connecting && !authPromptActive
              ? const LinearProgressIndicator()
              : null,
        ),
        const SizedBox(height: 12),
        if (message.isNotEmpty)
          Padding(
            padding: const EdgeInsets.only(bottom: 12),
            child: Semantics(liveRegion: true, child: Text(message)),
          ),
        if (visible.isEmpty)
          WorkspaceSection(
            title: busy ? 'Looking for boards…' : 'No matching devices',
            icon: PakarIcons.radio,
            children: [
              Text(
                search.isNotEmpty
                    ? 'Try another name or address.'
                    : 'Discover nearby boards or add a hostname.',
              ),
            ],
          ),
        for (final entry in visible)
          Padding(
            padding: const EdgeInsets.only(bottom: 8),
            child: AnimatedOpacity(
              duration: const Duration(milliseconds: 180),
              opacity: entry.saved != null && !entry.available && !busy
                  ? .48
                  : 1,
              child: Card(
                margin: EdgeInsets.zero,
                clipBehavior: Clip.antiAlias,
                child: ListTile(
                  contentPadding: const EdgeInsets.symmetric(
                    horizontal: 16,
                    vertical: 8,
                  ),
                  leading: Icon(
                    entry.saved?.location == DeviceLocation.remote
                        ? PakarIcons.cloud
                        : PakarIcons.circuitry,
                  ),
                  title: Text(
                    entry.name,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                  ),
                  subtitle: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        entry.saved?.endpoint ?? entry.peer.endpoint,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: TextStyle(
                          color: Theme.of(context).colorScheme.onSurfaceVariant,
                        ),
                      ),
                      const SizedBox(height: 8),
                      Wrap(
                        spacing: 6,
                        runSpacing: 6,
                        children: [
                          WorkspaceBadge(
                            entry.available
                                ? 'Available'
                                : busy
                                ? 'Checking'
                                : 'Not reached',
                            active: entry.available,
                          ),
                          if (entry.saved != null)
                            const WorkspaceBadge(
                              'Saved',
                              icon: PakarIcons.floppyDisk,
                            ),
                          if (entry.available && entry.peer.authRequired)
                            const WorkspaceBadge(
                              'Sign-in required',
                              icon: PakarIcons.lock,
                            ),
                          if (entry.available && entry.peer.protocol.isNotEmpty)
                            WorkspaceBadge(entry.peer.protocol),
                        ],
                      ),
                    ],
                  ),
                  onTap: connecting ? null : () => connect(entry),
                  trailing: PopupMenuButton<String>(
                    enabled: !connecting,
                    tooltip: 'Device actions',
                    onSelected: (value) => value == 'forget'
                        ? forget(entry.saved!)
                        : edit(device: entry.saved, discovered: entry.peer),
                    itemBuilder: (_) => [
                      PopupMenuItem(
                        value: 'edit',
                        child: Row(
                          children: [
                            Icon(
                              entry.saved == null
                                  ? PakarIcons.floppyDisk
                                  : PakarIcons.gear,
                              size: 18,
                            ),
                            const SizedBox(width: 10),
                            Text(
                              entry.saved == null
                                  ? 'Save device'
                                  : 'Edit profile',
                            ),
                          ],
                        ),
                      ),
                      if (entry.saved != null)
                        const PopupMenuItem(
                          value: 'forget',
                          child: Text('Forget device…'),
                        ),
                    ],
                  ),
                ),
              ),
            ),
          ),
      ],
    );
  }
}
