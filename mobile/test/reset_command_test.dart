import 'dart:convert';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/command_wire.dart';
import 'peer_fixture.dart';

void main() {
  test('reset uses correlated command and requires AVR acknowledgment', () {
    expect(commandNames['/api/game/reset'], 'reset');
    final packet=jsonEncode({'request_id':'a'*32,'board_id':'keycontroller','boot_id':'b'*32});
    final receipt=<String,dynamic>{
      'type':'command-result','request_id':'a'*32,'board_id':'keycontroller','boot_id':'b'*32,
      'accepted':true,'avr_acknowledged':true,'cached':true,
      'status':{...peerFixture().json,'board_id':'keycontroller','boot_id':'b'*32,'winner':1},
    };
    // A cached receipt proves the reset happened; a later winner must not
    // provoke another reset of the new round.
    expect(verifyCommandReply(receipt,packet,'/api/game/reset',{}).json['winner'],1);
    expect(()=>verifyCommandReply({...receipt,'avr_acknowledged':false},packet,'/api/game/reset',{}),throwsFormatException);
    expect(()=>verifyCommandReply({...receipt,'boot_id':'c'*32},packet,'/api/game/reset',{}),throwsFormatException);
  });
}
