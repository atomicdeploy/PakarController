import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:crypto/crypto.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

const appUpdatePackage = 'ir.digitalogic.pakar.pakarcontroller';
const appUpdateServiceType = '_pakar-update._tcp.';
const appUpdateSsdpType = 'urn:atomicdeploy:device:PakarControllerApp';

/// A read-only package source, never a connected board or installation grant.
class AppUpdatePeer {
  const AppUpdatePeer({
    required this.name,
    required this.origin,
    required this.instanceId,
    this.verified = false,
    this.manifest = const {},
    this.protocols = const {},
  });
  final String name, origin, instanceId;
  final bool verified;
  final Map<String, dynamic> manifest;
  final Set<String> protocols;
}

String _origin(String input) {
  final uri = Uri.parse(input.trim());
  if (!{'http', 'https'}.contains(uri.scheme) ||
      uri.host.isEmpty ||
      uri.userInfo.isNotEmpty ||
      uri.hasQuery ||
      uri.hasFragment ||
      !{'', '/', '/api/app-updates'}.contains(uri.path) ||
      uri.port < 1 ||
      uri.port > 65535) {
    throw const FormatException('Invalid app update endpoint');
  }
  return uri.replace(path: '', query: null, fragment: null).toString();
}

bool _text(Object? value, int limit) =>
    value is String &&
    value.isNotEmpty &&
    value.length <= limit &&
    !RegExp(r'[\x00-\x1f\x7f]').hasMatch(value);
bool _digest(Object? value) =>
    value is String && RegExp(r'^[a-f0-9]{64}$').hasMatch(value);

/// Adapted from PCController's source-pinned discovery verifier: DIRECT GET,
/// no redirects, credentials, commands or board-selected API client. Package
/// signer compatibility is a later local APK inspection, not advertisement trust.
Future<AppUpdatePeer?> verifyAppUpdatePeer(AppUpdatePeer candidate) async {
  final client = HttpClient()
    ..findProxy = ((_) => 'DIRECT')
    ..connectionTimeout = const Duration(seconds: 1);
  Future<AppUpdatePeer?> read() async {
    final origin = _origin(candidate.origin);
    final request = await client.getUrl(
      Uri.parse(origin).resolve('/api/app-updates'),
    );
    request.followRedirects = false;
    final response = await request.close();
    if (response.statusCode != 200 || response.contentLength > 16384) {
      return null;
    }
    final bytes = <int>[];
    await for (final chunk in response) {
      if (bytes.length + chunk.length > 16384) return null;
      bytes.addAll(chunk);
    }
    final raw = jsonDecode(utf8.decode(bytes));
    if (raw is! Map<String, dynamic> ||
        raw['product'] != 'PakarController' ||
        raw['kind'] != 'android-app' ||
        raw['package_sharing'] != true ||
        raw['rpc'] != '/api/rpc' ||
        !_text(raw['instance_id'], 128) ||
        raw['app_updates'] is! Map<String, dynamic>) {
      return null;
    }
    final package = raw['app_updates'] as Map<String, dynamic>;
    final signers = package['signers'];
    if (package['package'] != appUpdatePackage ||
        package['version_code'] is! int ||
        (package['version_code'] as int) < 1 ||
        !_text(package['version_name'], 96) ||
        package['bytes'] is! int ||
        (package['bytes'] as int) < 1 ||
        (package['bytes'] as int) > 128 * 1024 * 1024 ||
        !_digest(package['sha256']) ||
        signers is! List ||
        signers.isEmpty ||
        signers.length > 8 ||
        !signers.every(_digest) ||
        signers.toSet().length != signers.length ||
        candidate.instanceId.isNotEmpty &&
            candidate.instanceId != raw['instance_id']) {
      return null;
    }
    return AppUpdatePeer(
      name: _text(raw['name'], 96) ? raw['name'] as String : candidate.name,
      origin: origin,
      instanceId: raw['instance_id'] as String,
      verified: true,
      manifest: Map<String, dynamic>.unmodifiable(raw),
      protocols: candidate.protocols,
    );
  }

  try {
    return await read().timeout(const Duration(seconds: 3));
  } on Object {
    return null;
  } finally {
    client.close(force: true);
  }
}

