import 'package:flutter/material.dart';

import 'api.dart';
import 'firmware_automation.dart';
import 'firmware_progress.dart';
import 'icons.dart';
import 'workspace_widgets.dart';

class FirmwareAutomationPage extends StatefulWidget {
  const FirmwareAutomationPage({
    super.key,
    required this.automation,
    required this.api,
  });
  final FirmwareAutomation automation;
  final PakarApi api;
  @override
  State<FirmwareAutomationPage> createState() => _FirmwareAutomationPageState();
}

class _FirmwareAutomationPageState extends State<FirmwareAutomationPage> {
  FirmwareAutomation get model => widget.automation;
  late final catalog = TextEditingController(text: model.policy.catalogUrl),
      origin = TextEditingController(text: model.policy.downloadOrigin),
      feed = TextEditingController(text: model.policy.feedEndpoint),
      namespace = TextEditingController(text: model.policy.namespace),
      event = TextEditingController(text: model.policy.event),
      interval = TextEditingController(text: '${model.policy.pollSeconds}'),
      token = TextEditingController(),
      bearer = TextEditingController();
  late bool enabled = model.policy.enabled,
      socketIo = model.policy.socketIo,
      autoStage = model.policy.autoStage,
      autoApply = model.policy.autoApply;
  late String board = model.policy.boardId,
      application = model.policy.applicationId;
  bool working = false;
  String error = '';
  @override
  void dispose() {
    for (final c in [
      catalog,
      origin,
      feed,
      namespace,
      event,
      interval,
      token,
      bearer,
    ]) {
      c.dispose();
    }
    super.dispose();
  }

