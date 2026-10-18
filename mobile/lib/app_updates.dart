import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

// Adapted from the completely read PCController unified/app_upgrade.dart.
typedef AppUpdateJson = Map<String, dynamic>;
typedef Json = AppUpdateJson;
Json object(dynamic value) => value is Map
    ? Map<String, dynamic>.from(value)
    : throw const FormatException('Expected an update object');

/// One self-package transfer, shared by UI and protected HTTP/WS/TCP/Intent RPC.
/// Installed APK export is read-only; receiving/installing never auto-retries.
class AppUpdates extends ChangeNotifier {
  static const channel = MethodChannel('ir.digitalogic.pakar/app-updates');
  static const maximum = 128 * 1024 * 1024, chunkSize = 16384;
  static const methods = {
    'app.package.info',
    'app.package.read',
    'app.upgrade.begin',
    'app.upgrade.write',
    'app.upgrade.finish',
    'app.upgrade.abort',
    'app.upgrade.status',
    'app.upgrade.install',
    'app.upgrade.permission',
    'app.upgrade.cleanup',
    'app.upgrade.pull',
    'app.upgrade.push',
  };
  Json state = {'state': 'idle'};
  bool pulling = false;
  bool get busy => pulling;
  HttpClient? _client;
  int _epoch = 0;
  bool _disposed = false;
  Timer? _changed;
  void _emit() {
    if (!_disposed) super.notifyListeners();
  }

  void _notify() {
    if (_disposed) return;
    if (state['state'] == 'receiving' || state['state'] == 'transmitting') {
      _changed ??= Timer(const Duration(milliseconds: 80), () {
        _changed = null;
        _emit();
      });
    } else {
      _changed?.cancel();
      _changed = null;
      _emit();
    }
  }

  AppUpdates() {
    channel.setMethodCallHandler((call) async {
      if (call.method == 'installStatus') {
        final receipt = object(call.arguments);
        // A late installer callback must not erase a different active transfer.
        state = state['transfer_id'] != null || pulling
            ? {...state, 'last_install_receipt': receipt}
            : {'state': 'android_receipt', ...receipt};
        _emit();
      }
    });
  }
  Future<Json> refresh() => rpc('app.upgrade.status', {});
  Future<Json> pickLocal() async {
    if (busy || state['transfer_id'] != null) {
      throw StateError('Finish or discard the current APK first');
    }
    pulling = true;
    _emit();
    try {
      final value = await channel.invokeMethod<dynamic>('pick');
      if (value == null) return state;
      state = object(value);
      return state;
    } finally {
      pulling = false;
      _emit();
    }
  }

  Future<Json> exportInstalled() async {
    final result = await channel.invokeMethod<dynamic>('export');
    return result == null
        ? {'exported': false, 'cancelled': true}
        : object(result);
  }

  Future<Json> install() async {
    if (busy ||
        !{'verified', 'permission_required'}.contains(state['state']) ||
        state['transfer_id'] is! String) {
      throw StateError('Verify an APK before opening Android installation');
    }
    return rpc('app.upgrade.install', {'transfer_id': state['transfer_id']});
  }

  Future<Json> permission() => rpc('app.upgrade.permission', {});
  Future<Json> cleanup() async {
    if (busy) throw StateError('Finish the current transfer before cleanup');
    return rpc('app.upgrade.cleanup', {});
  }

  Future<Json> uninstallSelf({required bool confirmed}) async {
    if (!confirmed || busy || state['transfer_id'] != null) {
      throw StateError(
        'Explicit local confirmation and no staged transfer required',
      );
    }
    return object(
      await channel.invokeMethod<dynamic>('uninstall', {'confirmed': true}),
    );
  }

  Future<Json> abort() async {
    if (busy) throw StateError('Cancel the current transfer first');
    final transfer = state['transfer_id'];
    if (transfer == null) return refresh();
    return rpc('app.upgrade.abort', {'transfer_id': transfer});
  }

  Future<void> cancel() async {
    ++_epoch;
    _client?.close(force: true);
    // Active code owns cleanup of its exact transfer; no write is replayed.
  }

