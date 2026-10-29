import 'package:flutter/material.dart';

import 'firmware.dart';
import 'models.dart';
import 'workspace_widgets.dart';

// PCController hostui/update_progress.go's stage-local measured/unknown rule.
// A transferred file or reboot ACK is never a verified completed update.
class FirmwareProgress {
  const FirmwareProgress(
    this.source,
    this.phase,
    this.done,
    this.total, {
    this.active = true,
    this.verified = false,
    this.error = '',
  });
  final String source, phase, error;
  final int done;
  final int? total;
  final bool active, verified;
  double? get fraction =>
      total != null && total! > 0 && done >= 0 && done <= total!
      ? done / total!
      : null;
  String get label => '$source · ${phase.replaceAll('_', ' ')}';

  static FirmwareProgress? observed(DeviceStatus status) {
    final lifecycle = status.upgrade;
    if (lifecycle != null) {
      final source = switch (lifecycle.source) {
        'local-ota' => 'Board OTA',
        'avr-maintenance' => 'AVR update',
        _ => 'Board update',
      };
      return FirmwareProgress(
        '$source #${lifecycle.operation}',
        lifecycle.phase,
        lifecycle.received,
        lifecycle.measured ? lifecycle.expected : null,
        active: lifecycle.active,
        verified: lifecycle.verified,
        error: lifecycle.error,
      );
    }
    final a = status.json['avr_maintenance'];
    final ota = status.json['update'], remote = status.json['remote_firmware'];
    final otherActive =
        (remote is Map && remote['busy'] == true) ||
        (ota is Map &&
            {
              'receiving',
              'verifying',
              'ready-to-reboot',
            }.contains(ota['phase']));
    if (a is Map &&
        status.json['boot_id'] is String &&
        (status.json['boot_id'] as String).isNotEmpty &&
        (a['busy'] == true || !otherActive) &&
        a['boot_id'] == status.json['boot_id'] &&
        {'urboot', 'isp'}.contains(a['method']) &&
        a['job_id'] is int &&
        a['job_id'] > 0 &&
        a['busy'] is bool &&
        {
          'queued',
          'prepare',
          'backup_isp',
          'program_isp',
          'backup_flash',
          'backup_eeprom',
          'program_avr',
          'verify_flash',
          'verify_eeprom',
          'awaiting_application',
          'complete',
          'failed',
        }.contains(a['phase'])) {
      final done = a['received_bytes'], total = a['expected_bytes'];
      if (done is! int || total is! int || done < 0 || done > total) {
        return null;
      }
      return FirmwareProgress(
        'AVR · ${a['method'] == 'isp' ? 'ISP' : 'Urboot'} #${a['job_id']}',
        a['phase'],
        done,
        {
                  'backup_flash',
                  'backup_eeprom',
                  'program_avr',
                  'verify_flash',
                  'verify_eeprom',
                }.contains(a['phase']) &&
                total > 0
            ? total
            : null,
        active: a['busy'],
        verified:
            a['phase'] == 'complete' &&
            a['verified'] == true &&
            a['application_recovered'] == true,
        error: a['error'] is String ? a['error'] : '',
      );
    }
    final local = status.json['update'];
    if (local is Map &&
        {
          'receiving',
          'verifying',
          'ready-to-reboot',
          'failed',
          'aborted',
        }.contains(local['phase'])) {
      final done = local['received_bytes'], total = local['expected_bytes'];
      if (done is! int ||
          done < 0 ||
          total is! int ||
          total < 0 ||
          done > total && total > 0) {
        return null;
      }
      return FirmwareProgress(
        'Board OTA',
        local['phase'] as String,
        done,
        local['phase'] == 'receiving' &&
                local['progress_known'] == true &&
                total > 0
            ? total
            : null,
        active: !{'failed', 'aborted'}.contains(local['phase']),
        verified: local['completion_verified'] == true,
      );
    }
    final raw = status.json['remote_firmware'];
    if (raw is! Map<String, dynamic>) return null;
    try {
      final job = RemoteJob.parse(raw);
      if (job.id == 0 ||
          job.phase == 'idle' ||
          job.boot != status.json['boot_id']) {
        return null;
      }
      return FirmwareProgress(
        'Board update #${job.id}',
        job.phase,
        job.received,
        {'download_esp', 'download_avr'}.contains(job.phase) && job.expected > 0
            ? job.expected
            : null,
        active: job.busy,
        verified:
            job.phase == 'complete' &&
            !job.busy &&
            raw['completion_verified'] == true,
        error: job.error,
      );
    } on FormatException {
      return null;
    }
  }
}

