// Existing CloudCommandGuard + mqtt-live-probe correlation discipline. This is
// one transport for the living API, not a board-version-specific adapter.
import {EventEmitter} from 'node:events';
import {randomUUID} from 'node:crypto';
import {TelemetryReceiver} from '../../tools/telemetry-reassembly.mjs';
import {peerVersionAllowed,toolSemver,toolRelease} from '../../tools/peer-identity.mjs';

export const failure=(status,code)=>Object.assign(Error(code),{status,code});
export const supportedFeatures=new Set(['telemetry','leds','table','cell-batch','display','text','buzzer','inputs','outputs','status-rgb']);
const identity=s=>s&&typeof s.board_id==='string'&&/^[a-z0-9_-]{1,64}$/.test(s.board_id)&&/^(?!0{32})[a-f0-9]{32}$/.test(s.boot_id??'');
export class Gateway extends EventEmitter {
  constructor({client,topic,board,release,now=Date.now,timeout=10000}){
    super();Object.assign(this,{client,topic,board,release,now,timeout});
    this.pending=new Map();this.receivers=new TelemetryReceiver();this.connected=false;this.status=null;this.at=0;this.sequence=0;this.readPending=false;
    client.on('connect',()=>client.subscribe(['status','availability','event','telemetry/part'].map(s=>topic+'/'+s),{qos:0},error=>{if(!error){this.connected=true;this.requestRead()}}));
    client.on('close',()=>this.disconnect());client.on('offline',()=>this.disconnect());
    client.on('error',()=>this.emit('transport-error'));
    client.on('message',(topic,bytes,packet)=>{try{this.message(topic,bytes,packet)}catch{this.emit('invalid-packet')}});
  }
  disconnect(){this.connected=false;this.at=0;for(const p of this.pending.values())p.reject(failure(503,'connection_lost_outcome_unknown'));this.pending.clear();this.emit('offline')}
  fresh(){return this.connected&&this.status&&this.now()-this.at>=0&&this.now()-this.at<8000}
  message(topic,bytes,packet={}){
    if(bytes.length>20000)return;
    if(topic===this.topic+'/availability'){if(bytes.toString()==='offline')this.disconnect();return}
    const value=JSON.parse(bytes.toString());
    if(topic===this.topic+'/status'){
      if(!identity(value)||value.board_id!==this.board||!toolSemver(value.version))throw Error('foreign_status');
      // A retained online/status packet is not evidence of a currently live MCU.
      if(packet.retain)return;
      this.connected=this.client.connected;this.status=value;this.at=this.now();this.sequence++;this.readPending=false;this.emit('status',this.project());return;
    }
    if(packet.retain)return; // Replies and telemetry must never be replayable.
    if(topic===this.topic+'/event'){
      const p=this.pending.get(value.request_id);
      if(p&&typeof value.accepted==='boolean'&&typeof value.code==='string'){
        this.pending.delete(value.request_id);value.accepted?p.resolve(value):p.reject(failure(409,'board_rejected_'+value.code));
      }
      return;
    }
    if(topic===this.topic+'/telemetry/part'){
      const full=this.receivers.accept(value,this.now());
      if(full&&full.board_id===this.board&&full.boot_id===this.status?.boot_id){this.telemetry=full;this.telemetryAt=this.now();this.emit('telemetry',full)}
    }
  }
  project(){
    if(!this.fresh())throw failure(503,'board_unavailable');
    const s=structuredClone(this.status);
    s.capabilities={...s.capabilities,acknowledged_features:(s.capabilities?.acknowledged_features??[]).filter(f=>supportedFeatures.has(f))};
    s.relay={application:'pakarcontroller-remote-panel',version:toolRelease.version,web_version:this.release.version,transport:'mqtt',direct:false,age_ms:this.now()-this.at};
    return s;
  }
  compatible(){
    const s=this.project(),p=s.compatibility;
    if(!peerVersionAllowed(s.version,this.release.minimum_board,this.release.maximum_board)||
      (p?.enforced!==false&&p?.minimum_app!==undefined&&!peerVersionAllowed(this.release.version,p.minimum_app,p.maximum_app)))throw failure(426,'unsupported_peer_version');
    return s;
  }
  send(value){
    if(!this.connected||!this.client.connected)throw failure(503,'broker_unavailable');
    const bytes=JSON.stringify(value);if(Buffer.byteLength(bytes)>1024)throw failure(413,'command_too_large');
    // MQTT.js is configured queueQoSZero:false. Never enqueue offline, retain,
    // use QoS replay, or resend after a publish/ACK timeout.
    this.client.publish(this.topic+'/command',bytes,{qos:0,retain:false},error=>{if(error){this.pending.get(value.request_id)?.reject(failure(504,'publish_outcome_unknown'));this.pending.delete(value.request_id)}});
  }
  requestRead(){
    if(!this.connected||this.readPending)return;
    this.readPending=true;
    try{this.send({command:'telemetry',request_id:'read-'+randomUUID().replaceAll('-','')})}catch{this.readPending=false}
    setTimeout(()=>{this.readPending=false},2000).unref?.();
  }
  waitAfter(sequence){
    if(this.fresh()&&this.sequence>sequence)return Promise.resolve(this.project());
    return new Promise((resolve,reject)=>{
      const finish=(error,s)=>{clearTimeout(timer);this.off('status',changed);this.off('offline',offline);error?reject(error):resolve(s)};
      const changed=s=>{if(this.sequence>sequence)finish(null,s)},offline=()=>finish(failure(503,'board_unavailable'));
      const timer=setTimeout(()=>finish(failure(504,'fresh_status_timeout')),this.timeout);timer.unref?.();
      this.on('status',changed);this.on('offline',offline);this.requestRead();
    });
  }
  read(){return this.fresh()?Promise.resolve(this.project()):this.waitAfter(this.sequence)}
  async command(command,values,pin){
    const s=this.compatible();
    if(pin?.board!==this.board||pin?.boot!==s.boot_id)throw failure(409,'peer_changed');
    if(this.pending.size>=12)throw failure(429,'command_capacity');
    const request_id=randomUUID().replaceAll('-',''),sequence=this.sequence;
    const request={...values,command,client_version:this.release.version,board_id:this.board,boot_id:s.boot_id,request_id,expires_at:Math.floor(this.now()/1000)+15};
    const reply=await new Promise((resolve,reject)=>{
      const timer=setTimeout(()=>{this.pending.delete(request_id);reject(failure(504,'command_outcome_unknown'))},this.timeout);timer.unref?.();
      const done=fn=>value=>{clearTimeout(timer);fn(value)};
      this.pending.set(request_id,{resolve:done(resolve),reject:done(reject)});
      try{this.send(request)}catch(e){this.pending.delete(request_id);clearTimeout(timer);reject(e)}
    });
    // Solicit a new snapshot, never replay the mutation, before reporting its
    // requested state to a client. A different boot is an uncertain outcome.
    this.readPending=false;let after;
    try{after=await this.waitAfter(Math.max(sequence,this.sequence))}
    catch(error){error.outcome_known=false;throw error}
    if(after.boot_id!==s.boot_id)throw Object.assign(failure(409,'peer_changed_after_command'),{outcome_known:false});
    return reply;
  }
}