  /// Digest/length-pinned HTTP(S) download, streamed directly into bounded native
  /// private staging. No HTTP redirect, implicit credentials, retry or install.
  Future<Json> download(
    String url, {
    required String sha256,
    required int bytes,
  }) async {
    final uri = validateAppUpdateUrl(url);
    if (bytes < 1 ||
        bytes > maximum ||
        !RegExp(r'^[a-f0-9]{64}$').hasMatch(sha256)) {
      throw const FormatException('Specify the APK size and lowercase SHA-256');
    }
    if (busy || state['transfer_id'] != null) {
      throw StateError('Finish or discard the current APK first');
    }
    final epoch = ++_epoch;
    final client = HttpClient()
      ..findProxy = ((_) => 'DIRECT')
      ..connectionTimeout = const Duration(seconds: 5);
    _client = client;
    pulling = true;
    String? transfer;
    final timeout = Timer(
      const Duration(minutes: 5),
      () => client.close(force: true),
    );
    void current() {
      if (_disposed || epoch != _epoch) {
        throw StateError('APK transfer cancelled');
      }
    }

    try {
      state = {'state': 'verifying', 'bytes': bytes, 'received': 0};
      _emit();
      // Native disk-space preflight happens before fetching/allocating APK data.
      final started = await rpc('app.upgrade.begin', {
        'bytes': bytes,
        'sha256': sha256,
      });
      transfer = started['transfer_id'] as String;
      current();
      final request = await client
          .getUrl(uri)
          .timeout(const Duration(seconds: 5));
      request.followRedirects = false;
      final response = await request.close().timeout(
        const Duration(seconds: 10),
      );
      current();
      if (response.statusCode != 200 ||
          (response.contentLength >= 0 && response.contentLength != bytes)) {
        throw const FormatException(
          'APK HTTP response or length does not match',
        );
      }
      var offset = 0;
      await for (final chunk in response.timeout(const Duration(seconds: 10))) {
        current();
        if (offset + chunk.length > bytes) {
          throw const FormatException('APK exceeds declared length');
        }
        for (var i = 0; i < chunk.length; i += chunkSize) {
          current();
          final end = (i + chunkSize).clamp(0, chunk.length);
          final piece = chunk.sublist(i, end);
          final receipt = await rpc('app.upgrade.write', {
            'transfer_id': transfer,
            'offset': offset,
            'data': base64Encode(piece),
          });
          offset += piece.length;
          if (receipt['received'] != offset) {
            throw StateError('Uncertain APK receipt; not replayed');
          }
        }
      }
      current();
      if (offset != bytes) {
        throw const FormatException('APK ended before declared length');
      }
      final receipt = await rpc('app.upgrade.finish', {
        'transfer_id': transfer,
      });
      transfer = null;
      return receipt;
    } catch (error) {
      if (transfer != null) {
        try {
          await rpc('app.upgrade.abort', {'transfer_id': transfer});
        } on Object {
          /* No retry. */
        }
      }
      state = {'state': 'failed', 'error': '$error'};
      rethrow;
    } finally {
      timeout.cancel();
      if (identical(_client, client)) _client = null;
      client.close(force: true);
      pulling = false;
      _emit();
    }
  }

  Future<Json> rpc(String method, Json params) async {
    if (!methods.contains(method)) {
      throw const FormatException('Unknown package method');
    }
    if (method == 'app.upgrade.pull') {
      return pullPeer('${params['endpoint'] ?? ''}');
    }
    if (method == 'app.upgrade.push') {
      return pushPeer('${params['endpoint'] ?? ''}');
    }
    final operation = method.split('.').last;
    final args = Map<String, dynamic>.of(params);
    if (operation == 'write') {
      final encoded = args.remove('data');
      if (encoded is! String || encoded.length > 21848) {
        throw const FormatException('Invalid APK chunk');
      }
      final bytes = base64Decode(encoded);
      if (bytes.isEmpty || bytes.length > chunkSize) {
        throw const FormatException('APK chunk exceeds 16 KiB');
      }
      args['bytes'] = bytes;
    }
    final result = object(await channel.invokeMethod<dynamic>(operation, args));
    if (operation == 'status' &&
        result['state'] == 'idle' &&
        result['install_status'] is String &&
        (result['install_status'] as String).isNotEmpty) {
      try {
        final receipt = object(jsonDecode(result['install_status']));
        if (receipt['status'] is int) {
          result.clear();
          result.addAll({'state': 'android_receipt', ...receipt});
        }
      } on FormatException {
        /* A malformed receipt is not installation proof. */
      }
    }
    if (operation == 'read') {
      final bytes = result.remove('bytes');
      if (bytes is! Uint8List || bytes.length > chunkSize) {
        throw const FormatException('Invalid APK read response');
      }
      result['data'] = base64Encode(bytes);
    } else if (method.startsWith('app.upgrade.') && operation != 'cleanup') {
      state = result;
      _notify();
    }
    return result;
  }

