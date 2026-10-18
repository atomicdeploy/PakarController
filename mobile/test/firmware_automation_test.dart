import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/firmware.dart';
import 'package:pakarcontroller/firmware_automation.dart';

import 'firmware_test.dart'
    show MaintenanceClient, jsonResponse, firmwareJob, manifestHash;

Map<String, dynamic> automationStatus({
  String board = 'board',
  String boot = 'fixture-boot',
}) => {
  'board_id': board,
  'boot_id': boot,
  'hostname': 'ledboard',
  'command_clock_ms': 1000,
  'capabilities': {
    'board_kind': 'table',
    'hostname': 'ledboard',
    'name': 'Table',
    'acknowledged_features': ['ota', 'isp'],
    'application': {
      'id': 'pakar.table',
      'name': 'Pakar',
      'title': 'Pakar Table',
    },
  },
};

FirmwareAutomationPolicy automationPolicy({
  bool apply = false,
  bool stage = true,
  String board = 'board',
}) => FirmwareAutomationPolicy(
  enabled: true,
  boardId: board,
  applicationId: 'pakar.table',
  catalogUrl: 'https://updates.example/catalog',
  downloadOrigin: 'https://updates.example/',
  autoStage: stage,
  autoApply: apply,
);

Map<String, dynamic> catalogOffer(
  http.BaseRequest request, {
  String hash = manifestHash,
  String kind = 'manifest',
  String target = 'all',
}) => {
  'id': 'offer-one',
  'session': request.headers['X-Pakar-Firmware-Session'],
  'board_id': request.headers['X-Pakar-Board'],
  'boot_id': request.headers['X-Pakar-Boot'],
  'expires_at_ms': DateTime.now().millisecondsSinceEpoch + 30000,
  'url': 'https://updates.example/image.json',
  'sha256': hash,
  'kind': kind,
  'target': target,
};

