import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

class MappingApi extends PakarApi {
  DeviceStatus observation = DeviceStatus({
    ...peerFixture().json,
    'board_id': 'mapping-board',
    'boot_id': 'a' * 32,
  });
  bool fresh = true;
  bool supersedeNext = false;
  int statusReads = 0, mappingReads = 0;
  Completer<void>? hold;
  Map<String, dynamic> response = {
    'board_id': 'mapping-board',
    'boot_id': 'a' * 32,
    'physical': List.generate(80, (i) => 80 - i),
    'names': List.generate(80, (i) => '${i + 1}'),
  };
  @override
  DeviceStatus? get freshStatus => fresh ? observation : null;
  @override
  Future<DeviceStatus> status() async {
    statusReads++;
    if (supersedeNext) {
      supersedeNext = false;
      throw const SupersededRead();
    }
    fresh = true;
    return observation;
  }

  @override
  Future<Map<String, dynamic>> getJson(String path) async {
    expect(path, '/api/cells/mapping');
    mappingReads++;
    final value = {...response};
    await hold?.future;
    return value;
  }
}

void main() {
  test(
    'fresh producer-bound mapping shares one GET without redundant status',
    () async {
      final api = MappingApi()..hold = Completer<void>();
      addTearDown(api.close);
      final first = api.mapping(), second = api.mapping();
      expect(identical(first, second), true);
      api.hold!.complete();
      expect((await first).physical.first, 80);
      expect(api.mappingReads, 1);
      expect(api.statusReads, 0);
    },
  );

  test(
    'foreign or partial mapping identity is rejected without fallback',
    () async {
      for (final identity in [
        {'board_id': 'another-board', 'boot_id': 'a' * 32},
        {'board_id': 'mapping-board', 'boot_id': 'b' * 32},
        {'board_id': 'mapping-board'},
      ]) {
        final api = MappingApi();
        addTearDown(api.close);
        api.response.remove('board_id');
        api.response.remove('boot_id');
        api.response.addAll(identity);
        await expectLater(api.mapping(), throwsA(isA<StaleConnection>()));
        expect(api.statusReads, 0);
      }
    },
  );

  test(
    'changed live boot or connection retires an in-flight mapping',
    () async {
      for (final replaceConnection in [false, true]) {
        final api = MappingApi()..hold = Completer<void>();
        addTearDown(api.close);
        final result = api.mapping();
        await Future<void>.delayed(Duration.zero);
        if (replaceConnection) {
          api.baseUrl = 'http://other-board.local';
        } else {
          api.observation = DeviceStatus({
            ...api.observation.json,
            'boot_id': 'b' * 32,
          });
        }
        final rejected = expectLater(result, throwsA(isA<StaleConnection>()));
        api.hold!.complete();
        await rejected;
      }
    },
  );

  test('identity omission still requires same-peer status fencing', () async {
    final api = MappingApi();
    addTearDown(api.close);
    api.response.remove('board_id');
    api.response.remove('boot_id');
    expect((await api.mapping()).valid, true);
    expect(api.statusReads, 1);
  });

  test(
    'superseded initial status is settled without replaying map or commands',
    () async {
      final api = MappingApi()
        ..fresh = false
        ..supersedeNext = true;
      addTearDown(api.close);
      expect((await api.mapping()).valid, true);
      expect(api.statusReads, 2);
      expect(api.mappingReads, 1);
    },
  );
}
