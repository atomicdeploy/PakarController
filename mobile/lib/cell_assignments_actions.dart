import 'package:flutter/material.dart';

import 'cell_assignments.dart';
import 'documents.dart';
import 'icons.dart';
import 'models.dart';

/// Pealayer ServerFilePicker's explicit select/preview/apply ownership contract
/// adapted to native SAF. Picking or cancelling never mutates a board.
class CellAssignmentsActions extends StatefulWidget {
  const CellAssignmentsActions({
    super.key,
    required this.status,
    required this.connected,
    required this.command,
    required this.readStatus,
    this.documents,
    this.ownerCurrent,
    this.seenFeatures = const {},
  });
  final DeviceStatus status;
  final bool connected;
  final Future<bool> Function(String, Map<String, String>) command;
  final Future<DeviceStatus> Function() readStatus;
  final AndroidDocuments? documents;
  final bool Function()? ownerCurrent;

  /// Retained presentation only; never grants command/export authority.
  final Set<String> seenFeatures;
  @override
  State<CellAssignmentsActions> createState() => _CellAssignmentsActionsState();
}

class _CellAssignmentsActionsState extends State<CellAssignmentsActions> {
  bool busy = false, previewing = false;
  String? message;
  AndroidDocuments get documents => widget.documents ?? AndroidDocuments();
  bool get ready =>
      widget.connected && widget.status.peer.tableSelection && !busy;
  bool get assignmentReady =>
      ready && widget.status.peer.has('cell-assignments');

  bool sameOwner(DeviceStatus initial) =>
      mounted &&
      (widget.ownerCurrent?.call() ?? widget.connected) &&
      widget.status.json['board_id'] == initial.json['board_id'] &&
      widget.status.json['boot_id'] == initial.json['boot_id'] &&
      widget.status.peer.tableSelection;

  Future<void> run(Future<void> Function(DeviceStatus) operation) async {
    if (!ready) return;
    final owner = widget.status;
    setState(() {
      busy = true;
      message = null;
    });
    try {
      await operation(owner);
    } catch (error) {
      if (mounted) setState(() => message = error.toString());
    } finally {
      if (mounted) {
        setState(() {
          busy = false;
          previewing = false;
        });
      }
    }
  }

  void requireOwner(DeviceStatus owner) {
    if (!sameOwner(owner)) {
      throw const FormatException('Board changed; no assignment was sent');
    }
  }

  void requireObservation(DeviceStatus observed, DeviceStatus owner) {
    requireOwner(owner);
    if (observed.json['board_id'] != owner.json['board_id'] ||
        observed.json['boot_id'] != owner.json['boot_id'] ||
        !observed.peer.tableSelection ||
        !observed.peer.has('cell-assignments')) {
      throw const FormatException(
        'Board role, boot or cell capabilities changed; operation cancelled',
      );
    }
  }