  Future<Json> pullPeer(
    String address, {
    Map<String, dynamic>? expectedManifest,
  }) async {
    if (pulling || state['transfer_id'] != null) {
      throw StateError('Finish the current APK transfer');
    }
    final origin = peerOrigin(address);
    final epoch = ++_epoch;
    pulling = true;
    final client = HttpClient()
      ..findProxy = ((_) => 'DIRECT')
      ..connectionTimeout = const Duration(seconds: 2);
    _client = client;
    final timeout = Timer(
      const Duration(minutes: 5),
      () => client.close(force: true),
    );
    String? transfer;
    try {
      // Preflight before 0%: source identity, manifest, version, signer, space.
      state = {'state': 'verifying'};
      _emit();
      final health = await boundedPeerGet(client, origin.resolve('/healthz'));
      if (health['status'] != 'ok' ||
          health['service'] != 'pakarcontroller-mobile') {
        throw const FormatException('Peer is not a controller app');
      }
      final remote = await peerRpc(client, origin, 'app.package.info', {});
      final local = await rpc('app.package.info', {});
      if (_disposed || epoch != _epoch) {
        throw StateError('APK transfer cancelled');
      }
      validateAppUpdateManifest(remote, local);
      if (expectedManifest != null) {
        for (final key in ['package', 'version_code', 'bytes', 'sha256']) {
          if (remote[key] != expectedManifest[key]) {
            throw const FormatException('Peer APK changed after discovery');
          }
        }
        final expectedSigners = expectedManifest['signers'];
        if (expectedSigners is! List ||
            !setEquals(
              expectedSigners.toSet(),
              (remote['signers'] as List).toSet(),
            )) {
          throw const FormatException(
            'Peer APK signer changed after discovery',
          );
        }
      }
      final size = remote['bytes'] as int;
      final started = await rpc('app.upgrade.begin', {
        'bytes': size,
        'sha256': remote['sha256'],
      });
      transfer = started['transfer_id'] as String;
      var offset = 0;
      while (offset < size) {
        if (_disposed || epoch != _epoch) {
          throw StateError('APK transfer cancelled');
        }
        final packet = await peerRpc(client, origin, 'app.package.read', {
          'offset': offset,
          'length': chunkSize,
          'sha256': remote['sha256'],
        });
        final encoded = packet['data'];
        if (packet['offset'] != offset ||
            encoded is! String ||
            encoded.length > 21848) {
          throw const FormatException('Peer APK range changed');
        }
        final bytes = base64Decode(encoded);
        if (_disposed || epoch != _epoch) {
          throw StateError('APK transfer cancelled');
        }
        if (bytes.isEmpty ||
            bytes.length > chunkSize ||
            offset + bytes.length > size) {
          throw const FormatException('Peer APK size changed');
        }
        final result = await rpc('app.upgrade.write', {
          'transfer_id': transfer,
          'offset': offset,
          'data': encoded,
        });
        offset += bytes.length;
        if (result['received'] != offset) {
          throw const FormatException(
            'Uncertain APK chunk receipt; not replayed',
          );
        }
      }
      if (_disposed || epoch != _epoch) {
        throw StateError('APK transfer cancelled');
      }
      final result = await rpc('app.upgrade.finish', {'transfer_id': transfer});
      transfer = null; // Keep verified staging until explicit Install or Abort.
      return result;
    } catch (error) {
      if (transfer != null) {
        try {
          await rpc('app.upgrade.abort', {'transfer_id': transfer});
        } on Object {
          /* Never replay installation. */
        }
      }
      state = {'state': 'failed', 'error': '$error'};
      rethrow;
    } finally {
      timeout.cancel();
      if (identical(_client, client)) _client = null;
      pulling = false;
      client.close(force: true);
      _emit();
    }
  }

