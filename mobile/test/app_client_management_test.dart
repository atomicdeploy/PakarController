import 'dart:async';
import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/app_client_management.dart';
import 'package:pakarcontroller/connection_preferences.dart' show SecretStore;

class Vault implements SecretStore {
  String? value;
  bool fail = false;
  final writes = <String>[];
  @override
  Future<String?> read() async => value;
  @override
  Future<void> write(String next) async {
    if (fail) throw StateError('Fixture secure storage failed');
    value = next;
    writes.add(next);
  }
}

void main() {
  const base = 'https://office.digitalogic.ir/pakar';
  String commandId(String label) => label.codeUnits
      .map((byte) => byte.toRadixString(16).padLeft(2, '0'))
      .join()
      .padRight(32, '0')
      .substring(0, 32);
  Map<String, dynamic> policy() => {
    'management_base': base,
    'management_enabled': true,
    'allow_remote_check': true,
    'allow_remote_cleanup': true,
  };
  Map<String, dynamic> command(String id, String action, {int? expires}) => {
    'id': commandId(id),
    'action': action,
    'expires_at_ms': expires ?? DateTime.now().millisecondsSinceEpoch + 30000,
  };

  test('Enrollment credentials and command receipts stay only in secure store; telemetry is allowlisted', () async {
    final vault = Vault(),
        settings = policy(),
        requests = <Map<String, dynamic>>[];
    var checks = 0, cleanups = 0;
    final service = AppClientManagement(
      secrets: vault,
      policy: () => settings,
      package: () => {
        'version_name': '0.2.8',
        'version_code': 9,
        'sdk': 35,
        'device_abis': ['arm64-v8a'],
        'password': 'must-not-report',
        'endpoint': 'private-host',
      },
      checkUpdate: () async {
        checks++;
      },
      cleanup: () async {
        cleanups++;
      },
      request: (method, uri, bearer, body) async {
        requests.add({'method': method, 'path': uri.path, 'body': body});
        if (uri.path.endsWith('/enroll')) {
          expect(bearer, isNull);
          expect(body!['enrollment_token'], 'e' * 32);
          return {'client_id': body['client_id'], 'token': 't' * 32};
        }
        expect(bearer, 't' * 32);
        if (uri.path.endsWith('/commands')) {
          return {
            'commands': [
              command('check-1', 'check-update'),
              command('check-1', 'check-update'),
              command('clean-1', 'cleanup'),
              command('remove-1', 'uninstall'),
              command('expired', 'cleanup', expires: 1),
              command('exec', 'execute'),
              {...command('invalid', 'cleanup'), 'id': 'A' * 32},
              {...command('invalid', 'cleanup'), 'id': 'short'},
            ],
          };
        }
        if (uri.path.endsWith('/claim')) {
          final secure = jsonDecode(vault.value!) as Map;
          expect(
            secure['claimed'],
            contains(uri.path.split('/').reversed.skip(1).first),
          );
          return {'claimed': true};
        }
        return {'accepted': true};
      },
    );
    addTearDown(service.dispose);
    await service.enroll(base, 'e' * 32, 'requested-client');
    expect(service.enrolled, true);
    expect(vault.value, contains('"token":"${'t' * 32}"'));
    expect(
      vault.value,
      isNot(contains('e' * 32)),
    ); // One-use token is discarded.
    await service.reportAndReceive(
      'verified',
      receivedBytes: 25,
      expectedBytes: 100,
    );
    await service.reportAndReceive('verified');
    expect(checks, 1);
    expect(cleanups, 1);
    expect(
      requests.where((r) => (r['path'] as String).endsWith('/claim')).length,
      2,
    );
    final report =
        requests.firstWhere(
              (r) => (r['path'] as String).endsWith('/report'),
            )['body']
            as Map;
    expect(report['version'], '0.2.8');
    expect(report['build'], 9);
    expect(report['architecture'], 'arm64');
    expect(report['os'], 'android');
    expect(report['foreground'], true);
    expect(report['allow_remote_uninstall'], false);
    expect(report['received_bytes'], 25);
    expect(report['expected_bytes'], 100);
    expect(report.containsKey('sdk'), false);
    expect(report.containsKey('architectures'), false);
    expect(report['uptime_seconds'], isA<int>());
    expect(jsonEncode(report), isNot(contains('must-not-report')));
    expect(report.containsKey('endpoint'), false);
    settings['management_base'] = 'https://other.example/pakar';
    final count = requests.length;
    await service.reportAndReceive('idle');
    expect(requests.length, count); // Never forward old bearer to another base.
  });

  test(
    'A failed or uncertain durable claim is never replayed or executed',
    () async {
      final vault = Vault(), settings = policy();
      var claims = 0, actions = 0;
      final service = AppClientManagement(
        secrets: vault,
        policy: () => settings,
        package: () => {},
        checkUpdate: () async {
          actions++;
        },
        cleanup: () async {
          actions++;
        },
        request: (method, uri, bearer, body) async {
          if (uri.path.endsWith('/enroll')) {
            return {'client_id': body!['client_id'], 'token': 't' * 32};
          }
          if (uri.path.endsWith('/commands')) {
            return {
              'commands': [command('uncertain', 'cleanup')],
            };
          }
          if (uri.path.endsWith('/claim')) {
            claims++;
            throw TimeoutException('Claim receipt lost');
          }
          return {'accepted': true};
        },
      );
      addTearDown(service.dispose);
      await service.enroll(base, 'e' * 32, 'fixture-client-1');
      await expectLater(
        service.reportAndReceive('idle'),
        throwsA(isA<TimeoutException>()),
      );
      await service.reportAndReceive('idle');
      expect(claims, 1);
      expect(actions, 0);
      expect((jsonDecode(vault.value!) as Map)['claimed'], [
        commandId('uncertain'),
      ]);
    },
  );

  test('Secure write failure prevents claim, and foreground retirement prevents execution', () async {
    final vault = Vault(), settings = policy();
    var claims = 0, actions = 0;
    final claimed = Completer<void>(), release = Completer<void>();
    final service = AppClientManagement(
      secrets: vault,
      policy: () => settings,
      package: () => {},
      checkUpdate: () async {
        actions++;
      },
      cleanup: () async {
        actions++;
      },
      request: (method, uri, bearer, body) async {
        if (uri.path.endsWith('/enroll')) {
          return {'client_id': body!['client_id'], 'token': 't' * 32};
        }
        if (uri.path.endsWith('/commands')) {
          return {
            'commands': [command('pause', 'cleanup')],
          };
        }
        if (uri.path.endsWith('/claim')) {
          claims++;
          claimed.complete();
          await release.future;
          return {'claimed': true};
        }
        return {'accepted': true};
      },
    );
    addTearDown(service.dispose);
    await service.enroll(base, 'e' * 32, 'fixture-client-1');
    vault.fail = true;
    await expectLater(service.reportAndReceive('idle'), throwsStateError);
    expect(claims, 0);
    expect(actions, 0);
    vault.fail = false;
    final pending = service.reportAndReceive('idle');
    await claimed.future;
    service.setForeground(false);
    release.complete();
    await pending;
    service.setForeground(true);
    await service.reportAndReceive('idle');
    expect(claims, 1);
    expect(actions, 0);
  });

  test('Local policy blocks remote actions; explicit forget removes secure enrollment', () async {
    final vault = Vault(),
        settings = policy()
          ..['allow_remote_check'] = false
          ..['allow_remote_cleanup'] = false;
    var claims = 0, actions = 0;
    final service = AppClientManagement(
      secrets: vault,
      policy: () => settings,
      package: () => {},
      checkUpdate: () async {
        actions++;
      },
      cleanup: () async {
        actions++;
      },
      request: (method, uri, bearer, body) async {
        if (uri.path.endsWith('/enroll')) {
          return {'client_id': body!['client_id'], 'token': 't' * 32};
        }
        if (uri.path.endsWith('/commands')) {
          return {
            'commands': [
              command('check', 'check-update'),
              command('clean', 'cleanup'),
              command('remove', 'uninstall'),
            ],
          };
        }
        if (uri.path.endsWith('/claim')) {
          claims++;
          return {'claimed': true};
        }
        return {'accepted': true};
      },
    );
    addTearDown(service.dispose);
    await service.enroll(base, 'e' * 32, 'fixture-client-1');
    await service.reportAndReceive('idle');
    expect(claims, 0);
    expect(actions, 0);
    await service.forget();
    expect(vault.value, '');
    expect(service.enrolled, false);
    expect(
      () => AppClientManagement.endpoint(
        'http://office.digitalogic.ir/pakar',
        '/api/clients/report',
      ),
      throwsFormatException,
    );
  });
}
