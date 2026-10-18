import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/firmware_progress.dart';
import 'package:pakarcontroller/models.dart';

Map<String, dynamic> upgradeFixture({int revision = 1}) => {
  'board_id': 'fixture-board',
  'boot_id': 'fixture-boot',
  'source_boot_id': 'fixture-boot',
  'source': 'remote-firmware',
  'operation_id': 7,
  'revision': revision,
  'phase': 'program_esp',
  'active': true,
  'received_bytes': 25,
  'expected_bytes': 100,
  'progress_known': true,
  'progress_percent': 25,
  'completion_verified': false,
  'error': '',
};
Map<String, dynamic> progressPeer({Map<String, dynamic>? upgrade}) => {
  'hostname': 'ledboard',
  'board_id': 'fixture-board',
  'boot_id': 'fixture-boot',
  'cells': ['red', 'white'],
  'selected_cell': 2,
  'upgrade': ?upgrade,
};

void main() {
  test('Same-peer lifecycle takes precedence and only measured bytes make a fraction', () {
    final raw = upgradeFixture();
    final status = DeviceStatus(progressPeer(upgrade: raw));
    final progress = FirmwareProgress.observed(status)!;
    expect(progress.fraction, .25);
    expect(progress.source, 'Board update #7');
    expect(progress.verified, false);
    raw['received_bytes'] = 1;
    raw['expected_bytes'] = 3;
    raw['progress_percent'] = 33;
    expect(FirmwareProgress.observed(status)!.fraction, closeTo(1 / 3, 1e-9));
    raw['received_bytes'] = 25;
    raw['expected_bytes'] = 100;
    raw['progress_known'] = false;
    raw['progress_percent'] = null;
    expect(FirmwareProgress.observed(status)!.fraction, isNull);
    raw['progress_known'] = true;
    raw['progress_percent'] = 99;
    expect(FirmwareProgress.observed(status)!.fraction, isNull);
    raw['progress_percent'] = 25;
    raw['phase'] = 'complete';
    raw['active'] = false;
    expect(FirmwareProgress.observed(status)!.verified, false);
    raw['completion_verified'] = true;
    expect(FirmwareProgress.observed(status)!.verified, true);
    raw['active'] = true;
    expect(FirmwareProgress.observed(status)!.verified, false);
  });

  test(
    'Lifecycle must name the current board and boot, not source boot ownership',
    () {
      final raw = upgradeFixture();
      final status = DeviceStatus(progressPeer(upgrade: raw));
      raw['board_id'] = 'another-board';
      expect(FirmwareProgress.observed(status), isNull);
      raw['board_id'] = 'fixture-board';
      raw['boot_id'] = 'old-boot';
      expect(FirmwareProgress.observed(status), isNull);
      raw['boot_id'] = 'fixture-boot';
      raw['source_boot_id'] = 'previous-upload-boot';
      raw['source'] = 'local-ota';
      raw['phase'] = 'complete';
      raw['active'] = false;
      raw['completion_verified'] = true;
      expect(FirmwareProgress.observed(status)!.verified, true);
      raw['operation_id'] = 0;
      expect(FirmwareProgress.observed(status), isNull);
    },
  );

  test('Malformed optional lifecycle does not invent progress or a verified finish', () {
    for (final patch in <Map<String, dynamic>>[
      {'revision': -1},
      {'active': 1},
      {'progress_known': 'true'},
      {'received_bytes': -1},
      {'received_bytes': 101},
      {'expected_bytes': '100'},
      {'completion_verified': 1},
      {'source_boot_id': ''},
      {'phase': 'bad\nphase'},
    ]) {
      expect(
        FirmwareProgress.observed(
          DeviceStatus(progressPeer(upgrade: {...upgradeFixture(), ...patch})),
        ),
        isNull,
      );
    }
    expect(FirmwareProgress.observed(DeviceStatus(progressPeer())), isNull);
  });

  test('Incremental lifecycle preserves cells and rejects duplicate/out-of-order/cross-peer events', () {
    final initial = DeviceStatus(
      progressPeer(upgrade: upgradeFixture(revision: 2)),
    );
    final next = initial.withUpgradeEvent({
      ...upgradeFixture(revision: 3),
      'type': 'update-progress',
      'phase': 'verify_esp',
    })!;
    expect(next.upgrade!.phase, 'verify_esp');
    expect(next.cells, ['red', 'white']);
    expect(next.selectedCell, 2);
    expect(initial.upgrade!.revision, 2);
    for (final event in [
      {...upgradeFixture(revision: 3), 'type': 'update-progress'},
      {...upgradeFixture(revision: 1), 'type': 'update-progress'},
      {
        ...upgradeFixture(revision: 4),
        'type': 'update-progress',
        'boot_id': 'other',
      },
      {
        ...upgradeFixture(revision: 4),
        'type': 'update-progress',
        'board_id': 'other',
      },
    ]) {
      expect(next.withUpgradeEvent(event), isNull);
    }
    final olderHttp = DeviceStatus(progressPeer(upgrade: upgradeFixture()));
    expect(olderHttp.preservingNewerUpgrade(next).upgrade!.revision, 3);
    expect(
      DeviceStatus(progressPeer())
          .preservingNewerUpgrade(next)
          .upgrade!
          .revision,
      3,
    );
    final otherBoot = DeviceStatus({...progressPeer(), 'boot_id': 'new-boot'});
    expect(otherBoot.preservingNewerUpgrade(next).upgrade, isNull);
  });
}
