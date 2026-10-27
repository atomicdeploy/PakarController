import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'models.dart';

/// Uses the same acknowledged command owner as table and key controls.
/// Draft fields and temporary previews never stand in for saved observations.
class StateAppearancePage extends StatefulWidget {
  const StateAppearancePage({super.key, required this.status, required this.command,
    required this.readStatus, required this.ids});
  final DeviceStatus status;
  final Future<bool> Function(String, Map<String, String>) command;
  final Future<DeviceStatus> Function() readStatus;
  final String ids;
  @override
  State<StateAppearancePage> createState() => _StateAppearancePageState();
}

class _StateAppearancePageState extends State<StateAppearancePage> {
  late DeviceStatus observed = widget.status;
  bool busy = false;
  String? error;
  final testColor = TextEditingController(text: '00FF00');
  Map get presentation => observed.json['state_presentation'] as Map? ?? const {};
  bool get samePeer => observed.json['board_id'] == widget.status.json['board_id'] &&
      observed.json['boot_id'] == widget.status.json['boot_id'];
  @override
  void dispose() {testColor.dispose();super.dispose();}
  Future<void> send(String path, Map<String, String> values) async {
    if(busy || !samePeer) return;
    setState(() {busy=true;error=null;});
    try {
      final current = await widget.readStatus();
      if(current.json['board_id'] != widget.status.json['board_id'] ||
          current.json['boot_id'] != widget.status.json['boot_id']) {
        throw const FormatException('Board changed; reopen appearance settings');
      }
      if(!await widget.command(path, values)) {
        throw const FormatException('Device did not confirm this change');
      }
      final next = await widget.readStatus();
      if(mounted) setState(() => observed=next);
    } on Object catch(e) {
      if(mounted) setState(() => error='$e');
    } finally {if(mounted) setState(() => busy=false);}
  }
  Future<void> patch(Map<String, Object> value) => send('/api/led/presentation', {'settings':jsonEncode(value)});
  Future<void> chooseColor(String state, int rgb) async {
    final input=TextEditingController(text:rgb.toRadixString(16).padLeft(6,'0').toUpperCase());
    final value=await showDialog<int>(context:context,builder:(context)=>AlertDialog(
      title:Text('$state color'),
      content:TextField(controller:input,autofocus:true,maxLength:6,
        inputFormatters:[FilteringTextInputFormatter.allow(RegExp('[0-9a-fA-F]'))],
        decoration:const InputDecoration(prefixText:'#',labelText:'RGB hex')),
      actions:[TextButton(onPressed:()=>Navigator.pop(context),child:const Text('Cancel')),
        FilledButton(onPressed:(){if(input.text.length==6)Navigator.pop(context,int.parse(input.text,radix:16));},child:const Text('Apply'))]));
    input.dispose();
    if(value!=null && mounted)await patch({'states':{state:{'rgb':value}}});
  }
  @override
  Widget build(BuildContext context) {
    final states=presentation['states'] as Map? ?? const {};
    return Scaffold(appBar:AppBar(title:const Text('State colors & effects')),
      body:ListView(padding:const EdgeInsets.all(16),children:[
        SwitchListTile(title:const Text('Extended effects'),value:presentation['enabled']==true,
          onChanged:busy||!samePeer?null:(v)=>patch({'enabled':v})),
        for(final state in ['idle','won','lost','off']) Builder(builder:(context){
          final style=states[state] as Map? ?? const {};
          final rgb=style['rgb'];final motion=style['motion'];
          return Card(child:Padding(padding:const EdgeInsets.all(12),child:Wrap(
            spacing:16,runSpacing:8,crossAxisAlignment:WrapCrossAlignment.center,children:[
              SizedBox(width:60,child:Text(state.toUpperCase(),style:Theme.of(context).textTheme.labelLarge)),
              OutlinedButton.icon(onPressed:busy||!samePeer||state=='off'||rgb is! int?null:()=>chooseColor(state,rgb),
                icon:Icon(Icons.circle,color:rgb is int?Color(0xff000000|rgb):null),
                label:Text(rgb is int?'#${rgb.toRadixString(16).padLeft(6,'0').toUpperCase()}':'Unknown')),
              if(state!='off' && (observed.peer.tableSelection || state!='idle') && motion is int && motion>=0 && motion<4)
                DropdownButton<int>(value:motion,items:[
                  for(final entry in ['Solid','Pulse','Circular chase','Chase'].asMap().entries)
                    DropdownMenuItem(value:entry.key,child:Text(entry.value))],
                  onChanged:busy||!samePeer?null:(v){if(v!=null)patch({'states':{state:{'motion':v}}});}),
            ])));
        }),
        for(final field in const {'fade_ms':'Transition (ms)','pulse_ms':'Pulse period (ms)',
          'ring_ms':'Circular period (ms)','chase_ms':'Chase period (ms)'}.entries)
          Padding(padding:const EdgeInsets.symmetric(vertical:6),child:TextFormField(
            key:ValueKey('${field.key}:${presentation[field.key]}'),initialValue:'${presentation[field.key] ?? ''}',
            enabled:!busy&&samePeer,keyboardType:TextInputType.number,
            inputFormatters:[FilteringTextInputFormatter.digitsOnly],
            decoration:InputDecoration(labelText:field.value),
            onFieldSubmitted:(text){final n=int.tryParse(text);if(n!=null)patch({field.key:n});})),
        SwitchListTile(title:const Text('Chase affects adjacent pixels'),value:presentation['neighbors']==true,
          onChanged:busy||!samePeer?null:(v)=>patch({'neighbors':v})),
        SwitchListTile(title:const Text('Bright chasing dot'),value:presentation['invert']==true,
          onChanged:busy||!samePeer?null:(v)=>patch({'invert':v})),
        if(observed.peer.tableSelection) SwitchListTile(title:const Text('Diagonal lost-state ripple'),
          value:presentation['diagonal']==true,onChanged:busy||!samePeer?null:(v)=>patch({'diagonal':v})),
        const Divider(),
        Text('Temporary color test',style:Theme.of(context).textTheme.titleMedium),
        TextField(controller:testColor,maxLength:6,inputFormatters:[FilteringTextInputFormatter.allow(RegExp('[0-9a-fA-F]'))],
          decoration:const InputDecoration(prefixText:'#',labelText:'RGB hex')),
        Wrap(spacing:12,children:[
          FilledButton.icon(icon:const Icon(Icons.visibility_outlined),label:const Text('Preview · 5 seconds'),
            onPressed:busy||!samePeer||widget.ids.isEmpty?null:(){
              final color=int.tryParse(testColor.text,radix:16);
              if(color!=null&&testColor.text.length==6)send('/api/led/preview',{'ids':widget.ids,'rgb':'$color','duration_ms':'5000'});
            }),
          OutlinedButton.icon(icon:const Icon(Icons.stop),label:const Text('Stop preview'),
            onPressed:busy||!samePeer?null:()=>send('/api/led/preview',{'cancel':'true'})),
        ]),
        SizedBox(height:48,child:busy?const LinearProgressIndicator():Text(error??'',style:TextStyle(color:Theme.of(context).colorScheme.error))),
      ]));
  }
}
