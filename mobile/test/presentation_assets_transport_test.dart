import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/presentation_assets.dart';

void main() {
  test('Actual HTTP data updates coalesce, verify and follow notices without any APK', () async {
    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final origin = 'http://127.0.0.1:${server.port}';
    var revision = 1, manifests = 0;
    final firstRead = Completer<void>(), release = Completer<void>();
    String bundle() => jsonEncode({
      'revision': revision,
      'palette': {'primary': revision == 1 ? '68a7ff' : '38d27a'},
    });
    String digest() => sha256.convert(utf8.encode(bundle())).toString();
    final subscription = server.listen((request) async {
      expect(request.method, 'GET');
      expect(request.headers.value('authorization'), isNull);
      request.response.headers.contentType = ContentType.json;
      if (request.uri.path == '/manifest') {
        manifests++;
        if (!firstRead.isCompleted) {
          firstRead.complete();
          await release.future;
        }
        request.response.write(
          jsonEncode({
            'revision': revision,
            'url': '$origin/bundle',
            'bytes': utf8.encode(bundle()).length,
            'sha256': digest(),
          }),
        );
      } else {
        request.response.write(bundle());
      }
      await request.response.close();
    });
    final saved = <String>[];
    final assets = PresentationAssets(save: (value) async => saved.add(value))
      ..source = '$origin/manifest';
    try {
      final read = assets.checkNow();
      await firstRead.future;
      expect(identical(read, assets.checkNow()), true);
      release.complete();
      await read;
      expect(manifests, 1);
      expect(assets.revision, 1);
      expect(assets.value['primary'], 0x68a7ff);
      expect(saved, hasLength(1));
      revision = 2;
      final notice = {
        'revision': 2,
        'manifest_url': assets.source,
        'sha256': digest(),
      };
      assets.enabled = false;
      await assets.acceptNotice(notice);
      expect(manifests, 1);
      assets.enabled = true;
      await expectLater(
        assets.acceptNotice({...notice, 'manifest_url': '$origin/foreign'}),
        throwsFormatException,
      );
      await assets.acceptNotice(notice);
      expect(assets.revision, 2);
      expect(assets.value['primary'], 0x38d27a);
      await assets.acceptNotice(notice);
      expect(manifests, 2);
    } finally {
      if (!release.isCompleted) release.complete();
      assets.dispose();
      await subscription.cancel();
      await server.close(force: true);
    }
  });
}
