// Read-only admission check: never starts a round, takes a lease or sends a tone.
import {Presenter} from './presenter.mjs';
import {WebSocket} from '../../services/client-management/node_modules/ws/wrapper.mjs';
const origin=process.argv[2]??'http://ledcontroller.local';
const presenter=new Presenter({origin,Socket:WebSocket});
try{
  const info=await presenter.connect();
  console.log(JSON.stringify({ok:true,...info,groups:presenter.groups.group_count,
    buzzer:presenter.status.capabilities.acknowledged_features.includes('buzzer'),
    presentation_revision:presenter.layer.revision,game_started:false,output_changed:false},null,2));
}catch(error){console.error(error.message);process.exitCode=1;}
finally{await presenter.close();}
