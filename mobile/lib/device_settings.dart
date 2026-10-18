import 'dart:convert';

import 'package:flutter/foundation.dart';

import 'api.dart';
import 'settings_catalog.dart';

enum SettingsGroup { network, cloud }

extension SettingsGroupContract on SettingsGroup {
  String get path => '/api/$name';
  String get acknowledgment =>
      this == SettingsGroup.network ? 'accepted' : 'updated';
  Map<String, String> get fields =>
      (deviceSettingsCatalog[name] as Map).cast<String, String>();
}

void settingsBounded(String text, int maximum, String label) {
  if (utf8.encode(text).length > maximum ||
      RegExp(r'[\x00-\x1f\x7f-\x9f]').hasMatch(text)) {
    throw FormatException(
      '$label exceeds its UTF-8 limit or contains controls',
    );
  }
}

/// Actual native Network/Cloud validation plus the shared Web field catalog.
/// Required fields never fall back to placeholder defaults or coerced values.
Map<String, Object> settingsSnapshot(
  SettingsGroup group,
  Map<String, dynamic> json,
) {
  final result = <String, Object>{};
  for (final entry in group.fields.entries) {
    final optional = entry.value.startsWith('optional:');
    if (optional && !json.containsKey(entry.key)) continue;
    final type = optional
        ? entry.value.substring('optional:'.length)
        : entry.value;
    final v = json[entry.key];
    final valid = switch (type) {
      'boolean' => v is bool,
      'string' => v is String,
      'port' => v is int && v >= 1 && v <= 65535,
      'interval' => v is int && v >= 1 && v <= 3600,
      _ => false,
    };
    if (!valid) throw FormatException('Missing or invalid ${entry.key}');
    result[entry.key] = v as Object;
  }
  if (group == SettingsGroup.network) {
    settingsBounded(result['station_ssid'] as String, 32, 'SSID');
    if (result['station_enabled'] == false && result['ap_enabled'] == false) {
      throw const FormatException('Keep station or recovery AP enabled');
    }
    if (result['station_enabled'] == true && result['station_ssid'] == '') {
      throw const FormatException('Enter the station SSID');
    }
    settingsValidateAddressing(result);
  } else {
    for (final prefix in ['mqtt', 'websocket']) {
      final host = result['${prefix}_host'] as String;
      settingsBounded(host, 63, 'Host');
      if ((result['${prefix}_enabled'] == true && host.isEmpty) ||
          !RegExp(r'^[A-Za-z0-9.-]*$').hasMatch(host)) {
        throw const FormatException('Enter a hostname without scheme or path');
      }
    }
    for (final entry in {
      'mqtt_user': 32,
      'mqtt_base_topic': 64,
      'websocket_path': 96,
    }.entries) {
      final text = result[entry.key] as String;
      settingsBounded(text, entry.value, entry.key);
      if (text.contains('"') || text.contains(r'\')) {
        throw const FormatException(
          'Current public settings do not support quotes or backslashes',
        );
      }
    }
    if ((result['mqtt_base_topic'] as String).isEmpty ||
        RegExp('[+#]').hasMatch(result['mqtt_base_topic'] as String)) {
      throw const FormatException(
        'MQTT topic must be nonempty without wildcards',
      );
    }
    if (!(result['websocket_path'] as String).startsWith('/')) {
      throw const FormatException('WebSocket path must start with /');
    }
  }
  return result;
}

/// Actual Web settingsValidateAddressing/native Addressing::validate contract:
/// older peers may omit the whole optional group, never fabricate its values.
void settingsValidateAddressing(Map<String, Object> value) {
  const fields = ['dhcp', 'address', 'gateway', 'subnet', 'dns1', 'dns2'];
  if (!fields.any(value.containsKey)) return;
  if (!fields.every(value.containsKey)) {
    throw const FormatException('Incomplete device addressing settings');
  }
  int ipv4(String text) {
    if (!RegExp(r'^\d{1,3}(\.\d{1,3}){3}$').hasMatch(text)) {
      throw const FormatException('Enter IPv4 address values');
    }
    final parts = text.split('.').map(int.parse).toList();
    if (parts.any((part) => part > 255)) {
      throw const FormatException('Enter IPv4 address values');
    }
    return parts.fold(0, (address, octet) => address * 256 + octet);
  }

  final values = <int>[];
  for (final key in fields.skip(1)) {
    final text = value[key] as String;
    values.add(key.startsWith('dns') && text.isEmpty ? 0 : ipv4(text));
  }
  if (value['dhcp'] == true) return;
  final address = values[0], gateway = values[1], subnet = values[2];
  final host = (~subnet) & 0xffffffff;
  if (host < 3 ||
      host == 0xffffffff ||
      (host & (host + 1)) != 0 ||
      address >> 24 == 0 ||
      address >> 24 >= 224 ||
      (address & host) == 0 ||
      (address & host) == host ||
      gateway >> 24 == 0 ||
      gateway >> 24 >= 224 ||
      (gateway & host) == 0 ||
      (gateway & host) == host ||
      (address & subnet) != (gateway & subnet) ||
      address == gateway ||
      values[3] >> 24 >= 224 ||
      values[4] >> 24 >= 224) {
    throw const FormatException(
      'Invalid static subnet, host, gateway or DNS address',
    );
  }
}

enum SecretMode { keep, replace, clear }

class SecretEdit {
  SecretMode mode = SecretMode.keep;
  String value = '';
  String? wireValue() {
    if (mode == SecretMode.keep) return null;
    if (mode == SecretMode.clear) return '';
    settingsBounded(value, 64, 'Secret');
    if (value.isEmpty) {
      throw const FormatException(
        'Enter a replacement secret or explicitly choose Clear',
      );
    }
    return value;
  }

  bool presence(bool saved) =>
      mode == SecretMode.keep ? saved : mode == SecretMode.replace;
  void clear() {
    mode = SecretMode.keep;
    value = '';
  }
}

/// Adapted actual desktop Draft.observe; dirty drafts are not overwritten.
class SettingsDraft {
  SettingsDraft(Map<String, Object> initial)
    : saved = Map.of(initial),
      value = Map.of(initial);
  Map<String, Object> saved, value;
  bool conflict = false;
  bool get dirty => !mapEquals(saved, value);
  void observe(Map<String, Object> next, {bool secretsDirty = false}) {
    if (!dirty && !secretsDirty) {
      saved = Map.of(next);
      value = Map.of(next);
    } else if (!mapEquals(saved, next)) {
      conflict = true;
    }
  }
}

/// User edits only. Fresh settings must be read before these patches can be
/// displayed in an editor, and their original values are conflict baselines,
/// never cached live authority. Secret values and secret-clear intent are not
/// exported. The shell's existing ordered workspace writer owns persistence.
class DeviceSettingsWorkspace extends ChangeNotifier {
  DeviceSettingsWorkspace({Map<String, dynamic> saved = const {}}) {
    final pages = (deviceSettingsCatalog['pages'] as List).cast<Map>();
    if (pages.any((p) => p['id'] == saved['page'])) {
      _page = saved['page'] as String;
    }
    final find = saved['search'];
    _search = find is String && find.length <= 256 ? find : '';
    final offset = saved['vertical'];
    _vertical = offset is num && offset.isFinite && offset >= 0
        ? offset.toDouble().clamp(0, 100000)
        : 0;
    final edits = saved['edits'];
    if (edits is Map) {
      for (final group in SettingsGroup.values) {
        final fields = edits[group.name];
        if (fields is! Map) continue;
        final patch = <String, Map<String, Object>>{};
        for (final key in group.fields.keys.where(
          (key) => _public(group, key),
        )) {
          final edit = fields[key];
          if (edit is Map &&
              _scalar(edit['before']) &&
              _scalar(edit['after'])) {
            patch[key] = {
              'before': edit['before'] as Object,
              'after': edit['after'] as Object,
            };
          }
        }
        if (patch.isNotEmpty) _edits[group] = patch;
      }
    }
    final identity = saved['identity'];
    if (identity is Map &&
        ['before_host', 'before_name', 'host', 'name'].every(
          (key) =>
              identity[key] is String && (identity[key] as String).length <= 63,
        )) {
      _identity = {
        for (final key in ['before_host', 'before_name', 'host', 'name'])
          key: identity[key] as String,
      };
    }
  }
  static bool _scalar(Object? value) =>
      value is bool || value is int || value is String && value.length <= 512;
  static bool _public(SettingsGroup group, String key) =>
      group.fields.containsKey(key) &&
      !key.contains('password') &&
      !key.contains('token');
  final _edits = <SettingsGroup, Map<String, Map<String, Object>>>{};
  bool _retired = false;
  Map<String, String>? _identity;
  String _page = 'overview', _search = '';
  double _vertical = 0;
  String get page => _page;
  set page(String value) {
    if (value == _page ||
        !(deviceSettingsCatalog['pages'] as List).cast<Map>().any(
          (p) => p['id'] == value,
        )) {
      return;
    }
    _page = value;
    notifyListeners();
  }

  String get search => _search;
  set search(String value) {
    if (value == _search || value.length > 256) return;
    _search = value;
    notifyListeners();
  }

  double get vertical => _vertical;
  set vertical(double value) {
    if (!value.isFinite || value < 0 || value == _vertical) return;
    _vertical = value.clamp(0, 100000);
    notifyListeners();
  }

  void capture(SettingsGroup group, SettingsDraft draft) {
    final patch = <String, Map<String, Object>>{};
    for (final key in group.fields.keys.where((key) => _public(group, key))) {
      final before = draft.saved[key], after = draft.value[key];
      if (before != after && _scalar(before) && _scalar(after)) {
        patch[key] = {'before': before!, 'after': after!};
      }
    }
    if (jsonEncode(_edits[group]) == jsonEncode(patch.isEmpty ? null : patch)) {
      return;
    }
    if (patch.isEmpty) {
      _edits.remove(group);
    } else {
      _edits[group] = patch;
    }
    notifyListeners();
  }

  SettingsDraft restore(SettingsGroup group, Map<String, Object> actual) {
    final draft = SettingsDraft(actual);
    for (final entry in (_edits[group] ?? {}).entries) {
      if (!actual.containsKey(entry.key)) continue;
      if (actual[entry.key] != entry.value['before']) draft.conflict = true;
      draft.value[entry.key] = entry.value['after']!;
      // Retain the original baseline so a conflict is never silently blessed
      // by another refresh, reconnect or process restart.
      draft.saved[entry.key] = entry.value['before']!;
    }
    return draft;
  }

  void discard(SettingsGroup group) {
    if (_edits.remove(group) != null) notifyListeners();
  }

  Map<String, String>? get identity =>
      _identity == null ? null : Map.unmodifiable(_identity!);
  void captureIdentity(
    String beforeHost,
    String beforeName,
    String host,
    String name,
  ) {
    if ([beforeHost, beforeName, host, name].any((v) => v.length > 63)) return;
    final next = beforeHost == host && beforeName == name
        ? null
        : {
            'before_host': beforeHost,
            'before_name': beforeName,
            'host': host,
            'name': name,
          };
    if (mapEquals(_identity, next)) return;
    _identity = next;
    notifyListeners();
  }

  void discardIdentity() {
    // A verified rename ACK may arrive after its old settings view closed.
    // Retiring that view's optional cache must not prevent the owning root
    // from following the ACK; transport revision checks still own handoff.
    if (_retired || _identity == null) return;
    _identity = null;
    notifyListeners();
  }

  @override
  void dispose() {
    _retired = true;
    super.dispose();
  }

  Map<String, dynamic> snapshot() => {
    'page': page,
    'search': search,
    'vertical': vertical,
    'edits': {for (final e in _edits.entries) e.key.name: e.value},
    if (_identity != null) 'identity': _identity,
  };
}

class SettingsWrite {
  SettingsWrite(this.group, this.form, this.expected);
  final SettingsGroup group;
  final Map<String, String> form;
  final Map<String, Object> expected;
  void verifyAck(Map<String, dynamic> json) {
    if (json[group.acknowledgment] != true) {
      throw const FormatException(
        'Device did not confirm a saved configuration write',
      );
    }
  }

  void verifyReadback(Map<String, Object> actual) {
    if (!mapEquals(expected, actual)) {
      throw const FormatException(
        'Write accepted but readback differs. Refresh before deciding to retry; no write replayed.',
      );
    }
  }
}

/// Selected-peer owner and request epochs adapt actual Web settingsSave and
/// native Editor pending/finish. Only optional public draft patches leave this
/// session; secrets, live ownership, acknowledgments and pending writes do not.
class DeviceSettingsSession extends ChangeNotifier {
  DeviceSettingsSession(this.api, {required this.authorized, this.workspace})
    : owner = api.connectionRevision;
  final PakarApi api;
  final bool Function() authorized;
  final DeviceSettingsWorkspace? workspace;
  final int owner;
  final drafts = <SettingsGroup, SettingsDraft>{};
  final _reads = <SettingsGroup, int>{};
  final mqttPassword = SecretEdit(), websocketToken = SecretEdit();
  String wifiPassword = '',
      message = 'Read the actual saved device settings first';
  bool openNetwork = false, pending = false, disposed = false;
  int epoch = 0;
  bool get live => !disposed && owner == api.connectionRevision && authorized();
  bool secretsDirty(SettingsGroup group) => group == SettingsGroup.network
      ? wifiPassword.isNotEmpty || openNetwork
      : mqttPassword.mode != SecretMode.keep ||
            websocketToken.mode != SecretMode.keep;
  bool get dirty =>
      drafts.entries.any((e) => e.value.dirty || secretsDirty(e.key));
  void check() {
    if (!live) throw const StaleConnection();
  }

  void changed() {
    if (!disposed) {
      for (final entry in drafts.entries) {
        workspace?.capture(entry.key, entry.value);
      }
      notifyListeners();
    }
  }

  void set(SettingsGroup group, String key, Object value) {
    check();
    if (pending) return;
    drafts[group]?.value[key] = value;
    changed();
  }

  void clearSecrets() {
    wifiPassword = '';
    openNetwork = false;
    mqttPassword.clear();
    websocketToken.clear();
  }

  void invalidate() {
    ++epoch;
    drafts.clear();
    clearSecrets();
    pending = false;
    message = 'Device context lost; read this peer again';
    changed();
  }

  Future<void> load(SettingsGroup group, {bool discard = false}) async {
    check();
    if (pending) return;
    final request = (_reads[group] ?? 0) + 1, captured = epoch;
    _reads[group] = request;
    try {
      final next = settingsSnapshot(group, await api.getJson(group.path));
      check();
      if (captured != epoch || request != _reads[group] || pending) return;
      if (discard || drafts[group] == null) {
        if (discard) workspace?.discard(group);
        drafts[group] = workspace?.restore(group, next) ?? SettingsDraft(next);
        if (group == SettingsGroup.network) {
          wifiPassword = '';
          openNetwork = false;
        } else {
          mqttPassword.clear();
          websocketToken.clear();
        }
      } else {
        drafts[group]!.observe(next, secretsDirty: secretsDirty(group));
      }
      message = drafts[group]!.conflict
          ? 'Another client changed these settings. Draft retained; discard and reload before saving.'
          : 'Observed saved public device settings';
    } catch (error) {
      if (live && captured == epoch && request == _reads[group]) {
        message = error.toString();
      }
    }
    changed();
  }

  SettingsWrite write(SettingsGroup group) {
    check();
    final draft = drafts[group];
    if (draft == null || draft.conflict) {
      throw const FormatException(
        'Reload the current saved settings before saving',
      );
    }
    final expected = settingsSnapshot(group, draft.value);
    final form = <String, String>{
      for (final entry in expected.entries) entry.key: '${entry.value}',
    };
    if (group == SettingsGroup.network) {
      settingsBounded(wifiPassword, 63, 'Wi-Fi password');
      if (expected['station_enabled'] == true &&
          !openNetwork &&
          utf8.encode(wifiPassword).length < 8) {
        throw const FormatException(
          'Enter an 8–63 byte Wi-Fi password or explicitly select an open network',
        );
      }
      form['ssid'] = form.remove('station_ssid')!;
      form['password'] = openNetwork ? '' : wifiPassword;
    } else {
      for (final entry in {
        'mqtt_password': mqttPassword,
        'websocket_token': websocketToken,
      }.entries) {
        final key = '${entry.key}_set', value = entry.value.wireValue();
        if (value != null) form[entry.key] = value;
        expected[key] = entry.value.presence(expected[key] as bool);
        form.remove(key);
      }
    }
    return SettingsWrite(group, form, expected);
  }

  Future<bool> save(SettingsGroup group) async {
    if (pending) return false;
    late final SettingsWrite request;
    try {
      request = write(group);
    } catch (error) {
      message = error.toString();
      changed();
      return false;
    }
    final captured = ++epoch;
    pending = true;
    message = 'Waiting for acknowledgment and saved-settings readback…';
    changed();
    try {
      final current = settingsSnapshot(group, await api.getJson(group.path));
      check();
      if (captured != epoch) throw const StaleConnection();
      if (!mapEquals(current, drafts[group]!.saved)) {
        drafts[group]!.conflict = true;
        throw const FormatException(
          'Another client changed these settings. Discard and reload before saving.',
        );
      }
      final ack = await api.postJson(
        group.path,
        request.form,
        authorize: () => live && captured == epoch,
      );
      check();
      if (captured != epoch) throw const StaleConnection();
      request.verifyAck(ack);
      final actual = settingsSnapshot(group, await api.getJson(group.path));
      check();
      if (captured != epoch) throw const StaleConnection();
      request.verifyReadback(actual);
      drafts[group] = SettingsDraft(actual);
      if (group == SettingsGroup.network) {
        wifiPassword = '';
        openNetwork = false;
      } else {
        mqttPassword.clear();
        websocketToken.clear();
      }
      message = 'Saved public settings verified by device readback. Secrets remain write-only.';
      return true;
    } catch (error) {
      if (live && captured == epoch) {
        message = '$error Uncertain writes are not retried.';
      }
      return false;
    } finally {
      if (!disposed && captured == epoch) {
        pending = false;
        changed();
      }
    }
  }

  @override
  void dispose() {
    ++epoch;
    disposed = true;
    drafts.clear();
    clearSecrets();
    super.dispose();
  }
}
