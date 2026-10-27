import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/winner_notice.dart';
import 'package:pakarcontroller/key_pixel_preview.dart';
import 'peer_fixture.dart';
DeviceStatus game(int winner,{String boot='boot'}) {
  final base=peerFixture();
  return DeviceStatus({...base.json,'board_id':'keys','boot_id':boot,'winner':winner,
    'capabilities':{...(base.json['capabilities'] as Map),'role':'first-wins'}});
}
void main(){
  test('winner edge, duplicate samples and reboot baselines',(){
    final notice=WinnerNotice();
    expect(notice.observe(game(0)),null);
    expect(notice.observe(game(2)),2);
    expect(notice.observe(game(2)),null);
    expect(notice.observe(game(2,boot:'new')),null);
    expect(notice.observe(game(0,boot:'new')),null);
    expect(notice.observe(game(2,boot:'new')),2);
  });
  test('two independent twelve-pixel groups use published mapping',(){
    final state=DeviceStatus({...game(1).json,'pixel_frame':{
      'revision':1,'pending':false,'physical':List.generate(18,(i)=>i+1),
      'rgb':'ff0000'*12+'00ff00'*12+'0000ff'*192,
    }});
    expect(keyPixelGroup(state,1,0),List.filled(12,0xff0000));
    expect(keyPixelGroup(state,1,1),List.filled(12,0x00ff00));
    expect(keyPixelGroup(state,2,0),List.filled(12,0x0000ff));
    expect(keyPixelGroup(state,10,0),null);
    expect(keyPixelGroup(DeviceStatus({...state.json,'pixel_frame':{}}),1,0),null);
  });
}
