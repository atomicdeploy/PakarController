import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'app_updates.dart';
import 'peer_identity.dart';

/// Data-only presentation updates. No Dart, HTML, scripts, paths or native code.
/// Reuses the update transport's bounded reads, origin policy and digest checks.
class PresentationAssets extends ValueNotifier<Map<String, int>> {
  PresentationAssets({this.save}) : super(const {});
  final Future<void> Function(String)? save;
  static const maximum = 24576;
  static const defaultSource =
      'https://office.digitalogic.ir/pakar/api/presentation';
  int revision = 0;
  String digest = '', source = '', message = '';
  bool enabled = true, foreground = true, busy = false;
  bool _closed = false;
  int _epoch = 0;
  bool _applying = false;
  String _stored = '';
  Timer? _timer;
  Future<void>? _checkTask;

  Future<void> load() async {
    final prefs = await SharedPreferences.getInstance();
    source = prefs.getString('pakar.presentation.source') ?? defaultSource;
    enabled = prefs.getBool('pakar.presentation.enabled') ?? true;
    try {
      final raw = prefs.getString('pakar.presentation.bundle');
      if (raw != null && raw.isNotEmpty) {
        final record = object(jsonDecode(raw));
        final text = record['bundle'] as String;
        final parsed = validate(text, record['sha256'] as String);
        revision = parsed.$1;
        digest = record['sha256'];
        value = parsed.$2;
        _stored = raw;
      }
    } on Object {
      message = 'Saved presentation data rejected; bundled theme retained';
    }
    _schedule();
  }

  static (int, Map<String, int>) validate(String text, String expected) {
    final bytes = utf8.encode(text);
    if (bytes.length > maximum ||
        !RegExp(r'^[a-f0-9]{64}$').hasMatch(expected) ||
        sha256.convert(bytes).toString() != expected) {
      throw const FormatException('Presentation size or SHA-256 mismatch');
    }
    final bundle = object(jsonDecode(text));
    final rev = bundle['revision'], palette = bundle['palette'];
    if (bundle.keys.any((k) => !{'revision', 'palette'}.contains(k)) ||
        rev is! int ||
        rev < 1 ||
        rev > 0x7fffffff ||
        palette is! Map ||
        palette.isEmpty ||
        palette.length > 4) {
      throw const FormatException('Expected a data-only presentation palette');
    }
    final colors = <String, int>{};
    for (final entry in palette.entries) {
      if (!{
            'primary',
            'secondary',
            'surface',
            'on_surface',
          }.contains(entry.key) ||
          entry.value is! String ||
          !RegExp(r'^[a-fA-F0-9]{6}$').hasMatch(entry.value)) {
        throw const FormatException(
          'Unsupported presentation role or RGB value',
        );
      }
      colors[entry.key as String] = int.parse(entry.value, radix: 16);
    }
    return (rev, Map.unmodifiable(colors));
  }

  Future<void> applyText(
    String text,
    String expected,
    bool Function() authorize,
  ) async {
    if (_applying) throw StateError('A presentation update is already pending');
    final parsed = validate(text, expected);
    if (parsed.$1 <= revision) {
      if (parsed.$1 == revision && digest == expected) return;
      throw const FormatException('Presentation revision is not newer');
    }
    if (_closed || !foreground || !authorize()) {
      throw StateError('Presentation session retired; nothing applied');
    }
    final encoded = jsonEncode({'bundle': text, 'sha256': expected});
    final previous = _stored;
    _applying = true;
    try {
      // One validated snapshot, no arbitrary file extraction or partial palette.
      if (save != null) {
        await save!(encoded);
      } else {
        final prefs = await SharedPreferences.getInstance();
        if (_closed || !foreground || !authorize()) {
          throw StateError('Presentation session retired; nothing applied');
        }
        if (!await prefs.setString('pakar.presentation.bundle', encoded)) {
          throw StateError('Presentation could not be saved');
        }
      }
      if (_closed || !foreground || !authorize()) {
        if (save != null) {
          await save!(previous);
        } else {
          final prefs = await SharedPreferences.getInstance();
          final restored = previous.isEmpty
              ? await prefs.remove('pakar.presentation.bundle')
              : await prefs.setString('pakar.presentation.bundle', previous);
          if (!restored) {
            throw StateError('Presentation rollback requires recovery');
          }
        }
        throw StateError(
          'Presentation session retired; previous palette restored',
        );
      }
      _stored = encoded;
      revision = parsed.$1;
      digest = expected;
      message = 'Presentation revision $revision applied';
      value = parsed.$2;
    } finally {
      _applying = false;
    }
  }