void main() {
  test('Policy persists exact board/application, not credentials or an active flash', () {
    final raw = automationPolicy(apply: true).toJson();
    final policy = FirmwareAutomationPolicy.parse(raw);
    expect(policy.autoApply, true);
    expect(policy.boardId, 'board');
    expect(
      raw.keys.any((k) => k.contains('token') || k.contains('password')),
      false,
    );
    expect(
      () => const FirmwareAutomationPolicy(enabled: true).validate(),
      throwsFormatException,
    );
    expect(
      () => const FirmwareAutomationPolicy(autoApply: true).validate(),
      throwsFormatException,
    );
  });

  test('Corrupt receipt history disables persisted automatic policy instead of replaying', () async {
    final api = PakarApi();
    final automation = FirmwareAutomation(
      api,
      authorized: (_) => true,
      save: (_) async {},
      initial: {
        'policy': automationPolicy(apply: true).toJson(),
        'consumed': ['ok', null],
      },
    );
    expect(automation.policy.enabled, false);
    expect(automation.running, false);
    expect(automation.error, contains('invalid'));
    automation.dispose();
    await api.close();
  });

  for (final apply in [false, true]) {
    test(
      'Catalog dispatches one exact pinned manifest with apply=$apply, never replays its digest',
      () async {
        var writes = 0, catalogs = 0;
        final snapshots = <Map<String, dynamic>>[];
        final api = PakarApi(
          client: MaintenanceClient((request) async {
            if (request.url.path == '/api/status') {
              return jsonResponse(automationStatus());
            }
            if (request.method == 'GET') return jsonResponse(firmwareJob());
            ++writes;
            final form = (request as http.Request).bodyFields;
            expect(request.url.path, '/api/firmware/fetch');
            expect(form['apply'], '$apply');
            expect(form['targets'], '3');
            expect(form['sha256'], manifestHash);
            expect(form['allow_downgrade'], 'false');
            expect(request.followRedirects, false);
            expect(snapshots.last['consumed'], hasLength(1));
            return jsonResponse(firmwareJob(phase: 'manifest', id: 1));
          }),
        );
        await api.status();
        final client = MaintenanceClient((request) async {
          ++catalogs;
          expect(request.followRedirects, false);
          expect(
            request.headers.keys.any((k) => k.toLowerCase() == 'authorization'),
            false,
          );
          return jsonResponse({
            'offers': [catalogOffer(request)],
          });
        });
        final automation = FirmwareAutomation(
          api,
          authorized: (_) => true,
          save: (value) async => snapshots.add(value),
          catalogClient: client,
        );
        await automation.configure(automationPolicy(apply: apply));
        expect(writes, 1);
        expect(automation.job?.phase, 'manifest');
        expect(automation.uncertain, false);
        await automation.checkNow();
        expect(writes, 1);
        expect(catalogs, 2);
        final saved = automation.toJson();
        automation.dispose();
        final restarted = FirmwareAutomation(
          api,
          authorized: (_) => true,
          save: (_) async {},
          initial: saved,
          catalogClient: client,
        );
        await restarted.reconcile();
        expect(writes, 1);
        restarted.dispose();
        await api.close();
        client.close();
      },
    );
  }

  test(
    'Different selected board never downloads or submits the saved policy',
    () async {
      var catalogs = 0, writes = 0;
      final api = PakarApi(
        client: MaintenanceClient((r) async {
          if (r.url.path == '/api/status') {
            return jsonResponse(automationStatus(board: 'other'));
          }
          ++writes;
          return jsonResponse(firmwareJob());
        }),
      );
      await api.status();
      final client = MaintenanceClient((r) async {
        ++catalogs;
        return jsonResponse(catalogOffer(r));
      });
      final automation = FirmwareAutomation(
        api,
        authorized: (_) => true,
        save: (_) async {},
        catalogClient: client,
      );
      await automation.configure(automationPolicy(apply: true));
      expect(catalogs, 0);
      expect(writes, 0);
      expect(automation.running, false);
      automation.dispose();
      await api.close();
      client.close();
    },
  );

  test('Raw ESP offer requires review even when automatic manifest Apply is enabled', () async {
    var writes = 0;
    final api = PakarApi(
      client: MaintenanceClient((r) async {
        if (r.url.path == '/api/status') {
          return jsonResponse(automationStatus());
        }
        ++writes;
        return jsonResponse(firmwareJob());
      }),
    );
    await api.status();
    final client = MaintenanceClient(
      (r) async => jsonResponse(catalogOffer(r, kind: 'binary', target: 'esp')),
    );
    final automation = FirmwareAutomation(
      api,
      authorized: (_) => true,
      save: (_) async {},
      catalogClient: client,
    );
    await automation.configure(automationPolicy(apply: true));
    expect(automation.pending?.target, 'esp');
    expect(writes, 0);
    await expectLater(automation.reviewPending(), throwsFormatException);
    expect(writes, 0);
    automation.dispose();
    await api.close();
    client.close();
  });

  test(
    'Cached identity cannot start automation when fresh status fails',
    () async {
      var fresh = true, catalogs = 0, writes = 0;
      final api = PakarApi(
        client: MaintenanceClient((r) async {
          if (r.url.path == '/api/status') {
            if (!fresh) throw const FormatException('No current status');
            return jsonResponse(automationStatus());
          }
          ++writes;
          return jsonResponse(firmwareJob());
        }),
      );
      await api.status();
      fresh = false;
      final client = MaintenanceClient((r) async {
        ++catalogs;
        return jsonResponse(catalogOffer(r));
      });
      final automation = FirmwareAutomation(
        api,
        authorized: (_) => true,
        save: (_) async {},
        catalogClient: client,
      );
      await automation.configure(automationPolicy(apply: true));
      expect(automation.running, false);
      expect(catalogs, 0);
      expect(writes, 0);
      automation.dispose();
      await api.close();
      client.close();
    },
  );

  test('Cannot save consumed receipt means zero automatic writes', () async {
    var writes = 0, saves = 0;
    final api = PakarApi(
      client: MaintenanceClient((r) async {
        if (r.url.path == '/api/status') {
          return jsonResponse(automationStatus());
        }
        if (r.method == 'POST') ++writes;
        return jsonResponse(firmwareJob());
      }),
    );
    await api.status();
    final client = MaintenanceClient(
      (r) async => jsonResponse(catalogOffer(r)),
    );
    final automation = FirmwareAutomation(
      api,
      authorized: (_) => true,
      save: (_) async {
        if (++saves > 1) throw const FileSystemException('disk unavailable');
      },
      catalogClient: client,
    );
    await automation.configure(automationPolicy(apply: true));
    expect(writes, 0);
    expect(automation.toJson()['consumed'], isEmpty);
    automation.dispose();
    await api.close();
    client.close();
  });

  test('Lost update ACK persists consumed digest and does not repeat after process restart', () async {
    var writes = 0;
    final api = PakarApi(
      client: MaintenanceClient((r) async {
        if (r.url.path == '/api/status') {
          return jsonResponse(automationStatus());
        }
        if (r.method == 'GET') return jsonResponse(firmwareJob());
        ++writes;
        throw const SocketException('write may have reached board');
      }),
    );
    await api.status();
    final client = MaintenanceClient(
      (r) async => jsonResponse(catalogOffer(r)),
    );
    final automation = FirmwareAutomation(
      api,
      authorized: (_) => true,
      save: (_) async {},
      catalogClient: client,
    );
    await automation.configure(automationPolicy(apply: true));
    expect(writes, 1);
    expect(automation.uncertain, true);
    final saved = automation.toJson();
    automation.dispose();
    final restarted = FirmwareAutomation(
      api,
      authorized: (_) => true,
      save: (_) async {},
      initial: saved,
      catalogClient: client,
    );
    await restarted.reconcile();
    expect(writes, 1);
    restarted.dispose();
    await api.close();
    client.close();
  });

  test('Background stops delivery; foreground receives a new session without replay', () async {
    final sessions = <String>[];
    var writes = 0;
    final api = PakarApi(
      client: MaintenanceClient((r) async {
        if (r.url.path == '/api/status') {
          return jsonResponse(automationStatus());
        }
        if (r.method == 'GET') return jsonResponse(firmwareJob());
        ++writes;
        return jsonResponse(firmwareJob(phase: 'manifest', id: 1));
      }),
    );
    await api.status();
    final client = MaintenanceClient((r) async {
      sessions.add(r.headers['X-Pakar-Firmware-Session']!);
      return jsonResponse(catalogOffer(r));
    });
    final automation = FirmwareAutomation(
      api,
      authorized: (_) => true,
      save: (_) async {},
      catalogClient: client,
    );
    await automation.configure(automationPolicy(apply: true));
    await automation.setForeground(false);
    expect(automation.running, false);
    await automation.checkNow();
    expect(sessions, hasLength(1));
    await automation.setForeground(true);
    expect(sessions, hasLength(2));
    expect(sessions.toSet(), hasLength(2));
    expect(writes, 1);
    automation.dispose();
    await api.close();
    client.close();
  });

  test('AVR maintenance uses actual hash-bound queued ISP/Urboot contract and board clock', () async {
    for (final method in ['isp', 'urboot']) {
      var writes = 0;
      Map<String, dynamic> receipt({bool queued = false}) => {
        'job_id': queued ? 1 : 0,
        'boot_id': 'fixture-boot',
        'request_id': queued ? 'request-one' : '',
        'method': method,
        'operation': queued ? 'program' : '',
        'phase': queued ? 'queued' : 'idle',
        'busy': queued,
        'received_bytes': 0,
        'expected_bytes': 0,
        'verified': false,
        'application_recovered': false,
        'sha256': queued ? manifestHash : '',
        'error': '',
        'methods': ['isp', 'urboot'],
      };
      final api = PakarApi(
        client: MaintenanceClient((r) async {
          if (r.url.path == '/api/status') {
            return jsonResponse(automationStatus());
          }
          expect(r.url.path, '/api/avr/maintenance');
          if (r.method == 'GET') return jsonResponse(receipt());
          ++writes;
          final form = (r as http.Request).bodyFields;
          expect(form, {
            'method': method,
            'operation': 'program',
            'request_id': 'request-one',
            'expires_at_ms': '56000',
            'sha256': manifestHash,
          });
          expect(r.headers['X-Pakar-Board'], 'board');
          expect(r.headers['X-Pakar-Boot'], 'fixture-boot');
          return jsonResponse(receipt(queued: true));
        }),
      );
      await api.status();
      final controller = FirmwareController(api, authorized: (_) => true);
      final queued = await controller.maintainAvr(
        method: method,
        operation: 'program',
        sha256: manifestHash,
        requestId: 'request-one',
      );
      expect(queued.phase, 'queued');
      expect(queued.verified, false);
      expect(queued.complete, false);
      expect(writes, 1);
      await expectLater(
        controller.maintainAvr(method: method, operation: 'program'),
        throwsFormatException,
      );
      expect(writes, 1);
      await api.close();
    }
  });
}
