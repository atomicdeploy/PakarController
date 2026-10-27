// Shared, finite game cues. Browser and AVR use the same pitch and duration.
// No tone is emitted merely by opening the launcher or reconnecting an observer.
export const cues=Object.freeze({
  start:{frequency:659,duration_ms:100},
  eat:{frequency:1047,duration_ms:65},
  over:{frequency:220,duration_ms:180},
  won:{frequency:1319,duration_ms:180},
  pause:{frequency:440,duration_ms:55},
  resume:{frequency:880,duration_ms:55},
});

export class GameAudio {
  constructor(){this.context=null;this.enabled=true;}
  async unlock(){
    const Audio=globalThis.AudioContext??globalThis.webkitAudioContext;
    if(!Audio)return false;
    this.context??=new Audio();
    await this.context.resume();return this.context.state==='running';
  }
  play(name){
    const cue=cues[name],audio=this.context;
    if(!this.enabled||!cue||audio?.state!=='running')return;
    const start=audio.currentTime,end=start+cue.duration_ms/1000;
    const oscillator=audio.createOscillator(),gain=audio.createGain();
    oscillator.type='sine';oscillator.frequency.value=cue.frequency;
    gain.gain.setValueAtTime(0,start);gain.gain.linearRampToValueAtTime(.12,start+.006);
    gain.gain.setValueAtTime(.12,Math.max(start+.006,end-.025));gain.gain.linearRampToValueAtTime(0,end);
    oscillator.connect(gain);gain.connect(audio.destination);
    oscillator.onended=()=>{oscillator.disconnect();gain.disconnect();};
    oscillator.start(start);oscillator.stop(end+.005);
  }
}
