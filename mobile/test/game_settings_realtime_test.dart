import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/bridge.dart';
import 'package:pakarcontroller/command_readback.dart';
import 'package:pakarcontroller/command_wire.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/peer_identity.dart';

import 'peer_fixture.dart';

Map<String, dynamic> gameState(Map<String, dynamic> settings) {
  final base = peerFixture(
    hostname: 'keycontroller',
    kind: 'keys',
    features: const [
      'command-ack',
      'reset-ack',
      'key-settings',
      'game-melodies',
      'leds',
      'inputs',
      'outputs',
    ],
  );
  return {
    ...base.json,
    'board_id': 'keycontroller-board',
    'boot_id': 'a' * 32,
    'command_clock_ms': 1000,
    'mode': 'first-wins',
    'pixels': 216,
    'winner': 0,
    'avr_online': true,
    'key_mask_applied': true,
    'key_game': settings,
    'capabilities': {
      ...(base.json['capabilities'] as Map),
      'role': 'first-wins',
      'keys': {'count': 9, 'groups_per_key': 2, 'pixels_per_group': 12},
    },
  };
}

void main() {
  test('melody ACK waits for AVR event without replaying the command', () async {
    final state=gameState({'winner_cue':'winner','reset_cue':'welcome'})
      ..['game_audio_applied']=true;
    final server=await HttpServer.bind(InternetAddress.loopbackIPv4,0);
    final api=PakarApi(baseUrl:'http://127.0.0.1:${server.port}')..commandHttpFallback=false;
    final sockets=<WebSocket>[];
    StreamSubscription<DeviceStatus>? stream;
    var mutations=0;
    final listener=server.listen((request) async {
      if(request.uri.path=='/ws') {
        final socket=await WebSocketTransformer.upgrade(request);sockets.add(socket);
        socket.add(jsonEncode(state));
        socket.listen((raw) {
          final packet=jsonDecode(raw as String) as Map<String,dynamic>;
          if(packet['type']=='status') socket.add(jsonEncode(state));
          if(packet['type']=='command') {
            mutations++;
            (state['key_game'] as Map)['winner_cue']=packet['winner_cue'];
            state['game_audio_applied']=false;
            socket.add(jsonEncode({'type':'command-result','request_id':packet['request_id'],
              'board_id':state['board_id'],'boot_id':state['boot_id'],'accepted':true,'status':state}));
            Future<void>.delayed(const Duration(milliseconds:150),() {
              state['game_audio_applied']=true;socket.add(jsonEncode(state));
            });
          }
        });
      } else {request.response.headers.contentType=ContentType.json;request.response.write(jsonEncode(state));await request.response.close();}
    });
    try {
      await api.status();
      final ready=Completer<void>();
      stream=api.liveStatus().listen((_) {if(!ready.isCompleted)ready.complete();});
        await ready.future.timeout(const Duration(seconds:2));
        final result=await api.fastCommand('/api/game/settings',{'winner_cue':'attention'},authorize:()=>true);
        expect(result?.json['game_audio_applied'],true);expect(mutations,1);
        expect(result?.json['key_game']['winner_cue'],'attention');
    } finally {await api.close();await stream?.cancel();for(final socket in sockets){await socket.close();}await listener.cancel();await server.close(force:true);}
  });
  test('game melodies require capability and AVR-applied receipt', () {
    final state=gameState({'winner_cue':'attention','reset_cue':'silent'});
    const values={'winner_cue':'attention','reset_cue':'silent'};
    final packet=jsonDecode(commandPacket(DeviceStatus(state),'/api/game/settings',values));
    expect(packet['winner_cue'],'attention');expect(packet['reset_cue'],'silent');
    expect(commandReadbackMatches(DeviceStatus(state),'/api/game/settings',values),false);
    state['game_audio_applied']=true;
    expect(commandReadbackMatches(DeviceStatus(state),'/api/game/settings',values),true);
    expect(()=>commandPacket(DeviceStatus(state),'/api/game/settings',{'winner_cue':'garbage'}),throwsFormatException);
    final caps=state['capabilities'] as Map;
    caps['acknowledged_features']=(caps['acknowledged_features'] as List)
        .where((feature)=>feature!='game-melodies').toList();
    expect(()=>commandPacket(DeviceStatus(state),'/api/game/settings',values),throwsFormatException);
  });
  test(
    'game settings use typed, bounded, correlated WebSocket commands',
    () async {
      final settings = <String, dynamic>{
        'enabled_mask': 3,
        'idle_rgb': 0xffffff,
        'win_rgb': 0x00ff00,
        'lose_rgb': 0xff0000,
        'extended_effects': false,
        'auto_reset_ms': 0,
      };
      final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      final api = PakarApi(baseUrl: 'http://127.0.0.1:${server.port}')
        ..commandHttpFallback = false;
      final sockets = <WebSocket>[];
      final commands = <Map<String, dynamic>>[];
      var httpCommands = 0;
      final listener = server.listen((request) async {
        if (request.uri.path == '/ws') {
          final socket = await WebSocketTransformer.upgrade(request);
          sockets.add(socket);
          socket.add(jsonEncode(gameState(settings)));
          socket.listen((message) {
            final packet =
                jsonDecode(message as String) as Map<String, dynamic>;
            if (packet['type'] == 'status') {
              socket.add(jsonEncode(gameState(settings)));
            } else if (packet['type'] == 'command') {
              commands.add(packet);
              settings.addAll({
                for (final key in settings.keys)
                  if (packet.containsKey(key)) key: packet[key],
              });
              socket.add(
                jsonEncode({
                  'type': 'command-result',
                  'request_id': packet['request_id'],
                  'board_id': packet['board_id'],
                  'boot_id': packet['boot_id'],
                  'accepted': true,
                  'status': gameState(settings),
                }),
              );
            }
          });
        } else if (request.uri.path == '/api/command') {
          httpCommands++;
          request.response.statusCode = 500;
          await request.response.close();
        } else {
          request.response.headers.contentType = ContentType.json;
          request.response.write(jsonEncode(gameState(settings)));
          await request.response.close();
        }
      });
      StreamSubscription<dynamic>? live;
      AndroidBridge? bridge;
      HttpClient? bridgeClient;
      try {
        await api.status();
        final ready = Completer<void>();
        live = api.liveStatus().listen((_) {
          if (!ready.isCompleted) ready.complete();
        });
        await ready.future.timeout(const Duration(seconds: 2));
        final actual = await api
            .fastCommand('/api/game/settings', {
              'enabled_mask': '15',
              'win_rgb': '${0x123456}',
              'extended_effects': 'true',
              'auto_reset_ms': '10000',
            }, authorize: () => true)
            .timeout(const Duration(seconds: 2));
        expect(actual?.json['key_game']['enabled_mask'], 15);
        expect(actual?.json['key_game']['lose_rgb'], 0xff0000);
        expect(commands, hasLength(1));
        expect(httpCommands, 0);
        expect(commands.single['command'], 'game-settings');
        expect(commands.single['enabled_mask'], 15);
        expect(commands.single['win_rgb'], 0x123456);
        expect(commands.single['extended_effects'], true);
        expect(commands.single['auto_reset_ms'], 10000);

        final observed = await api.status();
        bridge = AndroidBridge(api, () => observed);
        await bridge.start(
          BridgeOptions(commands: true, telemetrySeconds: 3600),
        );
        bridgeClient = HttpClient();
        final request = await bridgeClient.postUrl(
          Uri.parse('http://127.0.0.1:${bridge.server!.port}/api/request'),
        );
        request.headers.set('Authorization', 'Bearer ${bridge.token}');
        PeerIdentity.current.headers.forEach(request.headers.set);
        request.write(
          jsonEncode({
            'type': 'command',
            'id': 'bridge-one',
            'bridge_session': bridge.admission!.session,
            'board_id': 'keycontroller-board',
            'boot_id': 'a' * 32,
            'expires_at_ms': DateTime.now().millisecondsSinceEpoch + 30000,
            'path': '/api/game/settings',
            'values': {'enabled_mask': '3'},
          }),
        );
        final response = await request.close();
        final bridged =
            jsonDecode(await utf8.decoder.bind(response).join()) as Map;
        expect(response.statusCode, 200);
        expect(bridged['accepted'], true);
        expect(bridged['outcome'], contains('Correlated board receipt'));
        expect((bridged['board'] as Map)['key_game']['enabled_mask'], 3);
        expect(commands, hasLength(2));
        expect(httpCommands, 0);
      } finally {
        await bridge?.stop();
        bridgeClient?.close(force: true);
        await api.close();
        await live?.cancel();
        for (final socket in sockets) {
          await socket.close();
        }
        await listener.cancel();
        await server.close(force: true);
      }
    },
  );

  test(
    'game settings reject unknown and out-of-range fields and false readback',
    () {
      final state = gameState({
        'enabled_mask': 3,
        'win_rgb': 0x00ff00,
        'extended_effects': false,
        'auto_reset_ms': 0,
      });
      final peer = peerFixture(hostname: 'keycontroller');
      final actual = Map<String, dynamic>.of(state);
      expect(
        peer.peer.allows('/api/game/settings', {'enabled_mask': '3'}),
        false,
      );
      expect(
        () => commandPacket(DeviceStatus(actual), '/api/game/settings', {
          'enabled_mask': '0',
        }),
        throwsFormatException,
      );
      expect(
        () => commandPacket(DeviceStatus(actual), '/api/game/settings', {
          'win_rgb': '16777216',
        }),
        throwsFormatException,
      );
      expect(
        () => commandPacket(DeviceStatus(actual), '/api/game/settings', {
          'unknown': '2',
        }),
        throwsFormatException,
      );
      expect(
        commandReadbackMatches(DeviceStatus(actual), '/api/game/settings', {
          'enabled_mask': '15',
        }),
        false,
      );
    },
  );
}
