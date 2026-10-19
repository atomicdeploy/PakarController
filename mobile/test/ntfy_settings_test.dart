import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/device_settings.dart';

import 'settings_fixture.dart';
import 'peer_fixture.dart';

void main() {
  final configuration = <String, dynamic>{
    ...simulatedCloud,
    'ntfy_enabled': false,
    'ntfy_host': 'ntfy.sh',
    'ntfy_topic': 'fixture-private',
    'ntfy_token_set': false,
  };
  test('ntfy is optional and never invented for an unsupported board', () {
    expect(
      settingsSnapshot(
        SettingsGroup.cloud,
        simulatedCloud,
      ).containsKey('ntfy_enabled'),
      false,
    );
    expect(
      settingsSnapshot(SettingsGroup.cloud, configuration)['ntfy_enabled'],
      false,
    );
    for (final patch in [
      {'ntfy_topic': 'bad/path'},
      {'ntfy_host': 'http://ntfy.sh'},
      {'ntfy_enabled': true, 'ntfy_topic': ''},
      {'ntfy_token_set': null},
    ]) {
      expect(
        () =>
            settingsSnapshot(SettingsGroup.cloud, {...configuration, ...patch}),
        throwsFormatException,
      );
    }
  });
  test('ntfy secrets stay write-only, opt-in uses exact readback, topic is not exported', () async {
    final api = PakarApi(
      client: MockClient(
        (request) async => http.Response(
          jsonEncode(
            request.url.path == '/api/status'
                ? peerFixture().json
                : configuration,
          ),
          200,
        ),
      ),
    );
    final workspace = DeviceSettingsWorkspace();
    final session = DeviceSettingsSession(
      api,
      authorized: () => true,
      workspace: workspace,
    );
    await session.load(SettingsGroup.cloud);
    expect(
      session.write(SettingsGroup.cloud).form.containsKey('ntfy_token'),
      false,
    );
    session.set(SettingsGroup.cloud, 'ntfy_enabled', true);
    session.set(SettingsGroup.cloud, 'ntfy_topic', 'edited-private');
    session.ntfyToken.mode = SecretMode.replace;
    session.ntfyToken.value = 'tk_fixture';
    session.changed();
    final write = session.write(SettingsGroup.cloud);
    expect(write.form['ntfy_enabled'], 'true');
    expect(write.form['ntfy_token'], 'tk_fixture');
    expect(write.expected['ntfy_token_set'], true);
    expect(write.form.containsKey('ntfy_token_set'), false);
    expect(
      () => write.verifyReadback({...write.expected, 'ntfy_enabled': false}),
      throwsFormatException,
    );
    expect(jsonEncode(workspace.snapshot()), isNot(contains('edited-private')));
    expect(jsonEncode(workspace.snapshot()), isNot(contains('tk_fixture')));
    session.clearSecrets();
    expect(session.ntfyToken.mode, SecretMode.keep);
    session.ntfyToken.mode = SecretMode.clear;
    expect(session.write(SettingsGroup.cloud).form['ntfy_token'], '');
    session.dispose();
    workspace.dispose();
    await api.close();
  });
}
