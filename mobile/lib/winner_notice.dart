import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'models.dart';

class WinnerNotice {
  String? peer;
  int previous=0;
  int? observe(DeviceStatus value) {
    final board=value.json['board_id'], boot=value.json['boot_id'];
    final winner=value.json['winner'];
    if(value.peer.role!='first-wins' || board is! String || board.isEmpty ||
        boot is! String || boot.isEmpty || winner is! int || winner<0 || winner>9) {return null;}
    final owner='$board/$boot';
    if(peer!=owner){peer=owner;previous=winner;return null;}
    final old=previous;previous=winner;
    return old==0 && winner!=0 ? winner : null;
  }
}

// Application-only notification preferences; never play a second board melody.
final winnerAlerts=ValueNotifier(true);
final winnerAlertSound=ValueNotifier(false);
Future<void> loadWinnerPreferences() async {
  final prefs=await SharedPreferences.getInstance();
  winnerAlerts.value=prefs.getBool('pakar.winner_alerts')??true;
  winnerAlertSound.value=prefs.getBool('pakar.winner_sound')??false;
}
void showWinner(BuildContext context,int winner) {
  if(!winnerAlerts.value)return;
  ScaffoldMessenger.of(context).showSnackBar(SnackBar(
    content: Text('Box $winner wins'),duration: const Duration(seconds: 3)));
  if(winnerAlertSound.value)SystemSound.play(SystemSoundType.alert);
}
class WinnerPreferences extends StatelessWidget {
  const WinnerPreferences({super.key});
  @override Widget build(BuildContext context)=>Column(children:[
    ValueListenableBuilder(valueListenable:winnerAlerts,builder:(context,value,_)=>SwitchListTile(
      title:const Text('Winner notifications'),value:value,onChanged:(next)async{
        winnerAlerts.value=next;(await SharedPreferences.getInstance()).setBool('pakar.winner_alerts',next);
      })),
    ValueListenableBuilder(valueListenable:winnerAlertSound,builder:(context,value,_)=>SwitchListTile(
      title:const Text('Winner notification sound'),value:value,onChanged:(next)async{
        winnerAlertSound.value=next;(await SharedPreferences.getInstance()).setBool('pakar.winner_sound',next);
      })),
  ]);
}
