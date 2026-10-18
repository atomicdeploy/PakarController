// Runs the real Flutter transport on an adjacent PC, without installing an SDK
// there. This is command/buffer evidence, never Android UI or optical acceptance.
import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:pakarcontroller/api.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/pixel_diagnostic.dart';

Future<Map<String, dynamic>> networkReadback(String endpoint) async {
  final uri = Uri.parse(endpoint).replace(path: '/api/status');
  final result = <String, dynamic>{};
  var addresses = <InternetAddress>[];
  try {
    addresses = await InternetAddress.lookup(uri.host)
        .timeout(const Duration(seconds: 2));
    result['addresses'] = addresses.map((a) => a.address).toList();
  } catch (error) {
    result['dns_error'] = '$error';
  }
  Future<Map<String, dynamic>> read(Uri target) async {
    final client = HttpClient()
      ..findProxy = ((_) => 'DIRECT')
      ..connectionTimeout = const Duration(seconds: 2);
    try {
      final request = await client
          .getUrl(target)
          .timeout(const Duration(seconds: 3));
      final response = await request.close().timeout(
        const Duration(seconds: 3),
      );
      final bytes = await response
          .fold<BytesBuilder>(BytesBuilder(), (all, chunk) {
            if (all.length + chunk.length > 16384) {
              throw const FormatException('Oversized status');
            }
            return all..add(chunk);
          })
          .timeout(const Duration(seconds: 3));
      final status = jsonDecode(utf8.decode(bytes.takeBytes())) as Map;
      return {
        'host': target.host,
        'http': response.statusCode,
        'board_id': status['board_id'],
        'boot_id': status['boot_id'],
      };
    } catch (error) {
      return {'host': target.host, 'error': '$error'};
    } finally {
      client.close(force: true);
    }
  }

  result['default_client'] = await Future.wait([
    read(uri),
    if (uri.scheme == 'http')
      for (final address in addresses.take(2))
        read(uri.replace(host: address.address)),
  ]);
  return result;
}

Future<void> main(List<String> args) async {
  final options = <String, String>{};
  for (var i = 0; i + 1 < args.length; i += 2) {
    options[args[i]] = args[i + 1];
  }
  if (args.length != 8 ||
      options.length != 4 ||
      options.keys.toSet().difference({
        '--endpoint',
        '--board',
        '--boot',
        '--receipt',
      }).isNotEmpty) {
    stderr.writeln(
      'Usage: pixel-probe --endpoint URL --board ID --boot ID --receipt NEW.json',
    );
    exitCode = 2;
    return;
  }
  final receipt = File(options['--receipt']!);
  if (!receipt.isAbsolute || receipt.existsSync()) {
    throw const FormatException('Use an absolute, nonexisting receipt');
  }
  receipt.createSync(exclusive: true);
  final output = receipt.openSync(mode: FileMode.writeOnly);
  final api = PakarApi(baseUrl: options['--endpoint']!);
  StreamSubscription<DeviceStatus>? live;
  final proof = <String, dynamic>{
    'board': options['--board'],
    'boot': options['--boot'],
    'scope': 'actual-mobile-transport-not-android-or-optical',
    'steps': <Map<String, dynamic>>[],
    'verified': false,
  };
  try {
    final ready = Completer<DeviceStatus>();
    live = api.liveStatus().listen(
      (value) {
        if (!ready.isCompleted) ready.complete(value);
      },
      onError: (Object error) {
        if (!ready.isCompleted) ready.completeError(error);
      },
    );
    final before = await ready.future.timeout(const Duration(seconds: 8));
    proof['observed_board'] = before.json['board_id'];
    proof['observed_boot'] = before.json['boot_id'];
    if (before.json['board_id'] != options['--board'] ||
        before.json['boot_id'] != options['--boot'] ||
        !before.peer.has('pixel-diagnostic') ||
        before.mode != 'cell' ||
        before.effect != 'none') {
      throw const FormatException(
        'Exact boot, atomic capability and nonanimated cell mode required',
      );
    }
    final pixels = DeviceStatus(await api.getJson('/api/status?pixels=1'));
    if (pixels.json['board_id'] != options['--board'] ||
        pixels.json['boot_id'] != options['--boot'] ||
        !matchesDiagnosticFrame(pixels, Uint8List(1440))) {
      throw const FormatException(
        'Do not overwrite an active or unknown raw frame',
      );
    }
    final owner = api.connectionRevision;
    for (final step in [
      (const PixelProbe(), false),
      (const PixelProbe(index: 5, rgb: 0xff0000), false),
      (const PixelProbe(index: 6, rgb: 0x0000ff), false),
      (const PixelProbe(index: 479, rgb: 0x00ff00), false),
      (const PixelProbe(index: 479, group: true, rgb: 0xff0000), false),
      (const PixelProbe(index: 479), true),
    ]) {
      final clock = Stopwatch()..start();
      final actual = await api.pixelDiagnostic(
        step.$1,
        off: step.$2,
        authorize: () => owner == api.connectionRevision,
      );
      if (!matchesDiagnosticFrame(actual, step.$1.frame(off: step.$2))) {
        throw const FormatException('Complete frame differed');
      }
      final row = {
        'first': step.$1.first,
        'count': step.$1.count,
        'rgb': step.$1.hex,
        'off': step.$2,
        'elapsed_ms': clock.elapsedMilliseconds,
        'revision': (actual.json['pixel_frame'] as Map)['revision'],
      };
      (proof['steps'] as List).add(row);
      stdout.writeln(jsonEncode(row));
    }
    proof['verified'] = true;
    proof['all_off_confirmed'] = true;
  } catch (error) {
    proof['error'] = '$error';
    proof['network_readback'] = await networkReadback(options['--endpoint']!);
    exitCode = 1;
  } finally {
    try {
      output.writeStringSync(const JsonEncoder.withIndent('  ').convert(proof));
    } finally {
      output.closeSync();
    }
    await api.close();
    try {
      await live?.cancel().timeout(const Duration(seconds: 1));
    } on TimeoutException {
      /* Retired read-only stream. */
    }
  }
  stdout.writeln(jsonEncode(proof));
  await stdout.flush();
  exit(exitCode); // Own CLI process only; never a controller/app/server.
}
