import 'dart:typed_data';

import 'package:crypto/crypto.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/firmware.dart';

import 'firmware_test.dart' show MaintenanceClient, jsonResponse, firmwareJob;

// Header fixture only; SDK/signature/physical flash acceptance is separate.
Uint8List image(int size, {bool s3 = true, int chip = 9, bool hashed = true}) {
  final bytes = Uint8List(size)
    ..[0] = 0xe9
    ..[1] = 1;
  if (s3) {
    ByteData.sublistView(bytes).setUint16(12, chip, Endian.little);
    bytes[23] = hashed ? 1 : 0;
  }
  return bytes;
}

void main() {
  test('Actual migrated 2220352-byte S3 image fits, but slot capacity is exclusive', () {
    validateFirmwareImage(true, image(2220352));
    validateFirmwareImage(true, image(FirmwareImagePolicy.maximumBytes));
    expect(
      () => validateFirmwareImage(true, image(FirmwareImagePolicy.s3Capacity)),
      throwsFormatException,
    );
    expect(
      () => validateFirmwareImage(true, image(2220352), otaCapacity: 2220352),
      throwsFormatException,
    );
    validateFirmwareImage(true, image(2220352), otaCapacity: 2220353);
    expect(FirmwareImagePolicy.maximum(esp: true), 0x32ffff);
    expect(FirmwareImagePolicy.maximum(esp: false), 32768);
  });

  test(
    'Wrong chip, unhashed, malformed segments and known wrong platform reject',
    () {
      for (final invalid in [
        image(2220352, chip: 5),
        image(2220352, hashed: false),
        image(2220352)..[1] = 0,
        image(2220352)..[1] = 17,
      ]) {
        expect(
          () => validateFirmwareImage(true, invalid),
          throwsFormatException,
        );
      }
      expect(
        () => validateFirmwareImage(
          true,
          image(1024, chip: 5),
          platform: EspFirmwarePlatform.esp32s3,
        ),
        throwsFormatException,
      );
      expect(
        () => validateFirmwareImage(
          true,
          image(1024),
          platform: EspFirmwarePlatform.esp8266,
        ),
        throwsFormatException,
      );
      expect(
        () => validateFirmwareImage(
          true,
          image(1024, s3: false),
          platform: EspFirmwarePlatform.esp32s3,
        ),
        throwsFormatException,
      );
    },
  );

  test('Legacy E9 and AVR retain strict independent bounds; byte23 is not a legacy hash flag', () {
    for (final codeByte in [0, 1, 0xfe]) {
      validateFirmwareImage(true, image(4096, s3: false)..[23] = codeByte);
    }
    validateFirmwareImage(true, image(0xfffff, s3: false));
    expect(
      () => validateFirmwareImage(true, image(0x100000, s3: false)),
      throwsFormatException,
    );
    validateFirmwareImage(false, Uint8List(32768));
    for (final size in [0, 32767, 32769, 2220352]) {
      expect(
        () => validateFirmwareImage(false, Uint8List(size)),
        throwsFormatException,
      );
    }
  });

  test(
    'Only explicitly observed selected-board capacity narrows the fixed bound',
    () {
      expect(
        FirmwareImagePolicy.platform({'board_id': 'esp32s3-test'}),
        EspFirmwarePlatform.esp32s3,
      );
      expect(
        FirmwareImagePolicy.platform({'board_id': 'esp8266-test'}),
        EspFirmwarePlatform.esp8266,
      );
      expect(
        FirmwareImagePolicy.capacity({'free': 99, 'flash_bytes': 99999999}),
        isNull,
      );
      expect(
        FirmwareImagePolicy.capacity({
          'esp': {'ota_capacity_bytes': 2220353},
        }),
        2220353,
      );
      expect(
        FirmwareImagePolicy.maximum(esp: true, otaCapacity: 0xffffffff),
        0x32ffff,
      );
      for (final invalid in ['3342336', 24, 0, -1, true]) {
        expect(
          () => FirmwareImagePolicy.capacity({'ota_capacity_bytes': invalid}),
          throwsFormatException,
        );
      }
    },
  );

  test('Hashed HTTP S3 source above 2MiB uploads once, and export uses the same policy', () async {
    final bytes = image(2220352);
    var writes = 0, downloads = 0;
    final api = PakarApi(
      client: MaintenanceClient((request) async {
        if (request.url.path == '/api/status') {
          return jsonResponse({
            'hostname': 'ledboard',
            'board_id': 'esp32s3-test',
            'boot_id': 'fixture-boot',
          });
        }
        if (request.url.path == '/api/firmware/remote') {
          return jsonResponse(firmwareJob());
        }
        if (request.url.path == '/firmware/download') {
          return http.StreamedResponse(
            Stream.value(bytes),
            200,
            contentLength: bytes.length,
            headers: {'x-firmware-md5': md5.convert(bytes).toString()},
          );
        }
        expect(request.url.path, '/update');
        expect(request.method, 'POST');
        writes++;
        expect(request.headers['X-Pakar-Firmware-Size'], '${bytes.length}');
        expect(
          (request as http.MultipartRequest).files.single.length,
          bytes.length,
        );
        return jsonResponse({'updated': true, 'rebooting': true});
      }),
    );
    final source = MaintenanceClient((request) async {
      downloads++;
      expect(request.followRedirects, false);
      expect(request.headers['Authorization'], isNull);
      return http.StreamedResponse(
        Stream.value(bytes),
        200,
        contentLength: bytes.length,
      );
    });
    final controller = FirmwareController(api, authorized: (_) => true);
    try {
      await controller.fileUrl(
        'https://updates.example/esp.bin',
        sha256.convert(bytes).toString(),
        true,
        downloadClient: source,
      );
      expect(writes, 1);
      expect(downloads, 1);
      expect(await controller.espBytes(), bytes);
      await expectLater(
        controller.fileUrl(
          'https://updates.example/esp.bin',
          '',
          true,
          downloadClient: source,
        ),
        throwsFormatException,
      );
      expect(downloads, 1);
      await expectLater(
        controller.fileUrl(
          'https://updates.example/esp.bin',
          '0' * 64,
          true,
          downloadClient: source,
        ),
        throwsFormatException,
      );
      expect(writes, 1);
    } finally {
      await api.close();
      source.close();
    }
  });

  test(
    'Wrong chip or observed small slot cannot reach manual mutation',
    () async {
      var writes = 0;
      final api = PakarApi(
        client: MaintenanceClient((request) async {
          if (request.url.path == '/api/status') {
            return jsonResponse({
              'hostname': 'ledboard',
              'board_id': 'esp32s3-test',
              'boot_id': 'fixture-boot',
              'ota_capacity_bytes': 2220352,
            });
          }
          if (request.url.path == '/api/firmware/remote') {
            return jsonResponse(firmwareJob());
          }
          writes++;
          return jsonResponse({'updated': true, 'rebooting': true});
        }),
      );
      final controller = FirmwareController(api, authorized: (_) => true);
      try {
        await expectLater(
          controller.manual('update', bytes: image(1024, chip: 5)),
          throwsFormatException,
        );
        await expectLater(
          controller.manual('update', bytes: image(2220352)),
          throwsFormatException,
        );
        expect(writes, 0);
      } finally {
        await api.close();
      }
    },
  );
}