AppUpdatePeer? parseAppUpdateSsdp(Uint8List bytes, String source) {
  if (bytes.length > 4096) return null;
  try {
    final lines = const LineSplitter().convert(utf8.decode(bytes));
    if (lines.isEmpty || lines.first != 'HTTP/1.1 200 OK') return null;
    final headers = <String, String>{};
    for (final line in lines.skip(1)) {
      final colon = line.indexOf(':');
      if (colon <= 0) continue;
      final key = line.substring(0, colon).trim().toLowerCase();
      if (headers.containsKey(key)) return null;
      headers[key] = line.substring(colon + 1).trim();
    }
    final location = Uri.parse(headers['location'] ?? '');
    if (headers['st'] != appUpdateSsdpType ||
        headers['x-pakar-product'] != 'pakarcontroller-mobile' ||
        location.path != '/api/app-updates' ||
        InternetAddress.tryParse(location.host)?.address != source ||
        !_text(headers['x-pakar-instance'], 128)) {
      return null;
    }
    return AppUpdatePeer(
      name: _text(headers['x-pakar-name'], 96)
          ? headers['x-pakar-name']!
          : 'PakarController',
      origin: _origin(location.toString()),
      instanceId: headers['x-pakar-instance']!,
      protocols: const {'SSDP'},
    );
  } on FormatException {
    return null;
  }
}

AppUpdatePeer? parseAppUpdateMndp(Uint8List bytes, String source) {
  final ip = InternetAddress.tryParse(source);
  if (bytes.length < 8 ||
      bytes.length > 2048 ||
      bytes[0] != 0 ||
      bytes[1] != 0 ||
      ip == null ||
      ip.isMulticast ||
      ip.type != InternetAddressType.IPv4 ||
      source == '0.0.0.0') {
    return null;
  }
  try {
    final view = ByteData.sublistView(bytes), fields = <int, Uint8List>{};
    for (var offset = 4; offset < bytes.length;) {
      if (offset + 4 > bytes.length) return null;
      final type = view.getUint16(offset), length = view.getUint16(offset + 2);
      offset += 4;
      if (offset + length > bytes.length || fields.containsKey(type)) {
        return null;
      }
      fields[type] = bytes.sublist(offset, offset + length);
      offset += length;
    }
    final name = utf8.decode(fields[5] ?? []);
    final instance = utf8.decode(fields[11] ?? []);
    if (!_text(name, 96) ||
        !_text(instance, 128) ||
        fields[1]?.length != 6 ||
        utf8.decode(fields[0xff01] ?? []) != 'pakarcontroller-mobile' ||
        fields[0xff02]?.length != 2) {
      return null;
    }
    final port = ByteData.sublistView(fields[0xff02]!).getUint16(0);
    if (port == 0) return null;
    return AppUpdatePeer(
      name: name,
      origin: _origin('http://$source:$port'),
      instanceId: instance,
      protocols: const {'MNDP'},
    );
  } on FormatException {
    return null;
  }
}

/// Foreground, opt-in package-source discovery/advertising. Adapted from actual
/// PCController discovery.dart + peer_upgrades.dart; no board API or install RPC
/// is reachable here. Lost/background ownership closes sockets and retires scans.
class AppUpdateDiscovery extends ChangeNotifier {
  AppUpdateDiscovery({this.advertisements, this.verify});
  static const channel = MethodChannel(
    'ir.digitalogic.pakar/app-update-discovery',
  );
  final Future<List<AppUpdatePeer>> Function()? advertisements;
  final Future<AppUpdatePeer?> Function(AppUpdatePeer)? verify;
  final _peers = <AppUpdatePeer>[];
  List<AppUpdatePeer> get peers => List.unmodifiable(_peers);
  bool scanning = false;
  String error = '';
  final errors = <String, String>{};
  bool _disposed = false;
  int _epoch = 0, _advertiseEpoch = 0, _sequence = 0;
  int? _port;
  String _instance = '', _name = '';
  final _sockets = <RawDatagramSocket>[];
  final _scanners = <RawDatagramSocket>[];
  final _responded = <String, DateTime>{};
  List<InternetAddress> _addresses = [];
  Timer? _announce;

