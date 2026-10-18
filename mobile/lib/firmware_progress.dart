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
      total != null && total! > 0 && done <= total! ? done / total! : null;
  String get label => '$source · ${phase.replaceAll('_', ' ')}';

  static FirmwareProgress? observed(DeviceStatus status) {
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
        verified: job.phase == 'complete',
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
