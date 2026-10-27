// Local RPC controller. No import-time work or implicit LED output.
const [action='status',value,...extra]=process.argv.slice(2);
if(!['open','status','start','pause','resume','restart','direction','stop'].includes(action)||extra.some(v=>v!=='--board'))throw Error('Unknown Snake action');
const origin='http://127.0.0.1:18490';
if(action==='open')console.log(origin+'/snake');
else{
  const session=await fetch(origin+'/api/session').then(r=>r.json());
  if(action==='status')console.log(JSON.stringify(session,(k,v)=>k==='token'?undefined:v,2));
  else{
    const response=await fetch(origin+'/api/snake',{method:'POST',headers:{'Content-Type':'application/json','X-Snake-Token':session.token},
      body:JSON.stringify({action,...(action==='direction'?{value}:{}),...(['start','restart'].includes(action)?{board:value==='--board'||extra.includes('--board')}:{})})});
    const result=await response.json();if(!response.ok)throw Error(result.error);console.log(JSON.stringify(result,null,2));
  }
}
