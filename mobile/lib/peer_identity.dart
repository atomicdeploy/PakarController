import 'dart:convert';
import 'dart:io';
import 'dart:math';

import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'peer_policy.dart';

/// Defaults follow pubspec; Android obtains the actual installed package values
/// with a lightweight native call, not a digest of the entire APK.
const peerDefaultVersion = '0.2.8', peerDefaultBuild = '16';

String _id() {
  final bytes = List.generate(16, (_) => Random.secure().nextInt(256));
  bytes[6] = (bytes[6] & 15) | 64;
  bytes[8] = (bytes[8] & 63) | 128;
  final hex = bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join();
  return '${hex.substring(0, 8)}-${hex.substring(8, 12)}-${hex.substring(12, 16)}-${hex.substring(16, 20)}-${hex.substring(20)}';
}

String _text(Object? raw, int maximum) {
  if (raw is! String) return '';
  final clean = raw.replaceAll(RegExp(r'[\x00-\x1f\x7f]'), ' ');
  return clean.length <= maximum ? clean : clean.substring(0, maximum);
}

class PeerIdentity {
  PeerIdentity() {
    // Dart's WebSocket.connect appends supplied headers to its default HTTP
    // headers. Configure the documented default once and omit UA from the
    // extra WS headers, rather than emitting two comma-joined User-Agents.
    WebSocket.userAgent = userAgent;
  }
  static final current = PeerIdentity();
  static const channel = MethodChannel('ir.digitalogic.pakar/app-updates');
  final sessionId = _id();
  late String installationId = sessionId;
  String version = peerDefaultVersion, build = peerDefaultBuild;
  String model = '', manufacturer = '';
  Map<String, dynamic> hardware = {
    'cpu_count': Platform.numberOfProcessors,
    'os_version': _text(Platform.operatingSystemVersion, 96),
  };
  String architecture = '';
  Future<void>? _loading;

  Future<void> initialize() => _loading ??= _initialize();
  Future<void> _initialize() async {
    try {
      final preferences = await SharedPreferences.getInstance();
      var saved = preferences.getString('pakarcontroller.peer_installation');
      if (saved == null) {
        // Preserve the existing app discovery identity across every transport.
        try {
          final previous = jsonDecode(
            preferences.getString('pakar.app_updates') ?? '{}',
          );
          if (previous is Map && previous['instance_id'] is String) {
            saved = previous['instance_id'];
          }
        } on FormatException {
          // Corrupt unrelated preferences never become a peer identity.
        }
      }
      if (saved != null && RegExp(r'^[A-Za-z0-9_-]{16,80}$').hasMatch(saved)) {
        installationId = saved;
        if (preferences.getString('pakarcontroller.peer_installation') !=
            saved) {
          await preferences.setString(
            'pakarcontroller.peer_installation',
            saved,
          );
        }
      } else {
        installationId = _id();
        await preferences.setString(
          'pakarcontroller.peer_installation',
          installationId,
        );
      }
    } on Object {
      // Session identity is still truthful if storage is temporarily unavailable.
    }
    if (!Platform.isAndroid) return;
    try {
      final raw = await channel
          .invokeMapMethod<String, dynamic>('peerInfo')
          .timeout(const Duration(seconds: 2));
      if (raw == null) return;
      if (PeerVersion.parse(raw['version']) != null) version = raw['version'];
      build = _text('${raw['build'] ?? build}', 64);
      model = _text(raw['model'], 64);
      manufacturer = _text(raw['manufacturer'], 48);
      architecture = _text(raw['architecture'], 48);
      hardware = {
        ...hardware,
        if (model.isNotEmpty) 'model': model,
        if (manufacturer.isNotEmpty) 'manufacturer': manufacturer,
        if (raw['sdk'] is int) 'sdk': raw['sdk'],
        if (raw['memory_mb'] is int) 'memory_mb': raw['memory_mb'],
        if (raw['os_version'] is String)
          'os_version': _text(raw['os_version'], 96),
      };
    } on Object {
      // Keep the checked package identity if the optional hardware call fails;
      // do not invent unavailable model/architecture values or block startup.
    } finally {
      WebSocket.userAgent = userAgent;
    }
  }

  String get userAgent =>
      'PakarController/$version (${Platform.operatingSystem}; mobile; build/$build)';
  Map<String, String> get headers => {
    'User-Agent': userAgent,
    'X-Pakar-Client': 'pakarcontroller-mobile',
    'X-Pakar-Version': version,
    'X-Pakar-Client-Id': installationId,
    'X-Pakar-Client-Session': sessionId,
    'X-Pakar-Build': build,
  };
  Map<String, String> get webSocketHeaders =>
      {...headers}..remove('User-Agent');
  Map<String, String> get serverHeaders => {
    'Server': userAgent,
    'X-Pakar-Product': 'pakarcontroller-mobile',
    'X-Pakar-Version': version,
    'X-Pakar-Build': build,
    'X-Pakar-Instance': installationId,
  };

