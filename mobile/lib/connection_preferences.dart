import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:flutter/foundation.dart';

import 'dart:convert';

import 'api.dart';

/// Unsubmitted nonsecret connection fields. Never changes the selected target,
/// grants a connection, or exports the password controller.
class ConnectionDraft extends ChangeNotifier {
  String? endpoint, user;
  void restore(Object? saved) {
    if (saved is! Map) return;
    final host = saved['endpoint'], name = saved['user'];
    endpoint ??= host is String && host.length <= 1024 ? host : null;
    user ??= name is String && name.length <= 96 ? name : null;
  }

  void edit(String host, String name, PakarApi api) {
    final nextHost = host != api.baseUrl && host.length <= 1024 ? host : null;
    final nextUser = name != api.user && name.length <= 96 ? name : null;
    if (nextHost == endpoint && nextUser == user) return;
    endpoint = nextHost;
    user = nextUser;
    notifyListeners();
  }

  void clear() {
    if (endpoint == null && user == null) return;
    endpoint = user = null;
    notifyListeners();
  }

  Map<String, dynamic> snapshot() => {
    if (endpoint != null) 'endpoint': endpoint,
    if (user != null) 'user': user,
  };
}

abstract interface class SecretStore {
  Future<String?> read();
  Future<void> write(String value);
}

class PlatformSecretStore implements SecretStore {
  const PlatformSecretStore();
  static const storage = FlutterSecureStorage();
  static const key = 'pakarcontroller.device_password';
  @override
  Future<String?> read() => storage.read(key: key);
  @override
  Future<void> write(String value) => storage.write(key: key, value: value);
}

class ConnectionPreferences {
  ConnectionPreferences(this.preferences, {SecretStore? secrets})
    : secrets = secrets ?? const PlatformSecretStore();
  final SharedPreferences preferences;
  final SecretStore secrets;

  Future<void> restore(PakarApi api) async {
    api.commandHttpFallback =
        preferences.getBool('command_http_fallback') ?? true;
    api.automaticDiscovery =
        preferences.getBool('automatic_board_connection') ?? true;
    final endpoint = PakarApi.normalizeEndpoint(
      preferences.getString('endpoint') ?? 'http://ledboard.local',
    );
    if (api.connectionMethod != ConnectionMethod.usb &&
        endpoint.startsWith('ble://')) {
      api.connectionMethod = ConnectionMethod.bluetooth;
      api.expectedBluetoothBoard =
          preferences.getString('bluetooth_board_id') ?? '';
    } else if (api.connectionMethod != ConnectionMethod.usb &&
        preferences.getString('connection_method') == 'network') {
      api.connectionMethod = ConnectionMethod.network;
    }
    api.baseUrl = endpoint;
    api.user = preferences.getString('user') ?? 'admin';
    var password = await secrets.read();
    final legacy = preferences.getString('password');
    if (password == null && legacy != null) {
      await secrets.write(
        legacy,
      ); // Keep the old value if secure storage fails.
      password = legacy;
    }
    if (legacy != null && password != null) {
      await preferences.remove('password');
    }
    api.password = password ?? '';
    api.resetTransport();
    try {
      api.restoreNetworkRecovery(
        jsonDecode(preferences.getString('verified_network_route') ?? 'null'),
      );
    } on FormatException {
      /* Corrupt hints never replace hostname intent. */
    }
  }

  Future<void> saveAutomaticDiscovery(bool enabled) async {
    if (!await preferences.setBool('automatic_board_connection', enabled)) {
      throw StateError('Automatic connection preference could not be saved');
    }
  }

  /// Low-write same-board observations only. Never persists unverified UDP or
  /// an arbitrary reply IP, and never rewrites a user's in-progress UI draft.
  Future<void> saveNetworkRecovery(PakarApi api) async {
    final recovery = api.networkRecovery;
    if (recovery == null) return;
    final encoded = jsonEncode(recovery);
    if (preferences.getString('verified_network_route') == encoded) return;
    if (!await preferences.setString('verified_network_route', encoded)) {
      throw StateError('Verified network route could not be saved');
    }
    if (preferences.getString('endpoint') != api.baseUrl) {
      await preferences.setString('endpoint', api.baseUrl);
      await preferences.setString('last_network_endpoint', api.baseUrl);
    }
  }

  /// Local command policy only; do not reconnect or write to a board.
  Future<void> saveCommandHttpFallback(bool enabled) async {
    if (!await preferences.setBool('command_http_fallback', enabled)) {
      throw StateError('Command transport preference could not be saved');
    }
  }

  Future<void> save(
    String endpoint,
    String user,
    String password, {
    String bluetoothBoard = '',
  }) async {
    final normalized = PakarApi.normalizeEndpoint(endpoint);
    if (user.length > 96 || password.length > 256) {
      throw const FormatException('Credentials exceed their limits');
    }
    await secrets.write(password); // Never fall back to plaintext persistence.
    await preferences.setString('endpoint', normalized);
    await preferences.setString(
      'connection_method',
      normalized.startsWith('ble://') ? 'bluetooth' : 'network',
    );
    await preferences.setString(
      normalized.startsWith('ble://')
          ? 'last_bluetooth_endpoint'
          : 'last_network_endpoint',
      normalized,
    );
    if (normalized.startsWith('ble://')) {
      await preferences.setString('bluetooth_board_id', bluetoothBoard);
    }
    await preferences.setString('user', user.trim());
    await preferences.remove('password');
  }
}
