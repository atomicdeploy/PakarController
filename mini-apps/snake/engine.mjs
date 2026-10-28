// DRS-jSnake mechanics adapted from David Refoua's DRS-JSnake.html (2013).
// Original attribution/rights retained; see README.md and donor ledger.
// Pure host engine: no timers, DOM, transports or LED side effects on import.
export const directions = Object.freeze({up: [0,-1], right: [1,0], down: [0,1], left: [-1,0]});
export const tiles = Object.freeze(['block','detour1','detour2','reverse','teleport','switch','potion']);
export const logicalCell = (x,y) => 1 + Math.floor(x/2) + (x%2)*10 + y*20;
export const position = cell => { const n=cell-1; return {x:2*(n%10)+Math.floor(n/10)%2,y:Math.floor(n/20)}; };
const integer = (v,min,max) => Number.isInteger(v) && v>=min && v<=max;
export class Snake {
  constructor({random=Math.random}={}) { this.random=random; this.configure({}); this.reset(); }
  configure(options) {
    const next={interval:250,growth:3,seeds:1,immortal:false,autoplay:false,...this.options,...options};
    if(!integer(next.interval,80,2000)||!integer(next.growth,1,8)||!integer(next.seeds,1,8)||
       typeof next.immortal!=='boolean'||typeof next.autoplay!=='boolean') throw Error('Invalid game options');
    this.options=next;
  }
  reset() {
    this.body=[logicalCell(4,1),logicalCell(3,1),logicalCell(2,1)];
    this.direction='right'; this.queued=null; this.food=[]; this.growing=0; this.score=0;
    this.phase='ready'; this.tick=0; this.switchOpen=false; this.map??={}; this.fillFood();
  }
  setMap(map) {
    if(this.phase==='running'||this.phase==='paused') throw Error('Stop before editing the map');
    if(!map||Array.isArray(map)||typeof map!=='object'||Object.keys(map).length>70) throw Error('Invalid map');
    const next={};
    for(const [id,type] of Object.entries(map)) {
      if(!/^[1-9][0-9]?$/.test(id)||!integer(+id,1,80)||!tiles.includes(type)) throw Error('Invalid map tile');
      if([2,3,4].some(x=>logicalCell(x,1)===+id)) throw Error('Keep the initial snake cells clear');
      next[id]=type;
    }
    const portals=Object.values(next).filter(t=>t==='teleport').length;
    if(portals===1) throw Error('Place at least two portals');
    this.map=next; this.food=this.food.filter(id=>!next[id]); this.fillFood();
  }
  fillFood() {
    const free=Array.from({length:80},(_,i)=>i+1).filter(id=>!this.body.includes(id)&&!this.food.includes(id)&&!this.map[id]);
    while(this.food.length<this.options.seeds&&free.length) {
      const index=Math.min(free.length-1,Math.max(0,Math.floor(this.random()*free.length)));
      this.food.push(free.splice(index,1)[0]);
    }
    if(!free.length&&!this.food.length&&this.body.length===80) this.phase='won';
  }
  turn(direction) {
    if(!Object.hasOwn(directions,direction)) throw Error('Unknown direction');
    const [x,y]=directions[this.direction], [nx,ny]=directions[direction];
    // One turn per simulation tick prevents right -> up -> left reversal.
    if(this.queued||x+nx===0&&y+ny===0) return false;
    this.queued=direction; return true;
  }
  next(id,direction) {
    const {x,y}=position(id),[dx,dy]=directions[direction];
    return logicalCell((x+dx+20)%20,(y+dy+4)%4);
  }
  autoDirection() {
    const target=position(this.food[0]??this.body[0]),head=position(this.body[0]);
    const distance=id=>{const p=position(id),dx=Math.abs(p.x-target.x),dy=Math.abs(p.y-target.y);return Math.min(dx,20-dx)+Math.min(dy,4-dy);};
    return Object.keys(directions).filter(d=>{
      const [x,y]=directions[d],[px,py]=directions[this.direction],id=this.next(this.body[0],d);
      return !(x+px===0&&y+py===0)&&!this.body.slice(0,-1).includes(id)&&!(this.map[id]==='block'&&!this.switchOpen);
    }).sort((a,b)=>distance(this.next(logicalCell(head.x,head.y),a))-distance(this.next(this.body[0],b)))[0]??this.direction;
  }
  step() {
    if(this.phase!=='running')return;
    this.direction=this.queued??(this.options.autoplay?this.autoDirection():this.direction); this.queued=null;
    let next=this.next(this.body[0],this.direction),visited=new Set();
    // Donor detour/reverse/portal/switch routing, bounded against map loops.
    for(;;) {
      const tile=this.map[next];
      if(!['detour1','detour2','reverse','teleport','switch'].includes(tile))break;
      if(visited.has(next)){this.phase='over';return;} visited.add(next);
      if(tile==='switch') {this.switchOpen=!this.switchOpen; next=this.next(next,this.direction);continue;}
      if(tile==='reverse') {
        this.body.reverse();
        this.direction=Object.keys(directions).find(d=>this.next(this.body[1],d)===this.body[0])??this.direction;
        next=this.next(this.body[0],this.direction);continue;
      }
      if(tile==='teleport') {
        const ports=Object.keys(this.map).map(Number).filter(id=>this.map[id]==='teleport').sort((a,b)=>a-b);
        next=this.next(ports[(ports.indexOf(next)+1)%ports.length],this.direction);continue;
      }
      // DRS detour1 swaps vector axes; detour2 swaps and negates.
      const [dx,dy]=directions[this.direction],sign=tile==='detour1'?1:-1;
      this.direction=Object.keys(directions).find(d=>directions[d][0]===dy*sign&&directions[d][1]===dx*sign);
      next=this.next(next,this.direction);
    }
    const eating=this.food.includes(next),grow=this.growing>0||eating;
    const collision=(this.map[next]==='block'&&!this.switchOpen)||this.body.slice(0,grow?undefined:-1).includes(next);
    if(collision&&!this.options.immortal){this.phase='over';return;}
    if(eating){this.food=this.food.filter(id=>id!==next);this.score++;this.growing+=this.options.growth;}
    if(this.map[next]==='potion')this.options.autoplay=false;
    this.body.unshift(next);
    if(this.growing>0&&this.body.length<=80)this.growing--;else this.body.pop();
    this.tick++;this.fillFood();
  }
  command(action,value) {
    switch(action){
      case 'start': if(this.phase!=='ready')throw Error('Use restart for a new round');this.phase='running';break;
      case 'restart':this.reset();this.phase='running';break;
      case 'pause':if(this.phase==='running')this.phase='paused';break;
      case 'resume':if(this.phase==='paused')this.phase='running';break;
      case 'stop':this.phase='stopped';break;
      case 'direction':return this.turn(value);
      default:throw Error('Unknown game action');
    }
  }
  frame() {
    const colors={block:0xffbb22,detour1:0x22dddd,detour2:0x22dddd,reverse:0xff44cc,teleport:0x4488ff,switch:0xdddd00,potion:0xaa44ff};
    const cells=new Map(Object.entries(this.map).filter(([,t])=>t!=='block'||!this.switchOpen).map(([id,t])=>[+id,colors[t]]));
    for(const id of this.food)cells.set(id,0x27e991);
    for(const id of this.body)cells.set(id,0x125aed);
    cells.set(this.body[0],this.phase==='over'?0xff3344:0x80c4ff);
    return [...cells].map(([id,rgb])=>({id,rgb}));
  }
  snapshot() {return {phase:this.phase,body:[...this.body],food:[...this.food],direction:this.direction,score:this.score,tick:this.tick,options:{...this.options},map:{...this.map},cells:this.frame()};}
}
