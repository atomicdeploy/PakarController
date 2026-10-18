import 'dart:async';

import 'package:crypto/crypto.dart' as crypto;

import 'package:flutter/material.dart';

import 'api.dart';
import 'documents.dart';
import 'firmware.dart';
import 'firmware_progress.dart';
import 'firmware_feed.dart';
import 'icons.dart';
import 'workspace_widgets.dart';

class FirmwarePage extends StatefulWidget {
  const FirmwarePage({
    super.key,
    required this.api,
    required this.authorized,
    required this.busyChanged,
    this.documents,
  });
  final PakarApi api;
  final bool Function(String) authorized;
  final void Function(bool) busyChanged;
  final AndroidDocuments? documents;
  @override
  State<FirmwarePage> createState() => _FirmwarePageState();
}

class _FirmwarePageState extends State<FirmwarePage>
    with WidgetsBindingObserver {
  late FirmwareController controller = FirmwareController(
    widget.api,
    authorized: (feature) => widget.authorized(feature),
  );
  late final documents = widget.documents ?? AndroidDocuments();
  final url = TextEditingController(),
      hash = TextEditingController(),
      bearer = TextEditingController();
  final fileUrl = TextEditingController(), fileHash = TextEditingController();
  StreamSubscription<dynamic>? statusEvents;
  FirmwareProgress? boardProgress, transferProgress;
  String? observedBoard;
  bool fileAvr = false;
  final feed = FirmwareFeed();
  final feedUrl = TextEditingController(),
      feedOrigin = TextEditingController(),
      feedToken = TextEditingController(),
      feedNamespace = TextEditingController(text: '/'),
      feedEvent = TextEditingController(text: 'firmware-offer');
  bool socketIo = false, armed = false;
  FirmwareFeedOptions get feedOptions => FirmwareFeedOptions(
    endpoint: feedUrl.text,
    origin: feedOrigin.text,
    token: feedToken.text,
    namespace: feedNamespace.text,
    event: feedEvent.text,
    socketIo: socketIo,
  );
  RemoteJob? job;
  Timer? timer;
  String message = '', staged = '';
  String stagedDigest = '';
  bool working = false,
      executing = false,
      loading = false,
      uncertain = false,
      downgrade = false;
  int? targets;
  int readEpoch = 0;
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    feed.addListener(feedChanged);
    observedBoard = widget.api.latestStatus?.json['board_id'] as String?;
    statusEvents = widget.api.statusUpdates.listen((status) {
      if (!mounted || status.json['board_id'] != observedBoard) return;
      if (feed.running &&
          (status.json['boot_id'] != widget.api.latestStatus?.json['boot_id'] ||
              controller.owner != widget.api.connectionRevision)) {
        unawaited(feed.stop());
      }
      if (!working && controller.owner != widget.api.connectionRevision) {
        controller.dispose();
        controller = FirmwareController(
          widget.api,
          authorized: widget.authorized,
        );
      }
      setState(() {
        boardProgress = FirmwareProgress.observed(status);
        final raw = status.json['remote_firmware'];
        if (raw is Map<String, dynamic>) {
          try {
            job = RemoteJob.parse(raw);
          } on FormatException {
            job = null;
          }
        }
      });
    });
    final initial = widget.api.latestStatus;
    if (initial != null) boardProgress = FirmwareProgress.observed(initial);
    unawaited(refresh());
    timer = Timer.periodic(const Duration(seconds: 3), (_) {
      if (feed.running && controller.owner != widget.api.connectionRevision) {
        unawaited(feed.stop());
      }
      if (!widget.api.liveFresh) unawaited(refresh());
    });
  }

  @override
  void dispose() {
    timer?.cancel();
    WidgetsBinding.instance.removeObserver(this);
    feed.removeListener(feedChanged);
    feed.dispose();
    for (final text in [
      feedUrl,
      feedOrigin,
      feedToken,
      feedNamespace,
      feedEvent,
    ]) {
      text.dispose();
    }
    unawaited(statusEvents?.cancel());
    ++readEpoch;
    controller.dispose();
    url.dispose();
    hash.dispose();
    bearer.dispose();
    fileUrl.dispose();
    fileHash.dispose();
    super.dispose();
  }

  Future<void> refresh({bool explicit = false}) async {
    if (!mounted || working || loading) return;
    final epoch = ++readEpoch;
    loading = true;
    try {
      final observed = await controller.status();
      if (!mounted || epoch != readEpoch) return;
      setState(() {
        job = observed;
        if (explicit) {
          uncertain = false;
          message = 'Observed current job. Prior commands were not replayed.';
        }
      });
    } catch (error) {
      if (mounted && epoch == readEpoch) {
        setState(() {
          job = null;
          message = '$error';
        });
      }
    } finally {
      loading = false;
    }
  }

  Future<bool> confirm(String title, String detail) async =>
      await showDialog<bool>(
        context: context,
        builder: (context) => AlertDialog(
          title: Text(title),
          content: SingleChildScrollView(child: Text(detail)),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(context, true),
              child: const Text('Confirm'),
            ),
          ],
        ),
      ) ??
      false;
  Future<void> run(
    String title,
    String warning,
    Future<String> Function() action, {
    bool mutation = true,
    bool requireConfirmation = true,
  }) async {
    if (!mounted || working || (mutation && uncertain)) return;
    ++readEpoch;
    setState(() {
      working = true;
      message = '';
    });
    widget.busyChanged(true);
    try {
      if ((requireConfirmation && !await confirm(title, warning)) || !mounted) {
        return;
      }
      setState(() => executing = true);
      final result = await action();
      if (mounted) setState(() => message = result);
    } catch (error) {
      if (mounted) {
        setState(() {
          uncertain = mutation;
          message =
              '$error No operation was replayed. Inspect with Refresh before deciding on another action.';
        });
      }
    } finally {
      if (mounted) {
        setState(() {
          working = false;
          executing = false;
          transferProgress = null;
        });
        widget.busyChanged(false);
      }
    }
  }

  Future<void> stageRemote() async {
    final target = targets,
        source = url.text,
        digest = hash.text,
        token = bearer.text,
        allow = downgrade;
    try {
      if (target == null) {
        throw const FormatException('Choose acknowledged board targets');
      }
      validateRemote(source, digest, target, token);
      controller.checkTargets(target);
    } catch (error) {
      setState(() => message = '$error');
      return;
    }
    await run(
      'Download and stage?',
      'Download the SHA-256-pinned manifest over HTTPS for ${target == 3
          ? 'both boards'
          : target == 1
          ? 'ESP'
          : 'AVR'}. This does NOT flash either board. ${allow ? 'Downgrades explicitly allowed.' : 'Downgrades are not allowed.'}',
      () async {
        final ack = await controller.stage(
          source,
          digest,
          target,
          token,
          allowDowngrade: allow,
        );
        if (mounted) {
          job = ack;
          bearer.clear();
          staged = '';
        }
        return 'Download accepted as job ${ack.id}; wait for the actual ready state before applying.';
      },
    );
  }

  Future<void> jobAction(bool apply) async {
    final captured = job;
    if (captured == null) return;
    await run(
      apply ? 'Apply exact staged job?' : 'Cancel exact download?',
      'Job ${captured.id} · boot ${captured.boot}\nSHA-256 ${captured.hash}\n${apply ? 'This can replace firmware, reset both boards and interrupt networking. Device-side backups, EEPROM preservation, pair verification and the durable Prog gate apply. Keep stable power. Completion requires post-reboot readback, not this acknowledgment.' : 'Cancel only this download/staged job, not an active programming/recovery sequence. No previous image is applied.'}',
      () async {
        final ack = await controller.act(captured, apply: apply);
        if (mounted) job = ack;
        return apply
            ? 'Exact job accepted for preparation. Follow observed progress; acceptance is not completion.'
            : 'Exact job cancellation confirmed.';
      },
    );
  }

  Future<void> manual(String action) async {
    final esp = action == 'update';
    final warning = switch (action) {
      'probe' => 'ISP signature probe resets/interrupts AVR. It is sent once, even though its HTTP method is GET.',
      'backup' => 'Read full AVR flash (32768 bytes, including Urboot) and EEPROM (1024 bytes) into ESP storage. This replaces the device’s previous backups. Keep stable power.',
      'stage' => 'Choose a complete 32768-byte raw AVR .bin including the intended bootloader. This only replaces the shared staging file; it does not program AVR. Filename/size are not authenticity checks.',
      'program' => 'Program the CURRENT shared AVR staging image. Its hash is not advertised by this legacy route and another client can replace it. Only continue if you independently know its provenance. Prefer the pinned remote pair workflow. This resets AVR and preserves/verifies EEPROM; keep stable power.',
      'update' => 'Choose an uncompressed ESP application E9 binary for this exact board/profile. Direct OTA has no pinned manifest authenticity or paired-version assurance; prefer the remote pair workflow. ESP will reboot and disconnect. Keep stable power.',
      _ => '',
    };
    await run(
      esp
          ? 'Manual ESP update?'
          : 'AVR ${action == 'stage' ? 'image import' : action}?',
      warning,
      () async {
        controller.check(esp ? 'ota' : 'isp');
        FirmwareDocument? document;
        if (action == 'stage' || esp) {
          document = await documents.pick(maximum: esp ? 2097152 : 32768);
          controller.check(esp ? 'ota' : 'isp');
          if (document == null) {
            return 'File selection cancelled; no device operation sent.';
          }
          validateFirmwareImage(esp, document.bytes);
          if (!mounted ||
              !await confirm(
                'Use ${document.name}?',
                '${document.bytes.length} bytes. ${esp ? 'Apply this ESP firmware now?' : 'Store this full AVR image without programming?'} Filename and format do not verify origin.',
              )) {
            return 'Image not accepted; no upload sent.';
          }
          controller.check(esp ? 'ota' : 'isp');
        }
        final result = await controller.manual(
          action,
          bytes: document?.bytes,
          progress: onTransfer,
        );
        if (mounted) setState(() => transferProgress = null);
        if (mounted && action == 'stage') {
          staged = document!.name;
          stagedDigest = crypto.sha256.convert(document.bytes).toString();
        }
        return result;
      },
    );
  }

  Future<void> avrMaintenance() => run('AVR maintenance', '', () async {
    final current = await controller.avrMaintenance();
    if (!mounted || current.busy || current.methods.isEmpty) {
      throw const FormatException(
        'AVR maintenance is busy or no programming path is acknowledged',
      );
    }
    final digest = TextEditingController(text: stagedDigest);
    var method = current.methods.first, operation = 'program';
    final selected = await showDialog<(String, String, String)>(
      context: context,
      builder: (dialog) => StatefulBuilder(
        builder: (dialog, update) => AlertDialog(
          title: const Text('AVR maintenance'),
          content: SizedBox(
            width: 440,
            child: SingleChildScrollView(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  DropdownButtonFormField<String>(
                    initialValue: method,
                    decoration: const InputDecoration(
                      labelText: 'Acknowledged programming path',
                    ),
                    items: [
                      for (final path in current.methods)
                        DropdownMenuItem(
                          value: path,
                          child: Text(
                            path == 'isp' ? 'ISP' : 'Urboot · hardware UART',
                          ),
                        ),
                    ],
                    onChanged: (v) => update(() => method = v!),
                  ),
                  const SizedBox(height: 12),
                  DropdownButtonFormField<String>(
                    initialValue: operation,
                    decoration: const InputDecoration(labelText: 'Operation'),
                    items: [
                      for (final op in ['program', 'backup', 'probe'])
                        DropdownMenuItem(value: op, child: Text(op)),
                    ],
                    onChanged: (v) => update(() => operation = v!),
                  ),
                  if (operation == 'program') ...[
                    const SizedBox(height: 12),
                    TextField(
                      controller: digest,
                      autocorrect: false,
                      decoration: const InputDecoration(
                        labelText: 'Exact staged image SHA-256',
                      ),
                      onChanged: (_) => update(() {}),
                    ),
                  ],
                  const SizedBox(height: 12),
                  const Text(
                    'This interrupts AVR. Programming requires the full flash image, including its intended bootloader. Keep stable power.',
                  ),
                ],
              ),
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(dialog),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed:
                  operation == 'program' && !firmwareHash(digest.text.trim())
                  ? null
                  : () => Navigator.pop(dialog, (
                      method,
                      operation,
                      digest.text.trim(),
                    )),
              child: const Text('Start maintenance'),
            ),
          ],
        ),
      ),
    );
    await Future<void>.delayed(const Duration(milliseconds: 250));
    digest.dispose();
    if (selected == null || !mounted) return 'Maintenance cancelled';
    final job = await controller.maintainAvr(
      method: selected.$1,
      operation: selected.$2,
      sha256: selected.$2 == 'program' ? selected.$3 : '',
    );
    return 'AVR ${job.method} job ${job.id} queued; follow board progress for verified completion.';
  }, requireConfirmation: false);

  void onTransfer(String stage, int done, int? total) {
    if (!mounted) return;
    final next = FirmwareProgress('Application', stage, done, total);
    if (transferProgress?.phase == stage &&
        transferProgress?.fraction != null &&
        next.fraction != null &&
        (transferProgress!.fraction! * 100).floor() ==
            (next.fraction! * 100).floor()) {
      return;
    }
    setState(() => transferProgress = next);
  }

  void feedChanged() {
    if (mounted) setState(() {});
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state != AppLifecycleState.resumed) unawaited(feed.stop());
  }

  Future<void> startFeed() async {
    final status = widget.api.latestStatus;
    if (status == null ||
        status.json['board_id'] is! String ||
        status.json['boot_id'] is! String) {
      return;
    }
    if (armed &&
        !await confirm(
          'Arm remote firmware delivery?',
          'Fresh offers from this endpoint and trusted download origin may upload ESP firmware and reboot this board. Manifest/AVR offers stage only. The listener stops on backgrounding or peer change.',
        )) {
      return;
    }
    if (!mounted) return;
    try {
      await feed.start(
        feedOptions,
        status.json['board_id'],
        status.json['boot_id'],
        armed: armed,
        deliver: (offer) => deliverOffer(offer, unattended: true),
      );
    } catch (_) {
      if (mounted) {
        setState(
          () => message =
              'Firmware listener connection failed; no operation replayed.',
        );
      }
    }
  }

  Future<void> deliverOffer(
    FirmwareOffer offer, {
    bool unattended = false,
  }) async {
    if (working || uncertain) {
      throw const FormatException(
        'Maintenance is already owned; offer discarded',
      );
    }
    final options = feedOptions, session = feed.session;
    void admit() {
      final s = widget.api.latestStatus;
      if (!mounted || !feed.running || s == null) throw const StaleConnection();
      FirmwareOffer.parse(
        offer.json,
        options,
        session,
        s.json['board_id'],
        s.json['boot_id'],
      );
    }

    admit();
    await run(
      'Deliver firmware offer?',
      '${offer.id} · ${offer.target} · ${offer.kind}\nSHA-256 ${offer.hash}\nBinary ESP delivery reboots the board. Manifest/AVR delivery stages only.',
      () async {
        admit();
        if (offer.kind == 'manifest') {
          final target = offer.target == 'all'
              ? 3
              : offer.target == 'esp'
              ? 1
              : 2;
          final ack = await controller.stage(offer.url, offer.hash, target, '');
          if (mounted) setState(() => job = ack);
          return 'Manifest accepted for stage-only download; Apply remains explicit.';
        }
        return controller.fileUrl(
          offer.url,
          offer.hash,
          offer.target == 'esp',
          progress: onTransfer,
          beforeUpload: admit,
        );
      },
      requireConfirmation: !unattended,
    );
  }

  Future<void> uploadUrl() async {
    final esp = controller.allowed('ota') && !fileAvr,
        source = fileUrl.text,
        digest = fileHash.text,
        token = bearer.text;
    await run(
      'Download and upload firmware?',
      '${esp ? 'ESP will reboot after upload.' : 'The complete AVR image is staged, not programmed.'} The file must match its SHA-256. No redirects or automatic retries.',
      () async {
        final result = await controller.fileUrl(
          source,
          digest,
          esp,
          bearer: token,
          progress: onTransfer,
        );
        if (mounted) {
          setState(() {
            transferProgress = null;
            bearer.clear();
          });
        }
        return result;
      },
    );
  }

  Future<void> export(bool eeprom) =>
      run('Export stored ${eeprom ? 'EEPROM' : 'flash'} backup?', 'Download the existing ESP-stored backup, validate its exact full size, then choose a NEW Android document. This does not take a fresh backup or verify it represents the current running AVR. No existing document is overwritten.', () async {
        final bytes = await controller.backupBytes(eeprom);
        final saved = await documents.export(
          'avr-${eeprom ? 'eeprom' : 'flash'}.bin',
          bytes,
          stillOwned: () => mounted && controller.allowed('isp'),
        );
        controller.check('isp');
        return saved
            ? '${bytes.length}-byte stored backup exported to the new document.'
            : 'Export cancelled; no document written.';
      }, mutation: false);
  final expandedSections = <String>{};
  Widget card(String title, List<Widget> children) {
    final collapsible =
        title == 'Remote firmware subscription' || title == 'Firmware file URL';
    final expanded =
        !collapsible ||
        expandedSections.contains(title) ||
        (title == 'Remote firmware subscription' && feed.running);
    return WorkspaceSection(
      title: title,
      icon: title.startsWith('Remote')
          ? PakarIcons.cloud
          : title.startsWith('Observed')
          ? PakarIcons.clock
          : PakarIcons.cpu,
      trailing: collapsible
          ? IconButton(
              tooltip: expanded ? 'Collapse' : 'Expand',
              icon: Transform.rotate(
                angle: expanded ? 3.14159265 : 0,
                child: const Icon(PakarIcons.caretDown),
              ),
              onPressed: () => setState(() {
                if (expanded) {
                  expandedSections.remove(title);
                } else {
                  expandedSections.add(title);
                }
              }),
            )
          : null,
      children: expanded ? children : const [],
    );
  }

  Widget button(
    String text,
    IconData icon,
    VoidCallback action, {
    bool enabled = true,
  }) => OutlinedButton.icon(
    onPressed: working || uncertain || !enabled ? null : action,
    icon: Icon(icon),
    label: Text(text),
  );
  @override
  Widget build(BuildContext context) {
    final isp = controller.allowed('isp'), ota = controller.allowed('ota');
    if (!isp && !ota) return const SizedBox.shrink();
    final observed = job;
    final available = [if (ota) 1, if (isp) 2, if (ota && isp) 3];
    if (!available.contains(targets)) targets = null;
    final free =
        observed != null && !observed.busy && observed.phase != 'ready';
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        const WorkspaceHeader(
          title: 'Firmware',
          icon: PakarIcons.cpu,
          subtitle: 'Updates and recovery',
        ),
        if (transferProgress != null || boardProgress != null)
          card('Update progress', [
            FirmwareProgressView((transferProgress ?? boardProgress)!),
          ]),
        card('Remote firmware subscription', [
          for (final entry in [
            (feedUrl, 'WSS endpoint'),
            (feedOrigin, 'Trusted HTTPS download origin'),
            (feedEvent, 'Firmware event'),
            if (socketIo) (feedNamespace, 'Namespace'),
            (feedToken, 'Feed bearer · memory only'),
          ]) ...[
            TextField(
              controller: entry.$1,
              enabled: !working && !feed.running,
              obscureText: entry.$1 == feedToken,
              autocorrect: false,
              decoration: InputDecoration(labelText: entry.$2),
            ),
            const SizedBox(height: 12),
          ],
          SwitchListTile.adaptive(
            contentPadding: EdgeInsets.zero,
            title: const Text('Socket.IO'),
            value: socketIo,
            onChanged: working || feed.running
                ? null
                : (v) => setState(() => socketIo = v),
          ),
          SwitchListTile.adaptive(
            contentPadding: EdgeInsets.zero,
            title: const Text('Arm delivery for this board session'),
            value: armed,
            onChanged: working || feed.running
                ? null
                : (v) => setState(() => armed = v),
          ),
          Text(feed.state),
          if (feed.running) SelectableText('Session ${feed.session}'),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              OutlinedButton(
                onPressed: working
                    ? null
                    : feed.running
                    ? () => feed.stop()
                    : startFeed,
                child: Text(feed.running ? 'Stop listener' : 'Start listener'),
              ),
              if (feed.pending case final offer?)
                OutlinedButton(
                  onPressed: working
                      ? null
                      : () {
                          feed.pending = null;
                          unawaited(
                            deliverOffer(offer).catchError((Object _) {
                              if (mounted) {
                                setState(
                                  () => message = 'Offer expired or owner changed; nothing replayed.',
                                );
                              }
                            }),
                          );
                        },
                  child: Text('Review offer ${offer.id}'),
                ),
            ],
          ),
        ]),
        if (widget.api.supportsBinaryFirmware)
          card('Firmware file URL', [
            TextField(
              controller: fileUrl,
              enabled: !working,
              autocorrect: false,
              decoration: const InputDecoration(
                labelText: 'HTTP(S) binary URL',
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: fileHash,
              enabled: !working,
              autocorrect: false,
              decoration: const InputDecoration(labelText: 'File SHA-256'),
            ),
            if (isp && ota)
              SwitchListTile.adaptive(
                contentPadding: EdgeInsets.zero,
                title: const Text('AVR image · stage only'),
                value: fileAvr,
                onChanged: working ? null : (v) => setState(() => fileAvr = v),
              ),
            const SizedBox(height: 12),
            button(
              'Download and upload',
              PakarIcons.cloud,
              uploadUrl,
              enabled: free,
            ),
          ]),
        card('Remote firmware · pinned manifest', [
          const Text(
            'HTTPS download → verified staging → explicit apply → post-boot pair verification. No automatic replay.',
          ),
          const SizedBox(height: 16),
          for (final entry in [
            (url, 'Manifest HTTPS URL'),
            (hash, 'Manifest SHA-256 · lowercase'),
            (bearer, 'Optional download bearer'),
          ]) ...[
            TextField(
              controller: entry.$1,
              enabled: !working && !uncertain && free,
              obscureText: entry.$1 == bearer,
              autocorrect: false,
              enableSuggestions: false,
              decoration: InputDecoration(
                labelText: entry.$2,
                border: const OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 12),
          ],
          DropdownButtonFormField<int>(
            key: ValueKey(targets),
            initialValue: targets,
            isExpanded: true,
            icon: const Icon(PakarIcons.caretDown),
            decoration: const InputDecoration(
              labelText: 'Acknowledged update targets',
            ),
            items: [
              for (final t in available)
                DropdownMenuItem(
                  value: t,
                  child: Text(
                    t == 3
                        ? 'Both boards · coordinated'
                        : t == 1
                        ? 'ESP only'
                        : 'AVR only',
                  ),
                ),
            ],
            onChanged: working || uncertain || !free
                ? null
                : (value) => setState(() => targets = value),
          ),
          SwitchListTile.adaptive(
            contentPadding: EdgeInsets.zero,
            title: const Text('Explicitly allow firmware downgrade'),
            value: downgrade,
            onChanged: working || uncertain || !free
                ? null
                : (v) => setState(() => downgrade = v),
          ),
          button(
            'Download and stage',
            PakarIcons.cloud,
            stageRemote,
            enabled: free,
          ),
        ]),
        card('Observed job', [
          Row(
            children: [
              Expanded(child: Text(observed?.phase ?? 'Job state unavailable')),
              IconButton(
                tooltip: 'Refresh and inspect current state',
                onPressed: working ? null : () => refresh(explicit: true),
                icon: const Icon(PakarIcons.arrowClockwise),
              ),
            ],
          ),
          if (observed != null) ...[
            SelectableText('Job ${observed.id} · boot ${observed.boot}'),
            if (observed.id > 0) SelectableText('Manifest ${observed.hash}'),
            Text(
              'Targets: ${observed.targets == 3
                  ? 'ESP + AVR'
                  : observed.targets == 1
                  ? 'ESP'
                  : 'AVR'}',
            ),
            if (observed.expected > 0) ...[
              const SizedBox(height: 8),
              LinearProgressIndicator(
                value: observed.received / observed.expected,
              ),
              Text(
                '${observed.received} / ${observed.expected} received bytes · not flash percentage',
              ),
            ],
            if (observed.error.isNotEmpty)
              Text(
                observed.error,
                style: TextStyle(color: Theme.of(context).colorScheme.error),
              ),
            if (observed.phase == 'complete')
              const Text(
                'Device reports pair completion; physical peripheral acceptance is separate.',
              ),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                button(
                  'Apply exact job',
                  PakarIcons.lightning,
                  () => jobAction(true),
                  enabled:
                      observed.canApply &&
                      controller.allowedTargets(observed.targets),
                ),
                button(
                  'Cancel exact job',
                  PakarIcons.warning,
                  () => jobAction(false),
                  enabled:
                      observed.canCancel &&
                      controller.allowedTargets(observed.targets),
                ),
              ],
            ),
          ],
          if (uncertain)
            const Text(
              'Outcome needs inspection. Only explicit Refresh unlocks a new decision; no retry is queued.',
            ),
        ]),
        if (isp && widget.api.supportsBinaryFirmware)
          card('AVR ISP · complete flash and EEPROM', [
            const Text(
              'These maintenance actions reset AVR. Exported backups include all flash/Urboot or all EEPROM, not partial application data.',
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                button(
                  'Probe ISP',
                  PakarIcons.cpu,
                  () => manual('probe'),
                  enabled: free,
                ),
                button(
                  'Create full backups',
                  PakarIcons.floppyDisk,
                  () => manual('backup'),
                  enabled: free,
                ),
                button(
                  'Export flash',
                  PakarIcons.floppyDisk,
                  () => export(false),
                  enabled: observed != null && !observed.busy,
                ),
                button(
                  'Export EEPROM',
                  PakarIcons.floppyDisk,
                  () => export(true),
                  enabled: observed != null && !observed.busy,
                ),
                button(
                  'Import full AVR image',
                  PakarIcons.cpu,
                  () => manual('stage'),
                  enabled: free,
                ),
                button(
                  'ISP / Urboot maintenance',
                  PakarIcons.warning,
                  avrMaintenance,
                  enabled: free,
                ),
              ],
            ),
            if (staged.isNotEmpty) Text('Last local stage: $staged'),
            if (stagedDigest.isNotEmpty) SelectableText(stagedDigest),
          ]),
        if (ota && widget.api.supportsBinaryFirmware)
          card('Manual ESP OTA', [
            const Text(
              'Import a complete application binary for this ESP. The device restarts after updating.',
            ),
            const SizedBox(height: 12),
            button(
              'Export running ESP firmware',
              PakarIcons.floppyDisk,
              () => run('Download running ESP firmware?', 'Read the running application image, not full flash or private settings. Choose a new Android document. This is not an authenticated release manifest.', () async {
                final bytes = await controller.espBytes();
                final saved = await documents.export(
                  'esp-firmware.bin',
                  bytes,
                  stillOwned: () => mounted && controller.allowed('ota'),
                );
                return saved
                    ? '${bytes.length} running firmware bytes exported.'
                    : 'Export cancelled.';
              }, mutation: false),
              enabled: free,
            ),
            const SizedBox(height: 12),
            button(
              'Select and update ESP',
              PakarIcons.warning,
              () => manual('update'),
              enabled: free,
            ),
          ]),
        if (executing) const LinearProgressIndicator(),
        if (message.isNotEmpty)
          Padding(
            padding: const EdgeInsets.all(12),
            child: Semantics(liveRegion: true, child: Text(message)),
          ),
      ],
    );
  }
}