  /// Peer approval stages bytes only; sender cannot invoke the installer.
  Future<Json> pushPeer(String address) async {
    if (pulling || state['transfer_id'] != null) {
      throw StateError('Finish the current APK transfer');
    }
    final origin = peerOrigin(address);
    final epoch = ++_epoch;
    final client = HttpClient()
      ..findProxy = ((_) => 'DIRECT')
      ..connectionTimeout = const Duration(seconds: 2);
    _client = client;
    final timeout = Timer(
      const Duration(minutes: 5),
      () => client.close(force: true),
    );
    pulling = true;
    String? transfer, capability;
    try {
      state = {'state': 'offering'};
      _emit();
      final health = await boundedPeerGet(client, origin.resolve('/healthz'));
      if (health['status'] != 'ok' ||
          health['service'] != 'pakarcontroller-mobile') {
        throw const FormatException('Peer is not a controller app');
      }
      final manifest = await rpc('app.package.info', {});
      if (_disposed || epoch != _epoch) {
        throw StateError('APK transfer cancelled');
      }
      final receipt = await peerRpc(
        client,
        origin,
        'app.upgrade.offer',
        manifest,
      );
      transfer = receipt['transfer_id'] as String;
      capability = receipt['capability'] as String;
      if (capability.length < 32 ||
          capability.length > 128 ||
          receipt['bytes'] != manifest['bytes'] ||
          receipt['sha256'] != manifest['sha256']) {
        throw const FormatException('Invalid peer transfer receipt');
      }
      final size = manifest['bytes'] as int;
      var offset = 0;
      while (offset < size) {
        if (_disposed || epoch != _epoch) {
          throw StateError('APK transfer cancelled');
        }
        final packet = await rpc('app.package.read', {
          'offset': offset,
          'length': chunkSize,
          'sha256': manifest['sha256'],
        });
        final data = packet['data'] as String;
        if (_disposed || epoch != _epoch) {
          throw StateError('APK transfer cancelled');
        }
        final bytes = base64Decode(data);
        if (packet['offset'] != offset ||
            bytes.isEmpty ||
            bytes.length > chunkSize ||
            offset + bytes.length > size) {
          throw const FormatException('Installed APK changed');
        }
        final result = await peerRpc(client, origin, 'app.upgrade.write', {
          'transfer_id': transfer,
          'offset': offset,
          'data': data,
        }, capability: capability);
        offset += bytes.length;
        if (result['received'] != offset) {
          throw const FormatException(
            'Uncertain peer receipt; upload not replayed',
          );
        }
        state = {
          'state': 'transmitting',
          'bytes': size,
          'sent': offset,
          'endpoint': address,
        };
        _notify();
      }
      if (_disposed || epoch != _epoch) {
        throw StateError('APK transfer cancelled');
      }
      final result = await peerRpc(client, origin, 'app.upgrade.finish', {
        'transfer_id': transfer,
      }, capability: capability);
      transfer = null;
      state = {
        'state': 'delivered',
        'endpoint': address,
        'version_code': manifest['version_code'],
      };
      _emit();
      return result;
    } catch (error) {
      if (transfer != null && capability != null) {
        try {
          await peerRpc(client, origin, 'app.upgrade.abort', {
            'transfer_id': transfer,
          }, capability: capability);
        } on Object {
          /* No blind replay. */
        }
      }
      state = {'state': 'failed', 'error': '$error'};
      rethrow;
    } finally {
      timeout.cancel();
      if (identical(_client, client)) _client = null;
      pulling = false;
      client.close(force: true);
      _emit();
    }
  }

  @override
  void dispose() {
    _disposed = true;
    ++_epoch;
    _client?.close(force: true);
    _changed?.cancel();
    channel.setMethodCallHandler(null);
    super.dispose();
  }
}

Uri peerOrigin(String value) {
  final origin = Uri.tryParse(value);
  if (origin == null ||
      !['http', 'https'].contains(origin.scheme) ||
      origin.host.isEmpty ||
      origin.userInfo.isNotEmpty ||
      origin.hasQuery ||
      origin.hasFragment ||
      (origin.path.isNotEmpty && origin.path != '/')) {
    throw const FormatException('Use a direct peer HTTP(S) origin');
  }
  final ip = InternetAddress.tryParse(origin.host);
  if (ip != null &&
      (ip.type != InternetAddressType.IPv4 ||
          ip.isMulticast ||
          ip.address == '0.0.0.0' ||
          ip.address == '255.255.255.255')) {
    throw const FormatException('Use a unicast peer origin');
  }
  return origin;
}

Future<Json> boundedPeerGet(HttpClient client, Uri uri) async {
  final request = await client.getUrl(uri).timeout(const Duration(seconds: 5));
  request.followRedirects = false;
  final response = await request.close().timeout(const Duration(seconds: 3));
  return readPeerJson(response);
}

Future<Json> readPeerJson(HttpClientResponse response) async {
  if (response.statusCode != 200) {
    throw HttpException(
      'Peer rejected package request (${response.statusCode})',
    );
  }
  final bytes = await readAppUpdateBytes(response);
  return object(jsonDecode(utf8.decode(bytes)));
}