class FirmwareProgressView extends StatelessWidget {
  const FirmwareProgressView(this.progress, {super.key});
  final FirmwareProgress progress;
  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context), colors = theme.colorScheme;
    final state = _ProgressPresentation(progress);
    final tone = state.failed
        ? colors.error
        : state.verified
        ? colors.secondary
        : colors.primary;
    final fraction = progress.fraction;
    final measured = fraction != null;
    final metric = measured
        ? '${(fraction * 100).floor()}%'
        : state.verified
        ? 'Verified'
        : state.failed
        ? 'Stopped'
        : state.waiting
        ? 'Pending'
        : 'In progress';

    // WorkspaceActionCard's semantic icon surface and reserved border, adapted
    // for a read-only live operation. No inferred timeline or elapsed-time ETA.
    return Semantics(
      container: true,
      liveRegion: true,
      label:
          '${progress.label}. ${state.status}. '
          '${measured ? '$metric of this stage' : metric}',
      child: Container(
        key: const ValueKey('firmware-progress-card'),
        decoration: BoxDecoration(
          color: colors.surfaceContainerLow,
          borderRadius: BorderRadius.circular(16),
          border: Border.all(color: tone.withValues(alpha: .32)),
        ),
        padding: const EdgeInsets.all(18),
        child: LayoutBuilder(
          builder: (context, bounds) {
            final compact =
                bounds.maxWidth < 420 ||
                MediaQuery.textScalerOf(context).scale(14) > 21;
            final badge = WorkspaceBadge(
              state.status,
              active: state.verified,
              icon: state.failed
                  ? Icons.error_outline_rounded
                  : state.verified
                  ? Icons.verified_outlined
                  : state.waiting
                  ? Icons.hourglass_top_rounded
                  : Icons.sensors_rounded,
            );
            final identity = Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Container(
                  padding: const EdgeInsets.all(11),
                  decoration: BoxDecoration(
                    color: tone.withValues(alpha: .12),
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: Icon(state.icon, color: tone, size: 25),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Text(
                        state.title,
                        style: theme.textTheme.titleMedium?.copyWith(
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                      const SizedBox(height: 3),
                      Text(
                        progress.label,
                        style: theme.textTheme.bodySmall?.copyWith(
                          color: colors.onSurfaceVariant,
                        ),
                      ),
                    ],
                  ),
                ),
                if (!compact) ...[
                  const SizedBox(width: 14),
                  Flexible(child: badge),
                ],
              ],
            );
            return Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              mainAxisSize: MainAxisSize.min,
              children: [
                identity,
                if (compact) ...[
                  const SizedBox(height: 12),
                  Align(
                    alignment: AlignmentDirectional.centerStart,
                    child: badge,
                  ),
                ],
                const SizedBox(height: 20),
                Wrap(
                  alignment: WrapAlignment.spaceBetween,
                  crossAxisAlignment: WrapCrossAlignment.center,
                  spacing: 16,
                  runSpacing: 6,
                  children: [
                    Text(
                      measured ? 'This stage' : state.detail,
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: colors.onSurfaceVariant,
                      ),
                    ),
                    Text(
                      metric,
                      key: const ValueKey('firmware-stage-metric'),
                      style: theme.textTheme.titleLarge?.copyWith(
                        color: tone,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ],
                ),
                if (state.running || measured) ...[
                  const SizedBox(height: 10),
                  LinearProgressIndicator(
                    value: state.running || measured ? fraction : null,
                    color: tone,
                    backgroundColor: tone.withValues(alpha: .12),
                    minHeight: 8,
                    borderRadius: BorderRadius.circular(8),
                    semanticsLabel: 'Current firmware stage',
                  ),
                ],
                if (measured) ...[
                  const SizedBox(height: 9),
                  Text(
                    '${progress.done} / ${progress.total} bytes · this stage',
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: colors.onSurfaceVariant,
                    ),
                  ),
                  const SizedBox(height: 7),
                  Text(
                    state.detail,
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: colors.onSurfaceVariant,
                    ),
                  ),
                ],
                if (progress.error.isNotEmpty) ...[
                  const SizedBox(height: 12),
                  Text(
                    progress.error,
                    style: theme.textTheme.bodyMedium?.copyWith(
                      color: colors.error,
                    ),
                  ),
                ],
              ],
            );
          },
        ),
      ),
    );
  }
}

/// Only the current observation is presented; a byte transfer is not a flash
/// verification, and an inactive/complete observation alone is not success.
class _ProgressPresentation {
  _ProgressPresentation(this.progress);
  final FirmwareProgress progress;
  String get phase => progress.phase.replaceAll('-', '_').replaceAll(' ', '_');
  bool get failed =>
      progress.error.isNotEmpty ||
      {'failed', 'aborted', 'cancelled'}.contains(phase);
  bool get verified => !failed && !progress.active && progress.verified;
  bool get waiting =>
      !failed &&
      !verified &&
      (!progress.active ||
          {
            'ready_to_reboot',
            'rebooting',
            'reconnecting',
            'awaiting_application',
            'awaiting_readback',
            'complete',
          }.contains(phase));
  bool get running => progress.active && !failed && !verified;
  bool get checking => phase.contains('verif');
  bool get backup => phase.contains('backup');
  bool get download => phase.contains('download') || phase == 'receiving';
  bool get uploading => phase.contains('upload');
  bool get programming => phase.contains('program');
  String get status => failed
      ? 'Update stopped'
      : verified
      ? 'Update verified'
      : waiting
      ? 'Awaiting confirmation'
      : checking
      ? 'Verifying'
      : 'Live progress';
  String get title => failed
      ? 'Update interrupted'
      : verified
      ? 'Firmware ready'
      : waiting
      ? 'Waiting for board'
      : checking
      ? 'Verifying firmware'
      : backup
      ? 'Saving a recovery copy'
      : download
      ? 'Receiving firmware'
      : uploading
      ? 'Sending firmware'
      : programming
      ? 'Writing firmware'
      : 'Preparing update';
  IconData get icon => failed
      ? Icons.error_outline_rounded
      : verified
      ? Icons.verified_outlined
      : waiting
      ? Icons.restart_alt_rounded
      : checking
      ? Icons.fact_check_outlined
      : backup
      ? Icons.save_alt_rounded
      : download
      ? Icons.download_rounded
      : uploading
      ? Icons.upload_rounded
      : programming
      ? Icons.system_update_alt_rounded
      : Icons.memory_rounded;
  String get detail => failed
      ? 'Review the reported error before retrying.'
      : verified
      ? 'Board verification received.'
      : waiting
      ? 'Waiting for the board to confirm its running firmware.'
      : checking
      ? 'Checking stage integrity.'
      : backup
      ? 'Recovery data · current stage only'
      : progress.fraction == 1
      ? 'Transfer finished; board verification pending.'
      : download || uploading || programming
      ? 'Transferred bytes · current stage only'
      : 'Waiting for the next board progress event.';
}
