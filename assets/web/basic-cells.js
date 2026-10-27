// Actual shared createPeerHttp owns identity fencing, deadlines and no replay.
// PCController peripheral-availability.ts: ready AND explicitly advertised.
function createBasicCells({http,changed=()=>{}}){
  let observed=null,selected=1,busy=false,message='Unknown. Refresh to read the board.',authInfo=null;
  function table(s){const c=s?.capabilities,f=c?.acknowledged_features;return c?.board_kind==='table'&&Array.isArray(f)&&f.includes('table')&&f.includes('leds')&&c.table?.columns===10&&c.table?.rows===8&&c.table?.pixels_per_cell===6&&s.pixels===480&&Array.isArray(s.cells)&&s.cells.length===80&&s.cells.every(v=>['white','red','off'].includes(v))}
  function selectionRole(s){const c=s?.capabilities;return c?.role==='table-selection'||(c?.role===undefined&&(c?.modes===undefined||c.modes.some(m=>m?.id==='cell'&&m.code===1)))}
  function batch(s){return selectionRole(s)&&s?.capabilities?.acknowledged_features?.includes('cell-batch')===true}
  function supportedDriver(s){const d=s?.led_output?.driver;return d?.ready===true&&((d.backend==='gpio-static'&&d.dma===false)||(d.backend==='rmt-dma'&&d.dma===true))}
  function ready(s){return table(s)&&batch(s)&&s.led_output?.enabled===true&&s.led_output.mode==='static'&&supportedDriver(s)}
  function reason(){if(!observed)return 'Refresh to load device state.';if(!batch(observed))return 'Update board firmware to enable bulk cell control.';if(!supportedDriver(observed))return 'LED driver unavailable. Check device settings.';if(observed.led_output?.enabled!==true||observed.led_output.mode!=='static')return 'Enable static output to control cells here.';return 'Static output ready · '+(observed.led_output.driver.dma?'RMT DMA':'GPIO')}
  function snapshot(){return {selected,busy,known:!!observed,cells:observed?observed.cells.slice():Array(80).fill(null),hostname:observed?.hostname||'',message,reason:reason(),canWrite:!busy&&ready(observed),canEnable:!busy&&table(observed)&&batch(observed)&&supportedDriver(observed),authInfo:authInfo?{...authInfo}:null}}
  function publish(){changed(snapshot())}
  function check(s){if(!table(s))throw Error('This peer has not acknowledged the 80-cell / 480-pixel table.');return s}
  function unknown(error){observed=null;http.revoke();message=(error?.message||'Connection failed.')+' Unknown; Refresh needed. A sent command was not retried.'}
  async function perform(action){if(busy)return false;busy=true;publish();try{return await action()}catch(error){unknown(error);return false}finally{busy=false;publish()}}
  function select(id){if(!Number.isInteger(id)||id<1||id>80)return false;selected=id;publish();return true}
  async function refresh(){return perform(async()=>{const info=await http.request('/api/auth/info');authInfo={enabled:info?.enabled===true,username:typeof info?.username==='string'?info.username:'',default_credentials:info?.enabled===true&&info?.default_credentials===true};const s=check(await http.request('/api/status'));http.pin();observed=s;message='Board status read. Select a logical cell.';return true})}
  async function loadAuthInfo(){return perform(async()=>{const value=await http.request('/api/auth/info');authInfo={enabled:value?.enabled===true,username:typeof value?.username==='string'?value.username:'',default_credentials:value?.enabled===true&&value?.default_credentials===true};return true})}
  async function login(username,password){return perform(async()=>{const ack=await http.request('/login',{method:'POST',body:new URLSearchParams({username,password})});if(ack?.authenticated!==true)throw Error('Sign-in was not acknowledged.');observed=check(await http.request('/api/status'));http.pin();message='Signed in. Board status read.';return true})}
  async function write(kind,value){
    if(busy)return false;
    if(kind==='cell'&&!['white','red'].includes(value))return false;
    const id=selected;
    return perform(async()=>{
      const previous=observed;if(!previous)throw Error('Read the board before writing.');
      // Refresh stale admission without transferring an old click to a new boot.
      const before=check(await http.request('/api/status'));
      if(before.board_id!==previous.board_id||before.boot_id!==previous.boot_id)throw Error('Board or boot changed. Refresh before a new command.');
      const peer=http.pin();
      if(kind==='cell'&&!ready(before)){observed=before;message=reason();return false}
      if(kind==='cell'&&value==='white'&&before.red_lock===true&&before.cells[id-1]==='red'){observed=before;message='Red-cell lock prevents this cell changing to white. Unlock it in the full application first.';return false}
      if(kind==='enable'&&(!batch(before)||!supportedDriver(before))){observed=before;message=reason();return false}
      const path=kind==='cell'?'/api/cells':'/api/led/output?enabled=true&mode=static';
      const body=kind==='cell'?new URLSearchParams({ids:String(id),state:value,effect:'immediate'}):'';
      const ack=await http.request(path,{method:'POST',peer,body});
      if(kind==='cell'?ack?.accepted!==true||ack.count!==1:ack?.enabled!==true||ack.mode!=='static')throw Error('Command was not acknowledged.');
      const after=check(await http.request('/api/status',{peer}));
      if(!ready(after)||(kind==='cell'&&after.cells[id-1]!==value))throw Error('Board readback did not confirm the requested state and static rendering.');
      observed=after;message=kind==='cell'?`Device confirmed cell ${id}: ${value}`:'Static output enabled';return true;
    });
  }
  return {snapshot,select,refresh,loadAuthInfo,login,setColor:value=>write('cell',value),enableStatic:()=>write('enable')};
}
if(typeof document!=='undefined'){
  const byId=id=>document.getElementById(id),buttons=[];
  function renderBasic(s){
    byId('basicStatus').textContent=s.message;byId('basicReadiness').textContent=s.reason;
    byId('basicBoard').textContent=s.hostname?s.hostname+'.local':'No verified board';
    byId('basicLogin').hidden=s.known||s.authInfo?.enabled!==true;
    byId('basicSelection').textContent='Logical cell '+s.selected;
    byId('basicWhite').disabled=byId('basicRed').disabled=!s.canWrite;byId('basicEnable').disabled=!s.canEnable;byId('basicRefresh').disabled=s.busy;byId('basicSignIn').disabled=s.busy;
    byId('basicDefaultHint').hidden=s.authInfo?.default_credentials!==true;
    if(s.authInfo?.username&&!byId('basicUsername').value)byId('basicUsername').value=s.authInfo.username;
    buttons.forEach((button,index)=>{button.classList.toggle('selected',s.selected===index+1);button.classList.toggle('red',s.known&&s.cells[index]==='red');button.setAttribute('aria-pressed',String(s.selected===index+1));button.setAttribute('aria-label',`Logical cell ${index+1}, ${s.cells[index]??'Unknown'}`)});
  }
  const controller=createBasicCells({http:createPeerHttp(),changed:renderBasic});
  for(let id=1;id<=80;id++){const b=document.createElement('button');b.type='button';b.className='basic-cell';b.textContent=String(id);b.onclick=()=>controller.select(id);byId('basicGrid').appendChild(b);buttons.push(b)}
  byId('basicWhite').onclick=()=>controller.setColor('white');byId('basicRed').onclick=()=>controller.setColor('red');
  byId('basicEnable').onclick=()=>controller.enableStatic();byId('basicRefresh').onclick=()=>controller.refresh();
  byId('basicLogin').onsubmit=async event=>{event.preventDefault();const password=byId('basicPassword').value;byId('basicPassword').value='';await controller.login(byId('basicUsername').value,password)};
  renderBasic(controller.snapshot());controller.refresh();
}