  Future<void> scan({Iterable<String> seeds = const []}) async {
    if (_disposed || scanning) return;
    final epoch = ++_epoch;
    scanning = true;
    error = '';
    errors.removeWhere((adapter, _) => !adapter.endsWith('advertising'));
    notifyListeners();
    final found = <AppUpdatePeer>[];
    try {
      if (advertisements != null) {
        found.addAll(await advertisements!());
      } else {
        await _guard('multicast', () => channel.invokeMethod<void>('acquire'));
        await Future.wait([
          _guard('mDNS / DNS-SD', () async {
            final raw = await channel.invokeListMethod<dynamic>('scan');
            for (final value in raw ?? []) {
              if (value is! Map || !_text(value['name'], 96)) continue;
              try {
                found.add(
                  AppUpdatePeer(
                    name: value['name'] as String,
                    origin: _origin(value['endpoint'] as String),
                    instanceId: value['instance_id'] is String
                        ? value['instance_id'] as String
                        : '',
                    protocols: const {'mDNS / DNS-SD'},
                  ),
                );
              } on Object {
                /* Malformed advertisement, not an app. */
              }
            }
          }),
          _guard('SSDP', () => _scanUdp(found, false, epoch)),
          _guard('MNDP', () => _scanUdp(found, true, epoch)),
        ]);
      }
      for (final seed in seeds.take(32)) {
        try {
          final origin = _origin(seed);
          found.add(
            AppUpdatePeer(
              name: Uri.parse(origin).host,
              origin: origin,
              instanceId: '',
              protocols: const {'Saved endpoint'},
            ),
          );
        } on FormatException {
          /* No auth, paths or redirects in scan seeds. */
        }
      }
      final candidates = <String, AppUpdatePeer>{};
      for (final candidate in found.take(128)) {
        if (candidate.instanceId.isNotEmpty &&
            candidate.instanceId == _instance) {
          continue;
        }
        final previous = candidates[candidate.origin];
        if (previous != null &&
            previous.instanceId.isNotEmpty &&
            candidate.instanceId.isNotEmpty &&
            previous.instanceId != candidate.instanceId) {
          continue; // Conflicting adverts cannot change a queued identity fence.
        }
        candidates[candidate.origin] = AppUpdatePeer(
          name: candidate.name,
          origin: candidate.origin,
          instanceId: candidate.instanceId.isNotEmpty
              ? candidate.instanceId
              : previous?.instanceId ?? '',
          protocols: {...?previous?.protocols, ...candidate.protocols},
        );
      }
      final queue = candidates.values.toList(), verified = <AppUpdatePeer>[];
      var next = 0;
      final deadline = DateTime.now().add(const Duration(seconds: 12));
      await Future.wait(
        List.generate(4, (_) async {
          while (next < queue.length &&
              DateTime.now().isBefore(deadline) &&
              !_disposed &&
              epoch == _epoch) {
            final peer = await (verify ?? verifyAppUpdatePeer)(queue[next++]);
            if (peer != null && peer.verified && peer.instanceId != _instance) {
              verified.add(peer);
            }
          }
        }),
      );
      if (_disposed || epoch != _epoch) return;
      final dedup = <String, AppUpdatePeer>{};
      final conflicting = <String>{};
      for (final peer in verified) {
        if (conflicting.contains(peer.instanceId)) continue;
        final previous = dedup[peer.instanceId];
        if (previous != null &&
            jsonEncode(previous.manifest['app_updates']) !=
                jsonEncode(peer.manifest['app_updates'])) {
          dedup.remove(peer.instanceId);
          conflicting.add(peer.instanceId);
          continue; // A reused instance ID cannot silently select other bytes.
        }
        dedup[peer.instanceId] = AppUpdatePeer(
          name: peer.name,
          origin: previous?.origin ?? peer.origin,
          instanceId: peer.instanceId,
          verified: true,
          manifest: peer.manifest,
          protocols: {...?previous?.protocols, ...peer.protocols},
        );
      }
      _peers
        ..clear()
        ..addAll(dedup.values);
      _peers.sort((a, b) => a.name.compareTo(b.name));
      if (errors.isNotEmpty) {
        error = errors.entries.map((e) => '${e.key}: ${e.value}').join('; ');
      }
    } on Object catch (failure) {
      if (!_disposed && epoch == _epoch) {
        _peers.clear();
        error = '$failure';
      }
    } finally {
      try {
        await channel.invokeMethod<void>('release');
      } on Object {
        /* Cleanup. */
      }
      if (!_disposed && epoch == _epoch) {
        scanning = false;
        notifyListeners();
      }
    }
  }