  Map<String, dynamic> report({
    String transport = 'network',
    bool enforce = enforcePeerVersions,
    String? reportedIp,
  }) => {
    'id': installationId,
    'session_id': sessionId,
    'application': 'pakarcontroller-mobile',
    'version': version,
    'build': build,
    'hostname': _text(Platform.localHostname, 64),
    'os': Platform.operatingSystem,
    'architecture': architecture,
    'transport': transport,
    'reported_ip': ?_routeAddress(reportedIp),
    if (model.isNotEmpty) 'device_model': model,
    'hardware': hardware,
    'capabilities': [
      'http',
      'websocket',
      'bluetooth',
      'cell-batch',
      'firmware-transfer',
      'front-panel-mirror',
      'buzzer-mirror',
      'peer-sharing',
    ],
    'settings': {
      'transport': transport,
      'theme': 'dark', // main.MaterialApp forces ThemeMode.dark; no theme preference exists.
      'version_check': enforce,
    },
  };

  String reportJson({
    String transport = 'network',
    bool enforce = enforcePeerVersions,
    String? reportedIp,
  }) {
    final value = jsonEncode(
      report(transport: transport, enforce: enforce, reportedIp: reportedIp),
    );
    if (utf8.encode(value).length > 1536) {
      throw const FormatException('Peer introduction exceeds its safety limit');
    }
    return value;
  }
}

// The caller supplies only an actual selected connection's local address.
// Absence is unknown, not permission to choose an unrelated interface's IP.
String? _routeAddress(String? value) {
  final address = value == null ? null : InternetAddress.tryParse(value);
  return address == null ||
          address.isMulticast ||
          address.address == '0.0.0.0' ||
          address.address == '::'
      ? null
      : address.address;
}

void identifyRequest(HttpClientRequest request) {
  PeerIdentity.current.headers.forEach(
    (key, value) => request.headers.set(key, value),
  );
}

void identifyResponse(HttpResponse response) {
  PeerIdentity.current.serverHeaders.forEach(
    (key, value) => response.headers.set(key, value),
  );
}

/// Bounded self-reported operational metadata, not authentication or discovery
/// authority. Observed source is kept distinct from a claimed client IP.
class PeerRegistry {
  final _peers = <String, Map<String, dynamic>>{};
  List<Map<String, dynamic>> get peers => List.unmodifiable(_peers.values);
  void observe(Object? raw, {String source = ''}) {
    if (raw is! Map || utf8.encode(jsonEncode(raw)).length > 1536) {
      throw const FormatException('Invalid peer introduction size');
    }
    final id = raw['id'];
    if (id is! String ||
        !RegExp(r'^[A-Za-z0-9_-]{16,80}$').hasMatch(id) ||
        PeerVersion.parse(raw['version']) == null) {
      throw const FormatException('Invalid peer identity/version');
    }
    final value = <String, dynamic>{};
    for (final field in [
      'id',
      'session_id',
      'application',
      'version',
      'build',
      'hostname',
      'os',
      'architecture',
      'transport',
      'device_model',
    ]) {
      final item = raw[field];
      if (item is String) {
        value[field] = _text(item, field == 'version' ? 96 : 80);
      }
      if (field == 'build' && item is int) value[field] = item;
    }
    Map<String, dynamic> allowObject(Object? object, Set<String> allowed) => {
      if (object is Map)
        for (final key in allowed)
          if (object[key] is bool ||
              object[key] is num ||
              object[key] is String)
            key: object[key] is String ? _text(object[key], 96) : object[key],
    };
    value['hardware'] = allowObject(raw['hardware'], {
      'model',
      'manufacturer',
      'cpu',
      'os_version',
      'sdk',
      'memory_mb',
      'cpu_count',
      'screen_width',
      'screen_height',
    });
    value['settings'] = allowObject(raw['settings'], {
      'transport',
      'theme',
      'language',
      'silent_mode',
      'telemetry_enabled',
      'version_check',
    });
    value['capabilities'] = raw['capabilities'] is List
        ? (raw['capabilities'] as List)
              .whereType<String>()
              .where((v) => RegExp(r'^[!-~]{1,32}$').hasMatch(v))
              .take(12)
              .toSet()
              .toList()
        : <String>[];
    final claimed = raw['reported_ip'];
    if (claimed is String && InternetAddress.tryParse(claimed) != null) {
      value['reported_ip'] = claimed;
      value['ip_self_reported'] = true;
    }
    value['observed_source'] = _text(source, 96);
    value['last_seen_ms'] = DateTime.now().millisecondsSinceEpoch;
    _peers.remove(id);
    if (_peers.length >= 32) _peers.remove(_peers.keys.first);
    _peers[id] = Map.unmodifiable(value);
  }
}
