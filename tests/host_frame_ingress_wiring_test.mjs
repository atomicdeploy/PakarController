import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=path=>readFileSync(new URL('../'+path,import.meta.url),'utf8');
const main=read('firmware/esp/application.ino'),routes=read('firmware/esp/src/UrbootRoutes.h');
function wiring(source,routes) {
  assert.match(source,/hostFrameIngress\.begin\(onHostFrame,rejectHostFrame,revokeHostAdmission\)/);
  assert.match(source,/void revokeHostAdmission\(void\*\) \{hostCallerAdmitted=false;hostDemand\.clear\(\);usbTableCapture\.clear\(\);\}/);
  assert.match(source,/hostLink\.begin\(115200, onHostFrameIngress\)/);
  const ingress=source.slice(source.indexOf('void onHostFrameIngress('),source.indexOf('void serviceHostLink('));
  assert.match(ingress,/observeHostIngressFence\(\)/);
  assert.match(ingress,/hostFrameIngress\.receive\(frame,millis\(\),cloudServices\.ioActive\(\)\)/);
  const serviceStart=source.indexOf('void serviceHostLink(');
  const service=source.slice(serviceStart,source.indexOf('void setAvrTransmitBlocked(',serviceStart));
  assert.match(service,/if\(servicing\)return;servicing=true/);
  assert.equal((source.match(/hostLink\.service\(\)/g)||[]).length,1,'Only the nonrecursive wrapper may enter the RX parser');
  assert.equal((service.match(/hostFrameIngress\.service\(millis\(\),cloudServices\.ioActive\(\)\)/g)||[]).length,2);
  assert.ok(service.indexOf('hostFrameIngress.service(')<service.indexOf('hostLink.service()'));
  assert.ok(service.lastIndexOf('hostFrameIngress.service(')>service.indexOf('hostLink.service()'));
  const progress=source.slice(source.indexOf('cloudServices.setProgressService('),source.indexOf('stream.begin(kStreamPort)',source.indexOf('cloudServices.setProgressService(')));
  assert.match(progress,/serviceHostLink\(\)/);assert.doesNotMatch(progress,/onHostFrame\(/);
  const dispatch=source.slice(source.indexOf('void onHostFrame('),source.indexOf('void rejectHostFrame('));
  assert.match(dispatch,/if\(cloudServices\.ioActive\(\)/,'Reentrant cloud mutation defense stays in actual dispatcher');
  assert.match(dispatch,/PeerCompatibility::enforced && !hostCallerAdmitted && !bootstrap/);
  assert.match(dispatch,/urbootBusy\.get\(\) \|\| remoteFirmware\.busy\(\) \|\| otaActive \|\| otaRebootAt \|\| avrUploadOwner/);
  assert.match(dispatch,/onHostFrame\(command,nullptr\)/,'Boot-fenced table envelope retains the same dispatcher');
  assert.match(source,/otaProgress\.snapshot\(\)\.operation/);
  assert.match(source,/remoteFirmware\.jobId\(\)/);
  assert.match(source,/nextAvrJob=urbootJobId,nextUploadOwner=avrUploadOwner/);
  assert.match(source,/server\.setMaintenanceFence\(true\);hostFrameIngress\.discard\(\)/);
  assert.match(source,/hostFrameIngress\.discard\(\);hostLink\.discardInput\(\)/);
  assert.ok(routes.indexOf('hostFrameIngress.discard()')>routes.indexOf('urbootBusy.set(false)'));
  const admission=routes.slice(routes.lastIndexOf('server.setMaintenanceFence(true)'));
  assert.ok(admission.indexOf('hostFrameIngress.discard()')<admission.indexOf('urbootBusy.set(true)'));
}
wiring(main,routes);
assert.throws(()=>wiring(main.replace('hostLink.begin(115200, onHostFrameIngress)','hostLink.begin(115200, onHostFrame)'),routes));
assert.throws(()=>wiring(main.replace('serviceHostLink(); // Capture/queue only','hostLink.service(); // Capture/queue only'),routes));
assert.throws(()=>wiring(main.replace('if(cloudServices.ioActive()','if(false'),routes));
for(const retired of ['hostCallerAdmitted=false;','hostDemand.clear();','usbTableCapture.clear();']) {
  const complete='void revokeHostAdmission(void*) {hostCallerAdmitted=false;hostDemand.clear();usbTableCapture.clear();}';
  assert.throws(()=>wiring(main.replace(complete,complete.replace(retired,'')),routes),retired+' must remain in session retirement');
}
console.log('USB ingress wiring: actual callback, nonrecursive RX wrapper, cloud-only deferral, bounded before/after drain, maintenance/job fences and unchanged boot/admission/OTA/raw-UART guards; source proof only.');
