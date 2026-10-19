import 'package:flutter/material.dart';

import 'app_update_coordinator.dart';
import 'icons.dart';
import 'workspace_widgets.dart';

class AppUpdatesPage extends StatefulWidget {
  const AppUpdatesPage({
    super.key,
    required this.coordinator,
    this.extensions = const [],
  });
  final AppUpdateCoordinator coordinator;
  final List<Widget> extensions;
  @override
  State<AppUpdatesPage> createState() => _AppUpdatesPageState();
}

class _AppUpdatesPageState extends State<AppUpdatesPage> {
  AppUpdateCoordinator get model => widget.coordinator;
  final source = TextEditingController(),
      digest = TextEditingController(),
      size = TextEditingController(),
      enrollment = TextEditingController(),
      peerAddress = TextEditingController();
  final fields = <String, TextEditingController>{};
  late Map<String, dynamic> draft = Map.of(model.settings);
  bool working = false;
  String error = '';
  @override
  void initState() {
    super.initState();
    for (final key in [
      'catalog',
      'origin',
      'feed',
      'namespace',
      'event',
      'management_base',
      'check_interval_seconds',
    ]) {
      fields[key] = TextEditingController(text: '${draft[key]}');
    }
  }

  @override
  void dispose() {
    for (final c in [
      source,
      digest,
      size,
      peerAddress,
      enrollment,
      ...fields.values,
    ]) {
      c.dispose();
    }
    super.dispose();
  }

  Future<void> run(Future<dynamic> Function() action) async {
    if (working) return;
    setState(() {
      working = true;
      error = '';
    });
    try {
      await action();
    } on Object catch (e) {
      if (mounted) setState(() => error = '$e');
    } finally {
      if (mounted) setState(() => working = false);
    }
  }

  Future<void> savePreferences() async {
    await model.configure({
      ...draft,
      for (final entry in fields.entries)
        entry.key: entry.key == 'check_interval_seconds'
            ? int.tryParse(entry.value.text.trim())
            : entry.value.text.trim(),
    });
    draft = Map.of(model.settings);
  }

