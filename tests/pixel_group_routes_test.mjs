import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const read=p=>readFileSync(new URL(`../${p}`,import.meta.url),'utf8');
const app=read('firmware/esp/application.ino'),usb=read('firmware/esp/src/UsbApiService.h');
const grid=read('firmware/esp/src/LedGrid.cpp'),store=read('firmware/esp/src/PixelGroupStore.h');
for(const route of ['/api/pixels/groups','/api/pixels/frame']){
  assert(app.includes(`server.on("${route}",HTTP_GET`));
  assert(usb.includes(`path=="${route}"`));
}
assert(app.includes('server.on("/api/pixels/groups",HTTP_POST'));
assert(app.includes('server.hasArg("configuration")?server.arg("configuration"):server.jsonBody()'));
for(const text of ['maintenanceApiIdle()','grid.canConfigureGroups()','pixelCorrections.previewActive()','grid.backendPending()','ScopedLedFlashPause pause','pixelGroups.save(request)','grid.configureGroups(pixelGroups.layout)'])assert(app.includes(text));
assert(app.includes('pixel-group-layout'));
assert(app.includes('pixelGroups.begin(LittleFS,filesystemMounted)'));
assert(app.indexOf('pixelGroups.begin(LittleFS,filesystemMounted)')<app.indexOf('sceneStore.begin('));
assert(usb.includes('reply["pixel_groups"]'));
assert(usb.includes('section=="pixel_groups"'));
assert(usb.includes('savePixelGroups(settings)'));
const binding=read('firmware/esp/src/ShellTuiBinding.h');
assert(binding.includes('grid.groupLayout()'));
assert(binding.includes('d["count"]=action.count'));
assert(binding.includes('d["group_layout_revision"]=action.groupLayoutRevision'));
assert(!binding.includes('action.group?6:1'),'Device TUI uses the same revision-fenced physical spans');
// General file access is a read-only /user namespace, not an alternate settings
// writer. Both durable and staged topology paths remain behind validated APIs.
const userPath=usb.slice(usb.indexOf('bool maintenanceUserPath('),usb.indexOf('String maintenanceApi('));
assert(userPath.includes('!path.startsWith("/user/")'));
assert(userPath.includes('path.indexOf("..")<0'));
assert(usb.includes('reply["file_write"]=false'));
assert(usb.includes('if(!maintenanceUserPath(filename))return maintenanceApiError("protected_or_invalid_path")'));
const artifacts=read('firmware/esp/src/ArtifactDownload.h');
assert(!artifacts.includes('pixel-groups'));
assert(artifacts.includes('if(!path){request->send(400'));
for(const text of ['pixel_group_layout_changed_refresh_before_diagnostic','group_layout_revision','grid.groupLayout().customized()'])assert(app.includes(text));
const binary=app.slice(app.indexOf('bool applyPixelDiagnostic(uint16_t index,uint8_t count,uint32_t rgb,bool off) {'),app.indexOf('String pixelGroupsJson()'));
assert(binary.includes('if(grid.groupLayout().customized())return false;'));
for(const field of ['group_layout_revision','group_counts','group_starts'])assert(grid.includes(`\\"${field}\\"`));
assert(grid.includes('publishedGroups_=groupLayout_'));
assert(grid.includes('groupLayout_.diagnosticRange(index,count)'));
assert(!grid.includes('*kSectorsPerCell')&&!grid.includes('/kSectorsPerCell'),'All group coordinate arithmetic uses the shared topology');
for(const text of ['pixel_group_layout_changed','pixel_frame_layout_pending','grid.publishedGroupRevision()!=expectedRevision','strictU32(server.arg("group_layout_revision")'])assert(app.includes(text));
assert(usb.includes('body["group_layout_revision"].is<uint32_t>()'));
for(const text of ['pixel_groups_conflict','pixel_groups_damaged','pixel_groups_revision_exhausted','pixel_groups_verify_failed','pixel_groups_commit_failed'])assert(store.includes(text));
assert(store.includes('file.readString()==bytes'));
assert(!store.includes('format('),'No destructive initialization on invalid records');
assert(read('firmware/esp/src/DiagnosticReceipt.h').includes('"pixel_groups"'));
// Receipts are shared even when correction storage is compiled out (ESP8266).
assert.match(app,/#if PAKAR_PIXEL_CORRECTIONS\s+#include "src\/PixelCorrectionStore.h"\s+#endif\s+#include "src\/DiagnosticReceipt.h"/);
assert(app.includes('pixel_frame_partial\\":true'));
assert(app.includes('status_partial\\":true'));
assert(app.includes('capabilities.application.icon_png_base64'));
assert(read('tools/test-protocol.ps1').includes("'test-pixel-groups.ps1'"));
assert(read('tools/test-protocol.sh').includes('pixel_group_renderer_test.cpp'));
console.log('Pixel group shared REST/USB routes, revision fences, persisted ownership and immutable telemetry source contracts passed');