  Future<void> run(Future<void> Function() action) async {
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

  Future<bool> confirm(String title, String message) async =>
      await showDialog<bool>(
        context: context,
        builder: (c) => AlertDialog(
          title: Text(title),
          content: Text(message),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(c, false),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(c, true),
              child: const Text('Confirm'),
            ),
          ],
        ),
      ) ??
      false;
  Future<void> save() async {
    final policy = FirmwareAutomationPolicy(
      enabled: enabled,
      boardId: board,
      applicationId: application,
      catalogUrl: catalog.text.trim(),
      downloadOrigin: origin.text.trim(),
      feedEndpoint: feed.text.trim(),
      socketIo: socketIo,
      namespace: namespace.text.trim(),
      event: event.text.trim(),
      autoStage: autoStage,
      autoApply: autoApply,
      pollSeconds: int.tryParse(interval.text) ?? 0,
    );
    policy.validate();
    if (enabled &&
        autoApply &&
        !await confirm(
          'Enable automatic board upgrades?',
          'Verified manifests from the configured source may upgrade ESP and AVR on $board, interrupt outputs and reboot the pair. Keep stable power.',
        )) {
      return;
    }
    if (!mounted) return;
    await model.configure(
      policy,
      feedToken: token.text,
      downloadBearer: bearer.text,
    );
    token.clear();
    bearer.clear();
  }

  Widget field(TextEditingController c, String label, {bool secret = false}) =>
      Padding(
        padding: const EdgeInsets.only(bottom: 12),
        child: TextField(
          controller: c,
          enabled: !working && !model.busy,
          obscureText: secret,
          autocorrect: false,
          enableSuggestions: false,
          decoration: InputDecoration(labelText: label),
        ),
      );
  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: model,
    builder: (context, _) {
      final peer = widget.api.latestStatus, app = peer?.peer.application;
      final selectable =
          widget.api.liveFresh &&
          peer?.json['board_id'] is String &&
          app != null;
      return WorkspaceBody(
        children: [
          const WorkspaceHeader(
            title: 'Automatic firmware updates',
            icon: PakarIcons.cpu,
          ),
          WorkspaceSection(
            title: 'Board assignment',
            icon: PakarIcons.circuitry,
            children: [
              if (board.isNotEmpty) SelectableText(board),
              if (application.isNotEmpty)
                Text(application, style: Theme.of(context).textTheme.bodySmall),
              Align(
                alignment: Alignment.centerLeft,
                child: OutlinedButton.icon(
                  onPressed: working || model.busy || !selectable
                      ? null
                      : () => setState(() {
                          board = peer!.json['board_id'];
                          application = app.id;
                        }),
                  icon: const Icon(PakarIcons.link),
                  label: const Text('Use connected board'),
                ),
              ),
              SwitchListTile.adaptive(
                contentPadding: EdgeInsets.zero,
                title: const Text('Enable automatic source checks'),
                value: enabled,
                onChanged: working || model.busy
                    ? null
                    : (v) => setState(() => enabled = v),
              ),
            ],
          ),
          WorkspaceSection(
            title: 'Firmware sources',
            icon: PakarIcons.cloud,
            children: [
              field(catalog, 'HTTP(S) offer catalog'),
              field(origin, 'Trusted HTTPS firmware origin'),
              field(feed, 'WSS firmware feed'),
              SwitchListTile.adaptive(
                contentPadding: EdgeInsets.zero,
                title: const Text('Socket.IO'),
                value: socketIo,
                onChanged: working || model.busy
                    ? null
                    : (v) => setState(() => socketIo = v),
              ),
              if (socketIo) field(namespace, 'Socket.IO namespace'),
              field(event, 'Firmware event'),
              field(interval, 'Catalog interval in seconds'),
              field(token, 'Feed bearer · this session', secret: true),
              field(bearer, 'Download bearer · this session', secret: true),
            ],
          ),
          WorkspaceSection(
            title: 'Upgrade policy',
            icon: PakarIcons.gear,
            children: [
              SwitchListTile.adaptive(
                contentPadding: EdgeInsets.zero,
                title: const Text('Stage verified offers automatically'),
                value: autoStage,
                onChanged: working || model.busy
                    ? null
                    : (v) => setState(() {
                        autoStage = v;
                        if (!v) autoApply = false;
                      }),
              ),
              SwitchListTile.adaptive(
                contentPadding: EdgeInsets.zero,
                title: const Text('Apply verified manifests automatically'),
                value: autoApply,
                onChanged: working || model.busy || !autoStage
                    ? null
                    : (v) => setState(() => autoApply = v),
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  FilledButton.icon(
                    onPressed: working || model.busy ? null : () => run(save),
                    icon: const Icon(PakarIcons.floppyDisk),
                    label: const Text('Save preferences'),
                  ),
                  OutlinedButton.icon(
                    onPressed: working || model.busy || !model.running
                        ? null
                        : () => run(model.checkNow),
                    icon: const Icon(PakarIcons.arrowClockwise),
                    label: const Text('Check now'),
                  ),
                ],
              ),
            ],
          ),
          WorkspaceSection(
            title: 'Update activity',
            icon: PakarIcons.clock,
            children: [
              WorkspaceBadge(model.state, active: model.running),
              if (model.progress != null) ...[
                const SizedBox(height: 12),
                FirmwareProgressView(model.progress!),
              ],
              if (model.pending case final offer?) ...[
                const SizedBox(height: 12),
                Text('${offer.target} · ${offer.kind}'),
                SelectableText(offer.hash),
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    if (offer.kind != 'binary' || offer.target != 'esp')
                      OutlinedButton(
                        onPressed: working || model.busy
                            ? null
                            : () => run(() => model.reviewPending()),
                        child: const Text('Stage offer'),
                      ),
                    if (offer.kind == 'manifest' || offer.target == 'esp')
                      OutlinedButton(
                        onPressed: working || model.busy
                            ? null
                            : () => run(() async {
                                if (await confirm(
                                  offer.kind == 'manifest'
                                      ? 'Apply this manifest?'
                                      : 'Upload this ESP image?',
                                  offer.kind == 'manifest'
                                      ? 'This upgrades the selected board targets and may reboot the pair.'
                                      : 'This uploads the verified image immediately and reboots the ESP. Keep stable power; a transfer ACK is not postboot verification.',
                                )) {
                                  await model.reviewPending(apply: true);
                                }
                              }),
                        child: Text(
                          offer.kind == 'manifest'
                              ? 'Apply offered manifest'
                              : 'Upload ESP image',
                        ),
                      ),
                  ],
                ),
              ],
              if (error.isNotEmpty || model.error.isNotEmpty)
                Padding(
                  padding: const EdgeInsets.only(top: 12),
                  child: Text(
                    error.isNotEmpty ? error : model.error,
                    style: TextStyle(
                      color: Theme.of(context).colorScheme.error,
                    ),
                  ),
                ),
            ],
          ),
        ],
      );
    },
  );
}