  Future<void> removeSelf() async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (dialog) => AlertDialog(
        title: const Text('Remove PakarController?'),
        content: const Text(
          'Android will ask you to confirm uninstalling this app. Its local settings and enrollment may be removed. No board or other app will be uninstalled.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(dialog, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(dialog, true),
            child: const Text('Continue to Android'),
          ),
        ],
      ),
    );
    if (!mounted || confirmed != true || !model.foreground) return;
    await model.updates.uninstallSelf(confirmed: true);
    model.message =
        'Android removal confirmation requested; removal is not verified';
    model.changed();
  }

  Widget action(
    String label,
    IconData icon,
    Future<dynamic> Function() callback, {
    bool enabled = true,
  }) => OutlinedButton.icon(
    onPressed: working || model.updates.busy || !enabled
        ? null
        : () => run(callback),
    icon: Icon(icon, size: 18),
    label: Text(label),
  );
  Widget field(TextEditingController c, String label, {bool number = false}) =>
      Padding(
        padding: const EdgeInsets.only(bottom: 12),
        child: TextField(
          controller: c,
          enabled: !working && !model.updates.busy,
          keyboardType: number ? TextInputType.number : TextInputType.url,
          autocorrect: false,
          decoration: InputDecoration(labelText: label),
        ),
      );
  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: model,
    builder: (context, _) {
      final state = model.updates.state, busy = model.updates.busy;
      final total = state['bytes'], count = state['received'] ?? state['sent'];
      final fraction =
          total is int &&
              total > 0 &&
              count is int &&
              count >= 0 &&
              count <= total
          ? count / total
          : null;
      return WorkspaceBody(
        children: [
          WorkspaceHeader(
            title: 'App updates',
            icon: PakarIcons.cloud,
            actions: [
              IconButton(
                onPressed: working || busy ? null : () => run(model.refresh),
                tooltip: 'Refresh package',
                icon: const Icon(PakarIcons.arrowClockwise),
              ),
            ],
          ),
          WorkspaceSection(
            title: 'This Android app',
            icon: PakarIcons.circuitry,
            children: [
              if (model.package.isNotEmpty) ...[
                Text(
                  '${model.package['version_name']} · build ${model.package['version_code']}',
                  style: Theme.of(context).textTheme.titleMedium,
                ),
                SelectableText(
                  '${model.package['package']}',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
                const SizedBox(height: 12),
              ],
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  WorkspaceBadge(
                    '${state['state'] ?? 'idle'}'.replaceAll('_', ' '),
                    active: state['state'] == 'verified',
                  ),
                  if (model.sharing)
                    const WorkspaceBadge('Sharing enabled', active: true),
                  if (model.feedState != 'Stopped')
                    WorkspaceBadge(model.feedState),
                ],
              ),
              const SizedBox(height: 12),
              SizedBox(
                height: 4,
                child: busy ? LinearProgressIndicator(value: fraction) : null,
              ),
              if (model.available != null) ...[
                const SizedBox(height: 12),
                Text(
                  'Build ${model.available!['version_code']} available · ${model.available!['source_kind']}',
                ),
                if (!busy &&
                    state['transfer_id'] == null &&
                    model.available!['source_kind'] != 'incoming')
                  action(
                    'Receive update',
                    PakarIcons.cloud,
                    model.downloadPending,
                  ),
              ],
              if (count is int && total is int)
                Text(
                  '$count / $total bytes',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              if ({
                'verified',
                'permission_required',
              }.contains(state['state'])) ...[
                const SizedBox(height: 12),
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    action(
                      'Install update',
                      PakarIcons.lightning,
                      model.updates.install,
                    ),
                    action(
                      'Discard package',
                      PakarIcons.warning,
                      model.updates.abort,
                    ),
                    if (state['install_permission'] == false)
                      action(
                        'Allow app installs',
                        PakarIcons.gear,
                        model.updates.permission,
                      ),
                  ],
                ),
              ],
              if (busy)
                Align(
                  alignment: Alignment.centerLeft,
                  child: TextButton(
                    onPressed: () => model.updates.cancel(),
                    child: const Text('Cancel transfer'),
                  ),
                ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  action(
                    'Import APK',
                    PakarIcons.floppyDisk,
                    model.updates.pickLocal,
                  ),
                  action(
                    'Export installed APK',
                    PakarIcons.floppyDisk,
                    model.updates.exportInstalled,
                  ),
                ],
              ),
              if (error.isNotEmpty ||
                  model.message.isNotEmpty ||
                  state['error'] != null)
                Padding(
                  padding: const EdgeInsets.only(top: 12),
                  child: Text(
                    error.isNotEmpty
                        ? error
                        : '${state['error'] ?? model.message}',
                    style: TextStyle(
                      color: Theme.of(context).colorScheme.error,
                    ),
                  ),
                ),
            ],
          ),
          WorkspaceSection(
            title: 'Nearby app instances',
            icon: PakarIcons.broadcast,
            trailing: IconButton(
              tooltip: 'Discover app instances',
              onPressed: model.discovery.scanning || working
                  ? null
                  : () => run(() => model.discovery.scan()),
              icon: const Icon(PakarIcons.arrowClockwise),
            ),
            children: [
              SizedBox(
                height: 3,
                child: model.discovery.scanning
                    ? const LinearProgressIndicator()
                    : null,
              ),
              for (final peer in model.discovery.peers)
                ListTile(
                  contentPadding: EdgeInsets.zero,
                  title: Text(
                    peer.name,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                  ),
                  subtitle: Text(
                    '${peer.origin}\n${peer.manifest['app_updates']?['version_name'] ?? ''}',
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                  ),
                  trailing: PopupMenuButton<String>(
                    enabled: !working && !busy,
                    onSelected: (value) => run(
                      () => value == 'pull'
                          ? model.updates.pullPeer(peer.origin)
                          : model.updates.pushPeer(peer.origin),
                    ),
                    itemBuilder: (_) => const [
                      PopupMenuItem(
                        value: 'pull',
                        child: Text('Receive newer app'),
                      ),
                      PopupMenuItem(
                        value: 'push',
                        child: Text('Send installed app'),
                      ),
                    ],
                  ),
                ),
              if (model.discovery.error.isNotEmpty)
                Text(
                  model.discovery.error,
                  style: TextStyle(color: Theme.of(context).colorScheme.error),
                ),
              const SizedBox(height: 12),
              field(peerAddress, 'App instance HTTP(S) origin'),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  action(
                    'Receive app',
                    PakarIcons.cloud,
                    () => model.updates.pullPeer(peerAddress.text),
                  ),
                  action(
                    'Send app',
                    PakarIcons.broadcast,
                    () => model.updates.pushPeer(peerAddress.text),
                  ),
                ],
              ),
            ],
          ),
          WorkspaceSection(
            title: 'Manual package source',
            icon: PakarIcons.cloud,
            children: [
              field(source, 'HTTP(S) APK URL'),
              field(digest, 'SHA-256'),
              field(size, 'APK size in bytes', number: true),
              action(
                'Download APK',
                PakarIcons.cloud,
                () => model.updates.download(
                  source.text,
                  sha256: digest.text.trim(),
                  bytes: int.tryParse(size.text) ?? 0,
                ),
              ),
              if (model.pending != null) ...[
                const SizedBox(height: 12),
                Text(
                  'Build ${model.pending!['version_code']} available',
                  style: Theme.of(context).textTheme.titleSmall,
                ),
                action(
                  'Download offered update',
                  PakarIcons.cloud,
                  model.downloadPending,
                ),
              ],
            ],
          ),
          WorkspaceSection(
            title: 'Distribution preferences',
            icon: PakarIcons.gear,
            children: [
              for (final item in [
                (
                  'notify_updates',
                  'Notify when a compatible update is available',
                ),
                ('sharing', 'Share installed app on this network'),
                ('discover', 'Discover newer app instances automatically'),
                ('auto_check', 'Check for newer apps automatically'),
                (
                  'auto_download',
                  'Download compatible newer apps automatically',
                ),
                (
                  'auto_install',
                  'Request Android installation after verified automatic download',
                ),
                (
                  'auto_cleanup',
                  'Clean abandoned private APK staging automatically',
                ),
                ('socket_io', 'Socket.IO feed'),
              ])
                SwitchListTile.adaptive(
                  contentPadding: EdgeInsets.zero,
                  title: Text(item.$2),
                  value: draft[item.$1] == true,
                  onChanged: working || busy
                      ? null
                      : (v) => setState(() {
                          draft[item.$1] = v;
                          if (item.$1 == 'auto_download' && !v) {
                            draft['auto_install'] = false;
                          }
                          if (item.$1 == 'auto_install' && v) {
                            draft['auto_download'] = true;
                          }
                        }),
                ),
              const SizedBox(height: 12),
              field(
                fields['check_interval_seconds']!,
                'Automatic check interval · seconds (60–86400)',
                number: true,
              ),
              const Text(
                'Automatic installation still requires Android consent. Cleanup preserves active transfers, installed apps, settings and user files.',
              ),
              for (final entry in [
                ('catalog', 'HTTP(S) update catalog'),
                ('origin', 'Trusted APK download origin'),
                ('feed', 'WSS app-update feed'),
                if (draft['socket_io'] == true)
                  ('namespace', 'Socket.IO namespace'),
                ('event', 'App update event'),
              ])
                field(fields[entry.$1]!, entry.$2),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  action('Save preferences', PakarIcons.floppyDisk, () async {
                    await savePreferences();
                  }),
                  action(
                    'Check for updates',
                    PakarIcons.arrowClockwise,
                    model.checkNow,
                  ),
                  action(
                    'Enable update notifications',
                    PakarIcons.gear,
                    model.updates.notificationPermission,
                  ),
                ],
              ),
            ],
          ),
          ...widget.extensions,
          WorkspaceSection(
            title: 'Advanced app management',
            icon: PakarIcons.gear,
            children: [
              Text(
                model.management.state,
                style: Theme.of(context).textTheme.bodySmall,
              ),
              if (model.management.clientId.isNotEmpty)
                SelectableText('Client ${model.management.clientId}'),
              const SizedBox(height: 12),
              field(fields['management_base']!, 'Managed HTTPS base'),
              for (final item in [
                (
                  'management_enabled',
                  'Enable enrolled app telemetry and management',
                ),
                ('allow_remote_check', 'Allow managed check-update requests'),
                (
                  'allow_remote_cleanup',
                  'Allow managed private staging cleanup',
                ),
              ])
                SwitchListTile.adaptive(
                  contentPadding: EdgeInsets.zero,
                  title: Text(item.$2),
                  value: draft[item.$1] == true,
                  onChanged: working || busy
                      ? null
                      : (value) => setState(() => draft[item.$1] = value),
                ),
              const Text(
                'Only allowlisted app/version/build/SDK/uptime/update-state telemetry is reported. Board credentials, device passwords and enrollment tokens are never included. Remote uninstall is not enabled.',
              ),
              const SizedBox(height: 12),
              TextField(
                controller: enrollment,
                enabled: !working && !busy,
                onChanged: (_) => setState(() {}),
                obscureText: true,
                autocorrect: false,
                enableSuggestions: false,
                decoration: const InputDecoration(
                  labelText: 'One-use enrollment token · secure storage only',
                ),
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  action(
                    'Save management policy',
                    PakarIcons.floppyDisk,
                    savePreferences,
                  ),
                  action('Enroll this app', PakarIcons.cloud, () async {
                    await savePreferences();
                    await model.management.enroll(
                      fields['management_base']!.text.trim(),
                      enrollment.text.trim(),
                      model.instanceId,
                    );
                    enrollment.clear();
                    model.changed();
                    await model.reconcile();
                  }, enabled: enrollment.text.isNotEmpty),
                  action(
                    'Report now',
                    PakarIcons.broadcast,
                    model.reportManagement,
                    enabled:
                        model.management.enrolled &&
                        model.settings['management_enabled'] == true,
                  ),
                  action('Forget enrollment', PakarIcons.warning, () async {
                    await model.management.forget();
                    model.changed();
                  }, enabled: model.management.enrolled),
                  action(
                    'Clean abandoned staging',
                    PakarIcons.arrowClockwise,
                    model.cleanup,
                  ),
                  action(
                    'Uninstall this app…',
                    PakarIcons.warning,
                    removeSelf,
                    enabled: state['transfer_id'] == null,
                  ),
                ],
              ),
              if (model.cleanupSummary.isNotEmpty)
                Text(
                  model.cleanupSummary,
                  style: Theme.of(context).textTheme.bodySmall,
                ),
            ],
          ),
        ],
      );
    },
  );
}
