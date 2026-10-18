import 'package:flutter/material.dart';

import 'firmware.dart';
import 'models.dart';

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
  Widget build(BuildContext context) => Semantics(
    liveRegion: true,
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(progress.label, style: Theme.of(context).textTheme.bodySmall),
        if (progress.active) ...[
          const SizedBox(height: 6),
          LinearProgressIndicator(value: progress.fraction),
        ],
        if (progress.total != null)
          Text(
            '${progress.done} / ${progress.total} bytes · this stage',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        if (progress.error.isNotEmpty)
          Text(
            progress.error,
            style: TextStyle(color: Theme.of(context).colorScheme.error),
          ),
      ],
    ),
  );
}