  Future<void> importCells() => run((owner) async {
    final file = await documents.pickDocument(
      maximum: CellAssignments.maximumFileBytes,
      extensions: const ['json', 'csv', 'xlsx'],
    );
    if (file == null) return;
    requireOwner(owner);
    final assignments = CellAssignments.decode(file.name, file.bytes);
    final observed = await widget.readStatus();
    requireObservation(observed, owner);
    final cells = CellAssignments.fromObservedCells(observed.json['cells']);
    final blocked = observed.json['red_lock'] == true
        ? assignments.values.entries
              .where((e) => cells.values[e.key] == 'red' && e.value == 'white')
              .map((e) => e.key)
              .toList()
        : <int>[];
    if (!mounted) return;
    setState(() => previewing = true);
    final apply = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Import cell colors'),
        content: SizedBox(
          width: 460,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(file.name, style: Theme.of(context).textTheme.titleSmall),
              const SizedBox(height: 8),
              Text(
                '${assignments.values.length} assignments · ${80 - assignments.values.length} cells unchanged',
              ),
              if (blocked.isNotEmpty)
                Padding(
                  padding: const EdgeInsets.only(top: 8),
                  child: Text(
                    'Red lock blocks cells ${blocked.join(', ')}. Nothing will be applied.',
                    style: TextStyle(
                      color: Theme.of(context).colorScheme.error,
                    ),
                  ),
                ),
              const SizedBox(height: 12),
              ConstrainedBox(
                constraints: const BoxConstraints(maxHeight: 280),
                child: SingleChildScrollView(
                  child: Table(
                    columnWidths: const {0: FixedColumnWidth(70)},
                    children: [
                      const TableRow(
                        children: [Text('Cell'), Text('Current → Imported')],
                      ),
                      for (final entry in assignments.values.entries)
                        TableRow(
                          children: [
                            Padding(
                              padding: const EdgeInsets.symmetric(vertical: 5),
                              child: Text('${entry.key}'),
                            ),
                            Padding(
                              padding: const EdgeInsets.symmetric(vertical: 5),
                              child: Text(
                                '${cells.values[entry.key]} → ${entry.value}',
                              ),
                            ),
                          ],
                        ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: blocked.isEmpty
                ? () => Navigator.pop(context, true)
                : null,
            child: const Text('Apply atomically'),
          ),
        ],
      ),
    );
    if (mounted) setState(() => previewing = false);
    if (apply != true) return;
    requireOwner(owner);
    final verified = await widget.command('/api/cells/assignments', {
      'assignments': assignments.commandValue,
    });
    if (mounted) {
      setState(
        () => message = verified
            ? '${assignments.values.length} cell assignments confirmed by board readback.'
            : 'Import was not confirmed. It was not replayed.',
      );
    }
  });

  Future<void> exportCells(String extension) => run((owner) async {
    final observed = await widget.readStatus();
    requireObservation(observed, owner);
    final assignments = CellAssignments.fromObservedCells(
      observed.json['cells'],
    );
    final written = await documents.export(
      'pakar-cells.$extension',
      assignments.encode(extension),
      stillOwned: () => sameOwner(owner),
    );
    if (written && mounted) {
      setState(() => message = 'Exported all 80 observed cell colors.');
    }
  });

  @override
  Widget build(BuildContext context) {
    final lock = widget.status.json['red_lock'];
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Wrap(
          spacing: 8,
          runSpacing: 4,
          crossAxisAlignment: WrapCrossAlignment.center,
          children: [
            if (widget.status.peer.has('cell-lock') ||
                widget.seenFeatures.contains('cell-lock'))
              FilterChip(
                avatar: const Icon(PakarIcons.lock, size: 18),
                label: const Text('Lock red cells'),
                selected: lock == true,
                tooltip: 'Prevent red cells changing back to white',
                onSelected:
                    ready && widget.status.peer.has('cell-lock') && lock is bool
                    ? (value) => run((owner) async {
                        final verified = await widget.command(
                          '/api/cells/lock',
                          {'enabled': '$value'},
                        );
                        if (mounted) {
                          setState(
                            () => message = verified
                                ? null
                                : 'Lock change was not confirmed.',
                          );
                        }
                      })
                    : null,
              ),
            if (widget.status.peer.has('cell-assignments') ||
                widget.seenFeatures.contains('cell-assignments')) ...[
              OutlinedButton.icon(
                onPressed: assignmentReady ? importCells : null,
                icon: const Icon(PakarIcons.gridFour, size: 18),
                label: const Text('Import colors…'),
              ),
              PopupMenuButton<String>(
                enabled: assignmentReady,
                tooltip: 'Export cell colors',
                onSelected: exportCells,
                itemBuilder: (_) => [
                  for (final format in ['xlsx', 'csv', 'json'])
                    PopupMenuItem(
                      value: format,
                      child: Text(
                        format == 'xlsx'
                            ? 'Excel workbook (.xlsx)'
                            : '${format.toUpperCase()} document',
                      ),
                    ),
                ],
                child: const Padding(
                  padding: EdgeInsets.symmetric(horizontal: 12, vertical: 10),
                  child: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Icon(PakarIcons.floppyDisk, size: 18),
                      SizedBox(width: 8),
                      Text('Export colors'),
                      Icon(PakarIcons.caretDown, size: 16),
                    ],
                  ),
                ),
              ),
            ],
            if (busy && !previewing)
              const SizedBox(
                width: 18,
                height: 18,
                child: CircularProgressIndicator(strokeWidth: 2),
              ),
          ],
        ),
        // Reserve feedback height: WebSocket observations never shift the table.
        SizedBox(
          height: 42,
          child: Text(
            message ?? '',
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ),
      ],
    );
  }
}
