// Uses Pakar's existing living presentation contract, identity and HTTP tooling.
import {randomBytes} from 'node:crypto';
import {deviceOrigin,deviceRequest} from '../../tools/check-ethernet-live.mjs';
import {boardPeer,toolHeaders,toolReport} from '../../tools/peer-identity.mjs';
import {cues} from './sounds.mjs';

export class Presenter {
  constructor({origin,request=deviceRequest,Socket,environment=process.env,now=()=>performance.now()}={}) {
    this.origin=deviceOrigin(origin);this.request=request;this.Socket=Socket;this.now=now;
    this.owner='snake-'+randomBytes(8).toString('hex');this.lease=null;this.uncertain=false;this.busy=false;
    const user=environment.PAKAR_USERNAME,password=environment.PAKAR_PASSWORD;
    if((user===undefined)!==(password===undefined))throw Error('Set both PAKAR_USERNAME and PAKAR_PASSWORD');
    this.headers=user===undefined?{}:{Authorization:'Basic '+Buffer.from(user+':'+password).toString('base64')};
  }
  async read(path,options={}) {
    const response=await this.request(this.origin,path,{...options,expectedAddress:this.peer?.address,
      headers:{...this.headers,...(this.peer?{'X-Pakar-Board':this.peer.board,'X-Pakar-Boot':this.peer.boot}:{}),...options.headers}});
    if(response.status!==200)throw Error(`Board HTTP ${response.status}`);
    const value=JSON.parse(response.body.toString('utf8'));
    if(this.peer&&(value.board_id!==this.peer.board||value.boot_id!==this.peer.boot))throw Error('Board identity changed');
    return {value,remote:response.remote};
  }
  async connect() {
    const first=await this.read('/api/status');this.peer=boardPeer(first.value,first.remote);
    const features=first.value.capabilities?.acknowledged_features??[];
    if(!features.includes('host-presentation')||!features.includes('table-selection'))throw Error('Snake requires an acknowledged table-selection board');
    this.status=first.value;this.observed=this.now();
    this.groups=(await this.read('/api/pixels/groups')).value;
    if(this.groups.group_count!==80||!Number.isSafeInteger(this.groups.revision))throw Error('Expected 80 logical table cells');
    this.layer=(await this.read('/api/presentation')).value;
    if(this.layer.active||this.layer.temporary||this.layer.persistent)throw Error('Existing presentation: refusing to replace it');
    if(!Number.isSafeInteger(this.layer.revision)||!Number.isSafeInteger(this.status.command_clock_ms))throw Error('Missing presentation revision/board clock');
    await this.openSocket();return this.info();
  }
  async openSocket() {
    if(!this.Socket)return;
    await new Promise(resolve=>{
      const url=new URL('/ws',this.origin);url.protocol=url.protocol==='https:'?'wss:':'ws:';
      const ws=new this.Socket(url,{headers:toolHeaders({...this.headers,'X-Pakar-Board':this.peer.board,'X-Pakar-Boot':this.peer.boot}),
        maxPayload:65536,handshakeTimeout:2500,lookup:(_host,options,cb)=>cb(null,options.all?[{address:this.peer.address,family:4}]:this.peer.address,4)});
      this.socket=ws;let finished=false;
      const finish=()=>{if(!finished){finished=true;clearTimeout(timer);resolve();}};
      const timer=setTimeout(()=>{ws.terminate();finish();},3000);
      ws.on('error',()=>{this.socketReady=false;finish();});
      ws.on('close',()=>{this.socketReady=false;if(this.pending){this.pending.reject(Error('Command outcome unknown; lease will expire'));this.pending=null;}finish();});
      ws.on('open',()=>{ws.send(JSON.stringify({type:'status'}));});
      ws.on('message',raw=>{
        try {
          const value=JSON.parse(raw.toString());
          if(value.board_id&&value.boot_id&&(value.board_id!==this.peer.board||value.boot_id!==this.peer.boot))throw Error('WebSocket peer changed');
          if(this.pending&&value.request_id===this.pending.id){const p=this.pending;this.pending=null;p.resolve(value);return;}
          if(value.http_peer_fencing===true){boardPeer(value,this.peer.address,this.peer);this.status=value;this.observed=this.now();
            if(!this.socketReady){this.socketReady=true;ws.send(JSON.stringify({type:'peer-introduction',report:toolReport()}));}finish();}
        }catch{this.socketReady=false;ws.terminate();finish();}
      });
    });
  }
  info(){return {board_id:this.peer?.board,hostname:this.peer?.hostname,transport:this.socketReady?'WebSocket':'HTTP',active:!!this.lease,uncertain:this.uncertain};}
  async send(fields,command='presentation') {
    if(this.busy||this.uncertain)throw Error('Presentation busy or outcome unknown; no replay');
    this.busy=true;
    try {
      if(!this.socketReady||this.now()-this.observed>1500){
        const next=(await this.read('/api/status')).value;boardPeer(next,this.peer.address,this.peer);this.status=next;this.observed=this.now();
      }
      const packet={type:'command',command,board_id:this.peer.board,boot_id:this.peer.boot,
        request_id:randomBytes(16).toString('hex'),expires_at_ms:this.status.command_clock_ms+Math.floor(this.now()-this.observed)+8000,
        ...(command==='presentation'?{revision:this.layer.revision,group_layout_revision:this.groups.revision,owner:this.owner,
        lifetime:'temporary',ttl_ms:3000,space:'cells',composite:'replace',replace:true,
        ...(this.lease?{lease_id:this.lease}:{})}:{}),...fields};
      if(Buffer.byteLength(JSON.stringify(packet))>4096)throw Error('Presentation exceeds command budget');
      if(command==='presentation')this.uncertain=true;let receipt;
      if(this.socketReady){
        receipt=await new Promise((resolve,reject)=>{
          const timer=setTimeout(()=>{this.pending=null;reject(Error('Command outcome unknown; no HTTP replay'));},2200);
          this.pending={id:packet.request_id,resolve:v=>{clearTimeout(timer);resolve(v);},reject:e=>{clearTimeout(timer);reject(e);}};
          try{this.socket.send(JSON.stringify(packet));}catch(error){this.pending=null;clearTimeout(timer);reject(error);}
        });
      }else receipt=(await this.read('/api/command',{form:{packet:JSON.stringify(packet)}})).value;
      if(receipt.board_id!==this.peer.board||receipt.boot_id!==this.peer.boot||receipt.request_id!==packet.request_id||receipt.accepted!==true)
        throw Error(receipt.error??receipt.reason??'Command not acknowledged; no replay');
      if(command!=='presentation')return receipt;
      if(
        !Number.isSafeInteger(receipt.presentation?.revision)||receipt.presentation.revision<=this.layer.revision||
        (fields.action!=='clear'&&!(receipt.presentation.lease_id>0)))throw Error(receipt.error??'Presentation not acknowledged; no replay');
      this.layer=receipt.presentation;this.lease=fields.action==='clear'?null:this.layer.lease_id;this.uncertain=false;
      return this.info();
    } finally {this.busy=false;}
  }
  frame(entries){return this.send({entries});}
  renew(){return this.send({entries:[],replace:false});}
  async sound(name){
    if(!cues[name])throw Error('Unknown sound cue');
    if(!this.status.capabilities?.acknowledged_features?.includes('buzzer'))throw Error('Board buzzer is not acknowledged');
    return this.send(cues[name],'buzzer');
  }
  async close(){
    try{
      if(this.uncertain)return {restored:false,pending:true,message:'Presentation expires within 3 seconds of its last accepted frame'};
      if(this.lease){
        await this.send({action:'clear'});
        const current=(await this.read('/api/presentation')).value;
        if(current.temporary)throw Error('Scene release not yet confirmed');
        return {restored:true,pending:false,message:'Game layer released; board scene restored'};
      }
      return {restored:false,pending:false};
    }finally{this.socket?.close();this.socketReady=false;}
  }
}
