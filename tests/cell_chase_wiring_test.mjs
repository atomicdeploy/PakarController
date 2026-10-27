import {readFileSync} from 'node:fs';
import assert from 'node:assert/strict';
const read=p=>readFileSync(new URL('../'+p,import.meta.url),'utf8');
const sketch=read('firmware/esp/application.ino');
assert.match(sketch,/uint8_t defaultPhysicalCell\(uint8_t logicalCell\)\s*{\s*#if PAKAR_FIRST_WINS_ENABLED\s*return logicalCell >= 1 && logicalCell <= LedGrid::kCells \? logicalCell : 0;\s*#else\s*return BoardCore::TableCellMapping::physicalForLogical\(logicalCell\);\s*#endif\s*}/,
  'Keys retain direct group order while the table profile retains its calibrated cable mapping');
assert.match(sketch,/loadLedPreferences\(\);\s+loadCellMapping\(\);[\s\S]*?sceneStore.begin[\s\S]*?#if PAKAR_LED_START_CELL_CHASE\s+grid.setStaticMode\(true\);\s+grid.setPattern\(LedGrid::kPatternCellChase\)/);
assert.match(sketch,/value == F\("cell-chase"\)\) return LedGrid::kPatternCellChase/);
const preset=sketch.slice(sketch.indexOf('if(server.hasArg("preset"))'),sketch.indexOf('const int logical = server.arg("logical_id")'));
assert.match(preset,/server.arg\("preset"\)!=F\("table-cable"\)/);
assert.match(preset,/cellMapping=previous/);
assert.equal((preset.match(/saveCellMapping\(\)/g)||[]).length,1);
assert.doesNotMatch(preset,/names\[|memset\(/); // Preserve custom cell names.
assert.match(preset,/grid.setPhysicalMapping\(cellMapping.physical,LedGrid::kCells\)/);
const build=read('tools/build-firmware.ps1');
assert.match(build,/\[switch\] \$CellChaseOnBoot/);
assert.match(build,/if \(\$CellChaseOnBoot\) \{ \$flags \+= ' -DPAKAR_LED_START_CELL_CHASE=1' \}/);
assert.doesNotMatch(build,/\$flags \+= ' -DPAKAR_S3_LED_USE_DMA=1'/);
assert.match(build,/config\/esp32s3\/LedBackend.h/);
assert.match(build,/ReadAllText\(\$ledProfile.Path\) -cne \$ledProfile.Content/);
assert.match(build,/\[IO.FileShare\]::None/);
assert.match(build,/finally \{\s+if \(\$ledProfileLease\) \{ \$ledProfileLease.Dispose\(\)/);
assert.match(read('firmware/esp32s3/src/LedOutput.h'),/__has_include\(<build\/config\/esp32s3\/LedBackend.h>\)/);
const worker=read('firmware/esp32s3/src/LedOutput.cpp');
assert.match(worker,/if\(initialBlack\)::memset\(payload_,0,bytes\);\s+else encodeWireBytes\(current_\.rgb/,
  'Only startup clears black; runtime driver changes replay the current complete frame');
assert.match(worker,/rmt_new_bytes_encoder\(/);
assert.match(worker,/rmt_transmit\(channel_,encoder_,payload_,bytes,&tx\)/);
assert.match(worker,/xQueueReceive\(queue_,&current_,pdMS_TO_TICKS\(10\)\)/);
assert.doesNotMatch(worker,/have&&\(!blackSent\)|blackSent=!current_.enabled/);
console.log('Cell chase wiring: boot opt-in after mapping, named preset, shared ESP effect parser, DMA startup black and no unsubmitted replay. Source only, not physical acceptance.');