  Future<void> configure(String endpoint, bool automatic) async {
    endpoint = endpoint.trim();
    if (endpoint.isNotEmpty) validateAppUpdateUrl(endpoint);
    final prefs = await SharedPreferences.getInstance();
    if (!await prefs.setString('pakar.presentation.source', endpoint) ||
        !await prefs.setBool('pakar.presentation.enabled', automatic)) {
      throw StateError('Presentation update preferences could not be saved');
    }
    ++_epoch;
    source = endpoint;
    enabled = automatic;
    _schedule();
    notifyListeners();
  }

  Future<Map<String, dynamic>> _read(HttpClient client, Uri uri) async {
    final request = await client
        .getUrl(uri)
        .timeout(const Duration(seconds: 5));
    request.followRedirects = false;
    identifyRequest(request);
    final response = await request.close().timeout(const Duration(seconds: 6));
    if (response.statusCode != 200) {
      throw HttpException(
        'Presentation source rejected (${response.statusCode})',
      );
    }
    return object(
      jsonDecode(
        utf8.decode(
          await readAppUpdateBytes(
            response,
            maximum: 32768,
            deadline: const Duration(seconds: 8),
          ),
        ),
      ),
    );
  }

  Future<void> checkNow() {
    final active = _checkTask;
    if (active != null) return active;
    late final Future<void> task;
    task = _checkNow().whenComplete(() {
      if (identical(_checkTask, task)) _checkTask = null;
    });
    return _checkTask = task;
  }

  /// A notification is only a hint to read the configured trusted manifest.
  /// It cannot inject another URL, executable code or bypass disabled updates.
  Future<void> acceptNotice(Map<String, dynamic> notice) async {
    if (!enabled || !foreground || _closed) return;
    if (notice['manifest_url'] != source ||
        notice['revision'] is! int ||
        notice['revision'] < 1 ||
        !RegExp(r'^[a-f0-9]{64}$').hasMatch('${notice['sha256']}')) {
      throw const FormatException(
        'Presentation notice does not match the configured source',
      );
    }
    if (notice['revision'] <= revision) return;
    final owner = _epoch;
    final active = _checkTask;
    if (active != null) await active;
    if (owner == _epoch &&
        enabled &&
        foreground &&
        !_closed &&
        notice['revision'] > revision) {
      await checkNow();
    }
  }

  Future<void> _checkNow() async {
    if (busy || source.isEmpty || !foreground || _closed) return;
    final epoch = _epoch;
    busy = true;
    notifyListeners();
    final client = HttpClient()..connectionTimeout = const Duration(seconds: 5);
    try {
      final origin = validateAppUpdateUrl(source);
      final manifest = await _read(client, origin);
      if (epoch != _epoch || !foreground || _closed) return;
      if (manifest['revision'] is! int || manifest['revision'] <= revision) {
        return;
      }
      final url = manifest['url'],
          hash = manifest['sha256'],
          bytes = manifest['bytes'];
      if (url is! String ||
          hash is! String ||
          bytes is! int ||
          bytes < 1 ||
          bytes > maximum) {
        throw const FormatException('Invalid presentation manifest');
      }
      final target = validateAppUpdateUrl(url);
      if (target.origin != origin.origin) {
        throw const FormatException(
          'Presentation download must retain its trusted origin',
        );
      }
      final request = await client
          .getUrl(target)
          .timeout(const Duration(seconds: 5));
      identifyRequest(request);
      request.followRedirects = false;
      final response = await request.close().timeout(
        const Duration(seconds: 6),
      );
      if (response.statusCode != 200) {
        throw const HttpException('Presentation download rejected');
      }
      final payload = await readAppUpdateBytes(
        response,
        maximum: maximum,
        deadline: const Duration(seconds: 8),
      );
      if (payload.length != bytes) {
        throw const FormatException('Presentation length mismatch');
      }
      final text = utf8.decode(payload);
      if (validate(text, hash).$1 != manifest['revision']) {
        throw const FormatException('Presentation revision changed');
      }
      await applyText(
        text,
        hash,
        () => epoch == _epoch && foreground && !_closed,
      );
    } on Object catch (error) {
      message = '$error';
      rethrow;
    } finally {
      client.close(force: true);
      busy = false;
      if (!_closed) notifyListeners();
    }
  }

  void setForeground(bool active) {
    if (foreground != active) ++_epoch;
    foreground = active;
    _schedule();
  }

  void _schedule() {
    _timer?.cancel();
    if (!enabled || !foreground || _closed || source.isEmpty) return;
    unawaited(checkNow().catchError((Object _) {}));
    _timer = Timer.periodic(const Duration(minutes: 15), (_) {
      unawaited(checkNow().catchError((Object _) {}));
    });
  }

  @override
  void dispose() {
    _closed = true;
    ++_epoch;
    _timer?.cancel();
    super.dispose();
  }
}

final presentationAssets = PresentationAssets();