/// Absolute deadline and bounded bytes, not an indefinitely renewable per-chunk
/// timeout. Cancels the subscription when a slow/trickling peer exceeds budget.
Future<List<int>> readAppUpdateBytes(
  Stream<List<int>> stream, {
  int maximum = 32768,
  Duration deadline = const Duration(seconds: 3),
}) async {
  final result = <int>[];
  final timer = Stopwatch()..start();
  final chunks = StreamIterator<List<int>>(stream);
  try {
    while (true) {
      final remaining = deadline - timer.elapsed;
      if (remaining <= Duration.zero) {
        throw TimeoutException('APK RPC deadline expired');
      }
      if (!await chunks.moveNext().timeout(remaining)) break;
      if (result.length + chunks.current.length > maximum) {
        throw const FormatException('APK RPC exceeds its byte limit');
      }
      result.addAll(chunks.current);
    }
    return result;
  } finally {
    await chunks.cancel();
  }
}

Future<Json> peerRpc(
  HttpClient client,
  Uri origin,
  String method,
  Json params, {
  String? capability,
}) async {
  final request = await client
      .postUrl(origin.resolve('/api/rpc'))
      .timeout(const Duration(seconds: 5));
  request.followRedirects = false;
  request.headers.contentType = ContentType.json;
  if (capability != null) {
    request.headers.set('authorization', 'Bearer $capability');
  }
  request.write(
    jsonEncode({'jsonrpc': '2.0', 'id': 1, 'method': method, 'params': params}),
  );
  final result = await readPeerJson(
    await request.close().timeout(const Duration(seconds: 35)),
  );
  if (result['id'] != 1 ||
      result.containsKey('error') ||
      result['result'] is! Map) {
    throw const FormatException('Peer rejected package RPC');
  }
  return object(result['result']);
}

Uri validateAppUpdateUrl(String value) {
  final uri = Uri.tryParse(value);
  if (uri == null ||
      value.length > 2048 ||
      !['http', 'https'].contains(uri.scheme) ||
      uri.host.isEmpty ||
      uri.userInfo.isNotEmpty ||
      uri.hasFragment ||
      RegExp(r'[\x00-\x20\x7f\\]').hasMatch(value)) {
    throw const FormatException(
      'Use a direct HTTP(S) APK URL without embedded credentials',
    );
  }
  // Explicit local endpoints may be HTTP; remote delivery requires HTTPS.
  final ip = InternetAddress.tryParse(uri.host);
  final b = ip?.rawAddress ?? Uint8List(0);
  final local =
      uri.host == 'localhost' ||
      uri.host.endsWith('.local') ||
      (ip != null &&
          (ip.isLoopback ||
              b.length == 4 &&
                  (b[0] == 10 ||
                      b[0] == 192 && b[1] == 168 ||
                      b[0] == 172 && b[1] >= 16 && b[1] <= 31 ||
                      b[0] == 169 && b[1] == 254)));
  if (uri.scheme == 'http' && !local) {
    throw const FormatException('Use HTTPS for a remote APK endpoint');
  }
  return uri;
}

void validateAppUpdateManifest(
  Map<String, dynamic> remote,
  Map<String, dynamic> local,
) {
  List<String> signers(dynamic value) {
    if (value is! List ||
        value.isEmpty ||
        value.length > 8 ||
        value.any(
          (s) => s is! String || !RegExp(r'^[a-f0-9]{64}$').hasMatch(s),
        )) {
      throw const FormatException('APK signer manifest invalid');
    }
    return value.cast<String>().toList()..sort();
  }

  final size = remote['bytes'];
  if (size is! int ||
      size < 1 ||
      size > AppUpdates.maximum ||
      remote['package'] != local['package'] ||
      local['package'] is! String ||
      remote['version_code'] is! int ||
      local['version_code'] is! int ||
      (remote['version_code'] as int) <= (local['version_code'] as int) ||
      !RegExp(r'^[a-f0-9]{64}$').hasMatch('${remote['sha256']}') ||
      jsonEncode(signers(remote['signers'])) !=
          jsonEncode(signers(local['signers']))) {
    throw const FormatException(
      'APK is not a newer package signed by this app signer',
    );
  }
  if (remote.containsKey('minimum_sdk') &&
      (remote['minimum_sdk'] is! int ||
          local['sdk'] is! int ||
          (remote['minimum_sdk'] as int) > (local['sdk'] as int))) {
    throw const FormatException('APK requires a newer Android release');
  }
  if (remote.containsKey('abis')) {
    final abis = remote['abis'];
    final supported = local['device_abis'] ?? local['abis'];
    if (abis is! List ||
        abis.any((a) => a is! String) ||
        supported is! List ||
        abis.isNotEmpty && !abis.any(supported.contains)) {
      throw const FormatException('APK does not support this Android CPU');
    }
  }
}
