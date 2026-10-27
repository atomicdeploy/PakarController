import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const context=vm.createContext({});
vm.runInContext(readFileSync(new URL('../web/modules/table-layout.js',import.meta.url),'utf8'),context);
const {zoomAt,position}=context.WebTableLayout;
for(const factor of [.01,.5,1,2,100]) {
 const before={zoom:1,x:-20,y:15},p={x:200,y:80},after=zoomAt(before,factor,p);
 assert(after.zoom>=.5&&after.zoom<=8);
 assert.equal((p.x-after.x)/after.zoom,(p.x-before.x)/before.zoom);
 assert.equal((p.y-after.y)/after.zoom,(p.y-before.y)/before.zoom);
}
assert.equal(position(1).x,0);assert.equal(position(11).y,.5);
for(const factor of [NaN,Infinity,0,-1])assert.equal(zoomAt({zoom:1,x:0,y:0},factor,{x:20,y:30}).zoom,1);
console.log('Table zoom: anchor, clamp and independent logical geometry verified.');
