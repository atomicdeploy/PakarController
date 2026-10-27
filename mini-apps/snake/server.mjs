// Removable mini-app. Run explicitly; opening /snake never contacts a board.
import http from 'node:http';
import {readFile} from 'node:fs/promises';
import {randomBytes,timingSafeEqual} from 'node:crypto';
import {pathToFileURL} from 'node:url';
import {Snake} from './engine.mjs';
import {Presenter} from './presenter.mjs';

export async function createSnakeServer({port=0,origin='http://ledcontroller.local',enabled=true,PresenterClass=Presenter,Socket}={}) {
  const game=new Snake(),token=randomBytes(24).toString('hex'),clients=new Set();
  let presenter=null,busy=false,error='',timer=null,closed=false,boardOutput=false,lastObserver=0;
  const snapshot=()=>({...game.snapshot(),enabled,error,boardOutput,board:presenter?.info()??null});
  const publish=()=>{const text='data: '+JSON.stringify(snapshot())+'\n\n';for(const client of clients){
    if(client.writableLength>65536){clients.delete(client);client.destroy();lastObserver=Date.now();}
    else client.write(text);
  }};
  const schedule=()=>{clearTimeout(timer);if(closed||!['running','paused'].includes(game.phase))return;
    timer=setTimeout(async()=>{
      if(busy){schedule();return;}busy=true;
      try {
        // Loss of every controller ends the host lease, never runs forever unattended.
        if(!clients.size&&Date.now()-lastObserver>3000){await stop();return;}
        if(game.phase==='running')game.step();
        if(presenter){if(game.phase==='paused')await presenter.renew();else await presenter.frame(game.frame());}
        if(['over','won'].includes(game.phase)){await presenter?.close();presenter=null;}
      }catch(e){error=e.message;game.command('stop');await presenter?.close().catch(()=>{});presenter=null;}
      finally{busy=false;publish();schedule();}
    },game.phase==='paused'?900:game.options.interval);
  };
  async function stop(){clearTimeout(timer);game.command('stop');await presenter?.close();presenter=null;}
  async function command(body){
    if(!enabled)throw Error('Snake disabled for this host');
    if(body.action==='direction'){game.turn(body.value);publish();return;}
    if(busy)throw Error('Previous action is still completing');busy=true;
    try{
      error='';
      if(body.action==='configure'){
        if(['running','paused'].includes(game.phase))throw Error('Stop before configuring');
        // Validate the complete draft before replacing either component.
        const draft=new Snake();draft.configure(body.options??game.options);draft.setMap(body.map??game.map);
        game.configure(draft.options);game.setMap(draft.map);
      }
      else if(body.action==='start'||body.action==='restart'){
        if(body.action==='restart')await stop();
        if(!['ready','stopped','over','won'].includes(game.phase))throw Error('Already active');
        // Output target can change only on an explicit Start/restart, never on open.
        boardOutput=body.board===true;
        if(boardOutput){presenter=new PresenterClass({origin,Socket});await presenter.connect();}
        game.reset();game.command('start');if(presenter)await presenter.frame(game.frame());schedule();
      }else if(body.action==='stop')await stop();
      else {game.command(body.action,body.value);schedule();}
    }catch(e){error=e.message;if(presenter?.uncertain||body.action==='start'||body.action==='restart'){game.command('stop');await presenter?.close().catch(()=>{});presenter=null;}throw e;}
    finally{busy=false;publish();}
  }
  const server=http.createServer(async(req,res)=>{
    try{
      const expected=`127.0.0.1:${server.address().port}`;
      if(req.headers.host!==expected){res.writeHead(403).end();return;}
      const url=new URL(req.url,`http://${expected}`);
      res.setHeader('Cache-Control','no-store');res.setHeader('X-Content-Type-Options','nosniff');
      res.setHeader('Content-Security-Policy',"default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'");
      if(req.method==='GET'&&url.pathname==='/api/events'){
        if(clients.size>=8){res.writeHead(503).end();return;}
        res.writeHead(200,{'Content-Type':'text/event-stream'});clients.add(res);lastObserver=Date.now();publish();
        req.on('close',()=>{clients.delete(res);lastObserver=Date.now();});return;
      }
      if(req.method==='GET'&&url.pathname==='/api/session'){res.writeHead(200,{'Content-Type':'application/json'}).end(JSON.stringify({token,...snapshot()}));return;}
      if(req.method==='POST'&&url.pathname==='/api/snake'){
        const supplied=Buffer.from(String(req.headers['x-snake-token']??'')),secret=Buffer.from(token);
        if(supplied.length!==secret.length||!timingSafeEqual(supplied,secret)||req.headers.origin&&req.headers.origin!==`http://${expected}`){res.writeHead(403).end();return;}
        let text='';for await(const part of req){text+=part;if(text.length>8192){res.writeHead(413).end();return;}}
        await command(JSON.parse(text));res.writeHead(200,{'Content-Type':'application/json'}).end(JSON.stringify(snapshot()));return;
      }
      const assets={'/snake':'index.html','/snake/':'index.html','/':'index.html','/app.mjs':'app.mjs','/style.css':'style.css'};
      if(req.method!=='GET'||!assets[url.pathname]){res.writeHead(404).end();return;}
      res.writeHead(200,{'Content-Type':url.pathname.endsWith('.mjs')?'text/javascript':url.pathname.endsWith('.css')?'text/css':'text/html; charset=utf-8'});
      res.end(await readFile(new URL(assets[url.pathname],import.meta.url)));
    }catch(e){if(!res.headersSent)res.writeHead(400,{'Content-Type':'application/json'});res.end(JSON.stringify({error:e.message}));}
  });
  server.requestTimeout=5000;
  await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(port,'127.0.0.1',resolve);});
  return {url:`http://127.0.0.1:${server.address().port}/snake`,token,snapshot,
    async close(){closed=true;clearTimeout(timer);await stop().catch(()=>{});for(const c of clients)c.end();await new Promise(resolve=>server.close(resolve));}};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href){
  try{
    const options={port:18490};
    const args=process.argv.slice(2);
    for(let i=0;i<args.length;i++){
      if(args[i]==='--disabled'){options.enabled=false;continue;}
      if(args[i]==='--origin'&&args[i+1])options.origin=args[++i];
      else if(args[i]==='--port'&&/^\d+$/.test(args[i+1]??''))options.port=Number(args[++i]);
      else throw Error('Use --origin http://ledcontroller.local --port 18490 [--disabled]');
    }
    // Reuse the already pinned service dependency; HTTP works if it is absent.
    try{options.Socket=(await import('../../services/client-management/node_modules/ws/wrapper.mjs')).WebSocket;}catch{}
    const app=await createSnakeServer(options);console.log(app.url+' (opened = stopped; Start is explicit)');
    for(const signal of ['SIGINT','SIGTERM'])process.once(signal,()=>app.close().finally(()=>process.exit()));
  }catch(e){console.error(e.message);process.exitCode=1;}
}
