// SPDX-License-Identifier: MIT
// Adapted from PCController Tools/Bootloader/Urboot-Custom/build.mjs:
// Intel HEX validation, wrapped RJMP, unused-vector proof and merged-image
// round trip. Retain MiniCore's STOCK 384-byte UART0 Urboot; do not import the
// custom TM1637 backend wired to different pins or rebuild/download a toolchain.
import {readFileSync,writeFileSync,existsSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {join,dirname,resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const flashBytes=32768,flashWords=flashBytes/2;
const bootHexSha256='a1f557128760c597d12822faa072eb8712562fd49150cc03807dcdd40fa3a192';
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
function fail(message){throw Error(message)}
export function parseHex(text){
  let upper=0,ended=false;const bytes=new Map();
  for(const line of text.trim().split(/\r?\n/)){
    if(ended||!/^:(?:[0-9a-fA-F]{2})+$/.test(line))fail('Malformed Intel HEX or data after EOF');
    const record=Buffer.from(line.slice(1),'hex'),count=record[0],address=record.readUInt16BE(1),type=record[3];
    if(record.length!==count+5||record.reduce((sum,value)=>(sum+value)&255,0)!==0)fail('Invalid Intel HEX length/checksum');
    if(type===1){if(count||address)fail('Invalid EOF');ended=true;}
    else if(type===2||type===4){
      if(count!==2||address)fail('Invalid upper-address record');
      upper=record.readUInt16BE(4)*(type===2?16:65536);
    }else if(type===0){
      for(let i=0;i<count;i++){
        const at=upper+address+i,value=record[4+i];
        if(at>=flashBytes)fail('Intel HEX exceeds ATmega328P flash');
        if(bytes.has(at)&&bytes.get(at)!==value)fail('Conflicting Intel HEX data');
        bytes.set(at,value);
      }
    }else if(type===3||type===5){
      // The pinned MiniCore stock HEX includes a start-segment record. It is
      // entry metadata, not flash data and not the AVR hardware reset vector.
      if(count!==4||address||bytes.startAddress!==undefined)fail('Invalid/duplicate start record');
      const entry=type===3?record.readUInt16BE(4)*16+record.readUInt16BE(6):record.readUInt32BE(4);
      if(entry>=flashBytes||(entry&1))fail('Start record outside AVR flash');
      bytes.startAddress=entry;
    }else fail('Unsupported Intel HEX record');
  }
  if(!ended||!bytes.size)fail('Missing Intel HEX EOF/data');return bytes;
}
function readWord(memory,address){
  if(!memory.has(address)||!memory.has(address+1))fail('Missing AVR vector word');
  return memory.get(address)|(memory.get(address+1)<<8);
}
export function decodeJump(memory,address){
  const opcode=readWord(memory,address);
  if((opcode&0xf000)===0xc000){
    let delta=opcode&0xfff;if(delta&0x800)delta-=0x1000;
    return ((((address>>>1)+1+delta)%flashWords+flashWords)%flashWords)*2;
  }
  if(opcode===0x940c){const target=readWord(memory,address+2)*2;if(target<flashBytes)return target;}
  fail('Unsupported/out-of-range AVR vector jump');
}
function writeWord(memory,address,word){memory.set(address,word&255);memory.set(address+1,(word>>>8)&255);}
function absoluteJump(memory,address,target){writeWord(memory,address,0x940c);writeWord(memory,address+2,target>>>1);}
function wrappedRjmp(from,to){
  let delta=(to>>>1)-((from>>>1)+1);
  while(delta>2047)delta-=flashWords;
  while(delta< -2048)delta+=flashWords;
  if(delta< -2048||delta>2047)fail('Wrapped RJMP target out of range');return 0xc000|(delta&0xfff);
}
export function mergeUrboot(application,boot,badInterruptAddress){
  // The exact stock profile is pinned by hash in the file-producing entrypoint.
  // Metadata owns boot allocation/vector number; no guessed hardware BOOTRST.
  const pages=boot.get(0x7ffa),vector=boot.get(0x7ffb),start=flashBytes-pages*128;
  if(pages!==3||vector!==25||Math.min(...boot.keys())!==start||Math.max(...boot.keys())!==flashBytes-1||boot.size!==384)
    fail('Unsupported Urboot metadata/allocation');
  if(boot.startAddress!==undefined&&boot.startAddress!==start)fail('Urboot start record mismatch');
  if(!application.size||Math.min(...application.keys())!==0||Math.max(...application.keys())>=start)
    fail('Application overlaps Urboot or lacks reset vector');
  const originalReset=decodeJump(application,0),vectorAddress=vector*4;
  if(originalReset<104||originalReset>=start)fail('Invalid application reset target');
  if(!Number.isInteger(badInterruptAddress)||badInterruptAddress<104||badInterruptAddress>=start||
      decodeJump(application,vectorAddress)!==badInterruptAddress)fail('Urboot trampoline vector is in use');
  for(let i=0;i<4;i++)if(!application.has(vectorAddress+i))fail('Incomplete trampoline vector');
  const merged=new Map(application);
  writeWord(merged,0,wrappedRjmp(0,start));writeWord(merged,2,0);
  absoluteJump(merged,vectorAddress,originalReset);
  for(let i=start;i<flashBytes;i++)merged.set(i,boot.get(i));
  if(decodeJump(merged,0)!==start||decodeJump(merged,vectorAddress)!==originalReset)fail('Broken boot/application vector chain');
  const binary=Buffer.alloc(flashBytes,255);for(const [at,value]of merged)binary[at]=value;
  return {merged,binary,proof:{boot_start:start,vector,vector_address:vectorAddress,application_start:originalReset,
    bad_interrupt:badInterruptAddress,boot_bytes:384,application_bytes:application.size}};
}
function hexRecord(type,address,data=[]){
  const values=[data.length,(address>>>8)&255,address&255,type,...data];
  values.push((-values.reduce((sum,value)=>sum+value,0))&255);
  return ':'+values.map(value=>value.toString(16).padStart(2,'0').toUpperCase()).join('');
}
export function encodeHex(binary){
  const lines=[];for(let at=0;at<binary.length;at+=16)lines.push(hexRecord(0,at,[...binary.subarray(at,at+16)]));
  lines.push(hexRecord(1,0));return lines.join('\r\n')+'\r\n';
}
function writeChanged(path,data){
  const bytes=Buffer.from(data);if(existsSync(path)&&readFileSync(path).equals(bytes))return;
  writeFileSync(path,bytes);
}
export function prepare(directory,cache){
  const options=JSON.parse(readFileSync(join(cache,'build.options.json'),'utf8'));
  if(!options.fqbn?.startsWith('MiniCore:avr:328:')||!options.fqbn.includes('bootloader=uart0')||
      !options.fqbn.includes('variant=modelP')||!options.fqbn.includes('eeprom=keep'))fail('Unsupported AVR build profile');
  const core=options.hardwareFolders.split(',')[0];
  const bootPath=join(core,'bootloaders/urboot/atmega328p/watchdog_1_s/autobaud/uart0_rxd0_txd1/led+b5/urboot_atmega328p_pr_ee_ce.hex');
  const bootText=readFileSync(bootPath);
  if(hash(bootText)!==bootHexSha256)fail('Urboot source changed; rereview required before packaging');
  const commands=JSON.parse(readFileSync(join(cache,'compile_commands.json'),'utf8'));
  const compiler=commands[0]?.arguments?.[0];if(!compiler)fail('No actual compiler identity in build cache');
  const nm=join(dirname(compiler),process.platform==='win32'?'avr-nm.exe':'avr-nm');
  const elf=join(directory,'avr.ino.elf');
  const symbols=spawnSync(nm,['-n',elf],{encoding:'utf8',windowsHide:true});
  if(symbols.status!==0)fail('Cannot inspect application ELF vector ownership');
  const match=symbols.stdout.match(/^([0-9a-fA-F]+)\s+\w\s+__bad_interrupt$/m);
  if(!match)fail('Application has no __bad_interrupt symbol');
  const appText=readFileSync(join(directory,'avr.ino.hex'));
  const result=mergeUrboot(parseHex(appText.toString('ascii')),parseHex(bootText.toString('ascii')),parseInt(match[1],16));
  const hex=encodeHex(result.binary),roundtrip=parseHex(hex);
  if(roundtrip.size!==flashBytes||[...roundtrip].some(([at,value])=>result.binary[at]!==value))fail('Merged HEX round trip failed');
  // All checks above precede changes to generated outputs. Never touch source,
  // hardware, EEPROM or fuses. Keep the canonical existing artifact names.
  writeChanged(join(directory,'avr.ino.with_bootloader.bin'),result.binary);
  writeChanged(join(directory,'avr.ino.with_bootloader.hex'),hex);
  const proof={...result.proof,profile:options.fqbn,boot_hex_sha256:bootHexSha256,
    application_hex_sha256:hash(appText),binary_sha256:hash(result.binary),hex_sha256:hash(hex),
    structural_validation:true,physical_validation:false,clock_voltage_requires_live_verification:true};
  writeChanged(join(directory,'avr-image.json'),JSON.stringify(proof,null,2)+'\n');
  console.log('AVR ISP boot chain verified:',JSON.stringify(proof));return proof;
}
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  if(process.argv.length!==4)fail('Usage: node tools/prepare-avr-image.mjs OUTPUT_DIRECTORY BUILD_CACHE');
  prepare(resolve(process.argv[2]),resolve(process.argv[3]));
}
