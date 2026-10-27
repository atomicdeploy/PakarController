import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';

import 'api.dart';
import 'pixel_corrections_editor.dart';

/// One explicitly acquired, RAM-only preview lease. Edits coalesce while a
/// request is pending. Stale responses cannot re-enable a cancelled preview,
/// and cleanup can address only the original board/boot/connection owner.
/// While foreground-owned, successful readbacks renew the bounded board lease;
/// expiry is a disconnect safeguard, not a timed user interaction.
class PixelCorrectionPreview extends ChangeNotifier {
  PixelCorrectionPreview({
    required this.api,
    required this.allowed,
    required this.configuration,
    required this.pixels,
  });
  static const path = '/api/pixels/corrections/preview';
  static const ttlMs = 10000;
  final PakarApi api;
  final bool Function() allowed;
  final Map<String, dynamic> Function() configuration;
  final int Function() pixels;
  bool enabled = false, pending = false, _queued = false, _disposed = false;
  int _generation = 0, _owner = -1;
  String? _board, _boot;
  int? lease;
  String? error;
  Map<String, dynamic>? observed;
  Timer? _debounce, _expiry, _renew;
  bool get samePeer {
    final status = api.freshStatus;
    return api.connectionRevision == _owner &&
        status != null &&
        status.json['board_id'] == _board &&
        status.json['boot_id'] == _boot;
  }

  void changed() {
    if (!_disposed) notifyListeners();
  }

  Future<void> start() async {
    if (_disposed || enabled || pending || !allowed()) return;
    final status = api.freshStatus;
    if (status == null ||
        !status.peer.has('pixel-corrections-preview') ||
        status.json['board_id'] is! String ||
        status.json['boot_id'] is! String) {
      return;
    }
    _owner = api.connectionRevision;
    _board = status.json['board_id'] as String;
    _boot = status.json['boot_id'] as String;
    enabled = true;
    error = null;
    observed = null;
    lease = null;
    _generation++;
    await _send();
  }

  void edited() {
    if (!enabled || _disposed) return;
    _debounce?.cancel();
    _debounce = Timer(const Duration(milliseconds: 280), () {
      if (pending) {
        _queued = true;
      } else {
        unawaited(_send());
      }
    });
  }

  Future<void> _send() async {
    if (_disposed || !enabled || pending) return;
    if (!allowed() || !samePeer) {
      await stop();
      return;
    }
    _renew?.cancel();
    final generation = _generation;
    pending = true;
    _queued = false;
    error = null;
    changed();
    final issuedLease = lease;
    int? receivedLease = issuedLease;
    dynamic revision = observed?['revision'];
    final issuedAt = DateTime.now();
    try {
      final config =
          jsonDecode(jsonEncode(configuration())) as Map<String, dynamic>;
      if (issuedLease != null && config['revision'] != observed?['revision']) {
        throw const FormatException(
          'Saved corrections changed. Live preview stopped without replay.',
        );
      }
      revision = config['revision'];
      final expected = pixelCorrectionSources(
        pixels(),
        config['enabled'] == true,
        (config['rules'] as List)
            .map((value) => Map<String, dynamic>.from(value as Map))
            .toList(),
      );
      final reply = await api.previewCorrections(
        {...config, 'ttl_ms': ttlMs, 'lease_id': ?issuedLease},
        authorize: () =>
            !_disposed &&
            enabled &&
            generation == _generation &&
            allowed() &&
            samePeer,
      );
      final receipt = reply['preview'];
      if (receipt is Map &&
          receipt['active'] == true &&
          receipt['lease_id'] is int &&
          receipt['lease_id'] > 0 &&
          receipt['lease_id'] <= 0xffffffff) {
        final returnedLease = receipt['lease_id'] as int;
        if (issuedLease != null && returnedLease != issuedLease) {
          throw const FormatException(
            'Preview owner changed. No foreign preview was modified.',
          );
        }
        receivedLease = returnedLease;
      }
      if (_disposed ||
          !enabled ||
          generation != _generation ||
          !allowed() ||
          !samePeer) {
        if (receivedLease != null) await _release(receivedLease, revision);
        return;
      }
      final result = await api.getJson('/api/pixels/corrections');
      final preview = result['preview'];
      final actual = result['active_sources'];
      if (preview is! Map ||
          preview['active'] != true ||
          preview['lease_id'] != receivedLease ||
          receivedLease == null ||
          preview['base_revision'] != revision ||
          preview['remaining_ms'] is! int ||
          preview['remaining_ms'] <= 0 ||
          preview['remaining_ms'] > ttlMs ||
          result['revision'] != revision ||
          actual is! List ||
          !listEquals(actual, expected)) {
        throw const FormatException(
          'Preview readback differs. No preview was confirmed.',
        );
      }
      if (_disposed ||
          !enabled ||
          generation != _generation ||
          !allowed() ||
          !samePeer) {
        await _release(receivedLease, revision);
        return;
      }
      final remaining =
          (ttlMs - DateTime.now().difference(issuedAt).inMilliseconds).clamp(
            0,
            preview['remaining_ms'] as int,
          );
      if (remaining < 500) {
        throw const FormatException(
          'Live preview confirmation arrived too late. Reconnect before retrying.',
        );
      }
      lease = receivedLease;
      observed = result;
      _expiry?.cancel();
      _expiry = Timer(Duration(milliseconds: remaining), () {
        error = 'Live preview connection expired. No changes were saved.';
        unawaited(stop());
      });
      // Adapt the donor PeripheralMirror's owner/session refresh and expiry
      // cancellation: only confirmed same-owner leases may be renewed here.
      // One-shot scheduling (not periodic) prevents overlap or catch-up bursts.
      _renew = Timer(Duration(milliseconds: remaining ~/ 2), () {
        if (pending) {
          _queued = true;
        } else {
          unawaited(_send());
        }
      });
    } catch (failure) {
      if (receivedLease != null) await _release(receivedLease, revision);
      if (!_disposed && generation == _generation) {
        enabled = false;
        lease = null;
        observed = null;
        _expiry?.cancel();
        _renew?.cancel();
        error = failure.toString();
      }
    } finally {
      pending = false;
      changed();
      if (_queued && enabled && generation == _generation && !_disposed) {
        _queued = false;
        edited();
      }
    }
  }

  Future<void> _release(int value, dynamic revision) async {
    if (!samePeer) return; // Never redirect a delayed cleanup to another peer.
    final owner = _owner, board = _board, boot = _boot;
    bool authorized() =>
        api.connectionRevision == owner &&
        api.freshStatus?.json['board_id'] == board &&
        api.freshStatus?.json['boot_id'] == boot;
    try {
      await api.previewCorrections({
        'clear': true,
        'lease_id': value,
        'revision': revision,
      }, authorize: authorized);
    } catch (_) {
      // No mutation replay. The board's bounded lease is the recovery path.
    }
  }

  Future<void> stop() async {
    final ownedLease = lease;
    final revision = observed?['revision'];
    enabled = false;
    lease = null;
    observed = null;
    _generation++;
    _queued = false;
    _debounce?.cancel();
    _expiry?.cancel();
    _renew?.cancel();
    changed();
    if (ownedLease != null) await _release(ownedLease, revision);
  }

  /// A verified durable save ends the same owner's board lease atomically.
  void committed() {
    enabled = false;
    lease = null;
    observed = null;
    _generation++;
    _queued = false;
    _debounce?.cancel();
    _expiry?.cancel();
    _renew?.cancel();
    changed();
  }

  @override
  void dispose() {
    unawaited(stop());
    _disposed = true;
    super.dispose();
  }
}