  Future<void> _guard(String adapter, Future<void> Function() action) async {
    try {
      await action();
      errors.remove(adapter);
    } on Object catch (failure) {
      errors[adapter] = '$failure';
    }
  }

  Future<void> _scanUdp(List<AppUpdatePeer> found, bool mndp, int epoch) async {
    // Replies are unicast to the sender's ephemeral port. No competing fixed
    // MNDP/SSDP listener or selected-board discovery ownership is needed.
    final socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);
    if (_disposed || epoch != _epoch) {
      socket.close();
      return;
    }
    _scanners.add(socket);
    socket.broadcastEnabled = true;
    final subscription = socket.listen((event) {
      if (event != RawSocketEvent.read || _disposed || epoch != _epoch) return;
      for (var count = 0; count < 64; count++) {
        final packet = socket.receive();
        if (packet == null) break;
        if (found.length >= 128) continue;
        final p = packet;
        final peer = mndp
            ? parseAppUpdateMndp(p.data, p.address.address)
            : parseAppUpdateSsdp(p.data, p.address.address);
        if (peer != null) found.add(peer);
      }
    });
    try {
      if (mndp) {
        for (final target in ['255.255.255.255', '224.0.0.88']) {
          socket.send([0, 0, 0, 0], InternetAddress(target), 5678);
        }
      } else {
        socket.send(
          utf8.encode(
            'M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\n'
            'MAN: "ssdp:discover"\r\nMX: 1\r\nST: $appUpdateSsdpType\r\n\r\n',
          ),
          InternetAddress('239.255.255.250'),
          1900,
        );
      }
      await Future<void>.delayed(const Duration(seconds: 3));
    } finally {
      await subscription.cancel();
      socket.close();
      _scanners.remove(socket);
    }
  }

  Future<void> advertise({
    required int port,
    required String name,
    required String instanceId,
  }) async {
    if (_disposed) return;
    if (port < 1024 ||
        port > 65535 ||
        !_text(name, 96) ||
        !_text(instanceId, 128)) {
      throw const FormatException('Invalid app advertisement');
    }
    await stopAdvertising();
    if (_disposed) return;
    final epoch = ++_advertiseEpoch;
    _port = port;
    _name = name;
    _instance = instanceId;
    _addresses = (await NetworkInterface.list(type: InternetAddressType.IPv4))
        .expand((n) => n.addresses)
        .where((ip) => !ip.isLoopback)
        .toList();
    if (_disposed || epoch != _advertiseEpoch) return;
    await _guard('mDNS advertising', () async {
      await channel.invokeMethod<void>('advertise', {
        'port': port,
        'name': name,
        'instance_id': instanceId,
      });
    });
    if (_disposed || epoch != _advertiseEpoch) return;
    for (final mndp in [false, true]) {
      await _guard(
        mndp ? 'MNDP advertising' : 'SSDP advertising',
        () => _advertiseUdp(mndp, epoch),
      );
    }
    if (_disposed || epoch != _advertiseEpoch) return;
    _announce = Timer.periodic(const Duration(seconds: 30), (_) => _beacon());
    _beacon();
  }

  Future<void> _advertiseUdp(bool mndp, int epoch) async {
    final socket = await RawDatagramSocket.bind(
      InternetAddress.anyIPv4,
      mndp ? 5678 : 1900,
      reuseAddress: true,
    );
    if (_disposed || epoch != _advertiseEpoch) {
      socket.close();
      return;
    }
    try {
      socket.broadcastEnabled = true;
      socket.joinMulticast(
        InternetAddress(mndp ? '224.0.0.88' : '239.255.255.250'),
      );
      socket.listen((event) {
        if (event != RawSocketEvent.read ||
            _disposed ||
            epoch != _advertiseEpoch) {
          return;
        }
        for (var count = 0; count < 64; count++) {
          final p = socket.receive();
          if (p == null) break;
          if (p.data.length > 4096 ||
              p.address.isLoopback ||
              p.address.isMulticast) {
            continue;
          }
          final text = mndp
              ? ''
              : utf8.decode(p.data, allowMalformed: true).toLowerCase();
          final search = mndp
              ? p.data.length == 4 && p.data.every((b) => b == 0)
              : text.startsWith('m-search * http/1.1\r\n') &&
                    text.contains('man: "ssdp:discover"') &&
                    (text.contains('st: ssdp:all') ||
                        text.contains(
                          'st: ${appUpdateSsdpType.toLowerCase()}',
                        ));
          if (!search) continue;
          _responded.removeWhere(
            (_, at) => DateTime.now().difference(at).inSeconds > 1,
          );
          final key = '${p.address.address}|$mndp';
          if (_responded.containsKey(key) || _responded.length >= 128) continue;
          _responded[key] = DateTime.now();
          if (mndp) {
            socket.send(_mndp(), p.address, p.port);
          } else {
            unawaited(_replySsdp(p.address, p.port, epoch));
          }
        }
      });
      _sockets.add(socket);
    } on Object {
      socket.close();
      rethrow;
    }
  }

  Future<void> _replySsdp(
    InternetAddress destination,
    int port,
    int epoch,
  ) async {
    for (final ip in _addresses) {
      if (_disposed || epoch != _advertiseEpoch || _port == null) return;
      RawDatagramSocket? socket;
      try {
        socket = await RawDatagramSocket.bind(ip, 0);
        if (_disposed || epoch != _advertiseEpoch || _port == null) return;
        socket.send(
          utf8.encode(
            'HTTP/1.1 200 OK\r\nST: $appUpdateSsdpType\r\n'
            'LOCATION: http://${ip.address}:$_port/api/app-updates\r\n'
            'X-Pakar-Product: pakarcontroller-mobile\r\nX-Pakar-Instance: $_instance\r\n'
            'X-Pakar-Name: $_name\r\nCACHE-CONTROL: max-age=60\r\n\r\n',
          ),
          destination,
          port,
        );
      } on SocketException {
        /* Removed interface; foreground restart refreshes. */
      } finally {
        socket?.close();
      }
    }
  }

  Uint8List _mndp() {
    final mac = sha256.convert(utf8.encode(_instance)).bytes.take(6).toList();
    mac[0] =
        (mac[0] | 2) &
        254; // Explicit synthetic application identity, not NIC MAC.
    final bytes = BytesBuilder()
      ..add([0, 0, (_sequence >> 8) & 255, _sequence++ & 255]);
    void field(int type, List<int> value) {
      bytes.add([type >> 8, type & 255, value.length >> 8, value.length & 255]);
      bytes.add(value);
    }

    field(1, mac);
    field(5, utf8.encode(_name));
    field(11, utf8.encode(_instance));
    field(12, utf8.encode('Android application endpoint'));
    field(0xff01, utf8.encode('pakarcontroller-mobile'));
    field(0xff02, [_port! >> 8, _port! & 255]);
    return bytes.takeBytes();
  }

  void _beacon() {
    if (_disposed || _port == null) return;
    for (final socket in _sockets.where((s) => s.port == 5678)) {
      try {
        socket.send(_mndp(), InternetAddress('255.255.255.255'), 5678);
        socket.send(_mndp(), InternetAddress('224.0.0.88'), 5678);
      } on SocketException {
        /* Discovery failure is not board connection loss. */
      }
    }
  }

  Future<void> stopAdvertising() async {
    ++_advertiseEpoch;
    _announce?.cancel();
    _announce = null;
    _port = null;
    for (final socket in _sockets) {
      socket.close();
    }
    _sockets.clear();
    _responded.clear();
    _addresses.clear();
    try {
      await channel.invokeMethod<void>('unadvertise');
    } on Object {
      /* Cleanup. */
    }
  }

  @override
  void dispose() {
    if (_disposed) return;
    _disposed = true;
    ++_epoch;
    for (final socket in _scanners) {
      socket.close();
    }
    _scanners.clear();
    unawaited(stopAdvertising());
    super.dispose();
  }
}
