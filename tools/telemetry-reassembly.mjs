import {createHash} from 'node:crypto';
// Bounded receiver for MQTT /telemetry/part; incomplete/unverified data is never
// surfaced. QoS0 loss means wait for the next complete snapshot, not fake state.
export class TelemetryReceiver{
  #jobs=new Map();
  accept(packet,now=Date.now()){
    for(const [id,j]of this.#jobs)if(now-j.at>30000)this.#jobs.delete(id);
    const p=typeof packet==='string'?JSON.parse(packet):packet;
    if(p?.schema!==1||p.type!=='telemetry-fragment'||p.encoding!=='base64'||
      typeof p.snapshot_id!=='string'||!/^[0-9a-f]{32}-\d{1,10}$/.test(p.snapshot_id)||
      !Number.isInteger(p.total_bytes)||p.total_bytes<1||p.total_bytes>16384||
      !Number.isInteger(p.count)||p.count!==Math.ceil(p.total_bytes/768)||
      !Number.isInteger(p.index)||p.index<0||p.index>=p.count||! /^[0-9a-f]{64}$/.test(p.sha256)||
      typeof p.data!=='string'||p.data.length>1060||! /^[A-Za-z0-9+/=\r\n]*$/.test(p.data))throw Error('Invalid telemetry fragment');
    const bytes=Buffer.from(p.data,'base64'),expected=Math.min(768,p.total_bytes-p.index*768);
    if(bytes.length!==expected||bytes.toString('base64')!==p.data.replace(/[\r\n]/g,''))throw Error('Invalid fragment encoding/length');
    let job=this.#jobs.get(p.snapshot_id);
    if(!job){if(this.#jobs.size>=4)throw Error('Too many incomplete snapshots');job={at:now,count:p.count,total:p.total_bytes,sha:p.sha256,parts:new Map()};this.#jobs.set(p.snapshot_id,job);}
    if(job.count!==p.count||job.total!==p.total_bytes||job.sha!==p.sha256)throw Error('Mixed snapshot metadata');
    const old=job.parts.get(p.index);if(old&&!old.equals(bytes))throw Error('Conflicting duplicate');
    job.parts.set(p.index,bytes);if(job.parts.size!==job.count)return null;
    this.#jobs.delete(p.snapshot_id);
    const full=Buffer.concat(Array.from({length:job.count},(_,i)=>job.parts.get(i)));
    if(full.length!==job.total||createHash('sha256').update(full).digest('hex')!==job.sha)throw Error('Snapshot checksum mismatch');
    const value=JSON.parse(full.toString('utf8'));
    if(value.schema!==1||value.type!=='telemetry'||value.product!=='PakarController'||typeof value.boot_id!=='string')throw Error('Invalid telemetry schema');
    return value;
  }
}
