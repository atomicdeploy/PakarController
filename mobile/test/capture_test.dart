import 'dart:io';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

void main() {
  final path = Platform.environment['PAKAR_MOBILE_CAPTURE_PATH'];
  testWidgets(
    'Explicit simulated table renderer capture, no production IPC or device writes',
    (tester) async {
      if (path == null) return;
      final output = File(path);
      expect(output.isAbsolute, true);
      expect(
        output.existsSync(),
        false,
        reason: 'Never overwrite capture evidence',
      );
      for (final font in ['Vazirmatn', 'Phosphor']) {
        final loader = FontLoader(font)
          ..addFont(
            rootBundle.load(
              font == 'Vazirmatn'
                  ? 'assets/fonts/Vazirmatn-Regular.ttf'
                  : 'assets/fonts/Phosphor.ttf',
            ),
          );
        await loader.load();
      }
      tester.view.physicalSize = const Size(1100, 1500);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final boundary = GlobalKey();
      await tester.pumpWidget(
        RepaintBoundary(
          key: boundary,
          child: MaterialApp(
            debugShowCheckedModeBanner: false,
            theme: ThemeData(
              fontFamily: 'Vazirmatn',
              colorScheme: studioDark,
              scaffoldBackgroundColor: const Color(0xff080a0e),
              useMaterial3: true,
            ),
            home: Scaffold(
              appBar: AppBar(
                title: const Text('PakarController · Simulated table fixture'),
              ),
              body: TablePage(
                status: peerFixture(),
                mapping: CellMap.defaults(),
                command: (path, [values = const {}]) async {},
                verifiedCommand: (path, values) async => false,
              ),
            ),
          ),
        ),
      );
      await tester.enterText(find.byType(TextField).first, '1–3, 12, 80');
      await tester.pumpAndSettle();
      expect(tester.takeException(), isNull);
      final render =
          boundary.currentContext!.findRenderObject() as RenderRepaintBoundary;
      await tester.runAsync(() async {
        final image = await render.toImage(pixelRatio: 1);
        final bytes = await image.toByteData(format: ui.ImageByteFormat.png);
        expect(bytes, isNotNull);
        await output.writeAsBytes(bytes!.buffer.asUint8List(), flush: true);
        image.dispose();
      });
    },
  );
}
