// RFC 1952 permits OS=255 (unknown). Binary assets must not differ merely
// because zlib was built for Windows or Unix. No compressed data/CRC is changed.
import assert from 'node:assert/strict';
import {gzipSync} from 'node:zlib';
export function normalizeGzipPlatform(bytes) {
  const result=Buffer.from(bytes);
  assert(result.length>=18&&result[0]===31&&result[1]===139&&result[2]===8,
    'Expected gzip/deflate stream');
  // Reject optional fields/header CRC rather than edit a checksummed header.
  assert.equal(result[3],0,'Portable binary gzip must have no optional fields');
  assert.equal(result.readUInt32LE(4),0,'Portable binary gzip must have no timestamp');
  result[9]=255;
  return result;
}
export function portableGzip(bytes) {
  return normalizeGzipPlatform(gzipSync(bytes,{level:9}));
}