export function commandFor(path,form){
  const get=k=>form.get(k),number=k=>{const v=get(k);if(v===null||!/^\d{1,9}$/.test(v))throw failure(400,'invalid_'+k);return Number(v)};
  const required=(key,allowed)=>{const v=get(key);if(!allowed.includes(v))throw failure(400,'invalid_'+key);return v};
  switch(path){
    case '/api/cells': {const ids=get('ids');if(typeof ids!=='string'||!/^\d{1,2}(?:,\d{1,2}){0,79}$/.test(ids)||ids.split(',').some(id=>Number(id)<1||Number(id)>80))throw failure(400,'invalid_ids');return ['cells',{ids,state:required('state',['white','red','off']),effect:get('effect')===null?'immediate':required('effect',['immediate','fade','pulse'])},new Set(ids.split(',').map(Number)).size]}
    case '/api/cell': {const id=number('id');if(id<1||id>80)throw failure(400,'invalid_id');return ['cell',{id,state:required('state',['white','red','off']),effect:get('effect')===null?'immediate':required('effect',['immediate','fade','pulse'])},1]}
    case '/api/effect':return ['effect',{value:required('value',['none','rainbow','fire','cell-chase'])}];
    case '/api/brightness': {const value=number('value');if(value<1||value>255)throw failure(400,'invalid_brightness');return ['brightness',{value}]}
    case '/api/mode':return ['mode',{value:required('value',['cell','first-wins'])}];
    case '/api/game/reset':return ['reset',{}];
    case '/api/outputs': {const mask=number('mask');if(mask>255)throw failure(400,'invalid_mask');return ['outputs',{mask}]}
    case '/api/led/output': {const values={};if(form.has('enabled'))values.enabled=required('enabled',['true','false'])==='true';if(form.has('mode'))values.mode=required('mode',['static','effects']);if(!Object.keys(values).length)throw failure(400,'missing_output_settings');return ['led-output',values]}
    case '/api/audio':return ['audio',{cue:required('cue',['welcome','update-start','update-success','error','winner','finish','lost','incorrect','rapid-error','fault','increase','decrease','welcome-puzzleboard','welcome-40led','attention'])}];
    case '/api/buzzer': {const frequency=number('frequency'),duration_ms=number('duration_ms');if(frequency<20||frequency>12000||duration_ms<1||duration_ms>5000)throw failure(400,'invalid_tone');return ['buzzer',{frequency,duration_ms}]}
    case '/api/display': {if(get('release')==='true')return ['display',{release:true}];if(form.has('text')){const text=get('text');if(text.length>40||/[^\x20-\x7e]/.test(text))throw failure(400,'invalid_display_text');const values={text};for(const k of ['speed_ms','hold_ms','interval_seconds'])if(form.has(k))values[k]=number(k);if(form.has('repeat'))values.repeat=required('repeat',['once','loop','interval']);if(form.has('force_scroll'))values.force_scroll=required('force_scroll',['true','false'])==='true';return ['display',values]}const v=get('value');if(!/^-?\d{1,4}$/.test(v??'')||Number(v)<-999||Number(v)>9999)throw failure(400,'invalid_display_value');return ['display',{value:Number(v)}]}
    default:throw failure(501,'route_unavailable_over_mqtt');
  }
}
