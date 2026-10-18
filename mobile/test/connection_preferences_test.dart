import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/connection_preferences.dart';

class TestSecrets implements SecretStore {
  String? value;
  bool fail = false;
  @override
  Future<String?> read() async => value;
  @override
  Future<void> write(String value) async {
    if (fail) throw StateError('locked');
    this.value = value;
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  test('Plaintext is removed only after secure migration succeeds', () async {
    SharedPreferences.setMockInitialValues({'password': 'fixture-password'});
    final prefs = await SharedPreferences.getInstance();
    final secrets = TestSecrets()..fail = true;
    final settings = ConnectionPreferences(prefs, secrets: secrets);
    final api = PakarApi();
    await expectLater(settings.restore(api), throwsStateError);
    expect(prefs.getString('password'), 'fixture-password');
    secrets.fail = false;
    await settings.restore(api);
    expect(prefs.getString('password'), isNull);
    expect(secrets.value, 'fixture-password');
    expect(api.baseUrl, 'http://ledboard.local');
    await api.close();
  });
  test('Secure storage failure never falls back to plaintext', () async {
    SharedPreferences.setMockInitialValues({});
    final prefs = await SharedPreferences.getInstance();
    final secrets = TestSecrets()..fail = true;
    final settings = ConnectionPreferences(prefs, secrets: secrets);
    await expectLater(
      settings.save('ledboard.local', 'admin', 'fixture-password'),
      throwsStateError,
    );
    expect(prefs.getString('password'), isNull);
    expect(prefs.getString('endpoint'), isNull);
    secrets.fail = false;
    await settings.save('renamed.local', 'editor', 'fixture-password');
    expect(prefs.getString('endpoint'), 'http://renamed.local');
    expect(prefs.getString('password'), isNull);
  });
  test('Empty credentials are optional and restore without default-password guesses', () async {
    SharedPreferences.setMockInitialValues({});
    final prefs = await SharedPreferences.getInstance(),
        secrets = TestSecrets();
    final settings = ConnectionPreferences(prefs, secrets: secrets),
        api = PakarApi();
    await settings.restore(api);
    expect(api.password, '');
    await settings.save('ledboard.local', '', '');
    await settings.restore(api);
    expect(api.user, '');
    expect(api.password, '');
    await api.close();
  });
}
