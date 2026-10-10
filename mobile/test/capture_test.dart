import 'dart:io';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';
import 'package:pakarcontroller/device_settings_page.dart';

import 'peer_fixture.dart';
import 'device_settings_page_test.dart' show SettingsApi;

void main() {
  final path = Platform.environment['PAKAR_MOBILE_CAPTURE_PATH'];
  final view = Platform.environment['PAKAR_MOBILE_CAPTURE_VIEW'] ?? 'Table';
  testWidgets(
    'Explicit simulated renderer capture, no production IPC or device writes',
    (tester) async {
      if (path == null) return;
      expect(['Table', 'SettingsTablet', 'SettingsPhone'], contains(view));
      final settings = view != 'Table', phone = view == 'SettingsPhone';
      final api = SettingsApi();
      addTearDown(api.close);
      final output = File(path);
      expect(output.isAbsolute, true);
      expect(
        output.existsSync(),
        false,
        reason: 'Never overwrite capture evidence',
      );
      for (final font in ['Vazirmatn', 'Phosphor', 'MaterialIcons']) {
        final loader = FontLoader(font)
          ..addFont(
            rootBundle.load(
              font == 'MaterialIcons'
                  ? 'fonts/MaterialIcons-Regular.otf'
                  : font == 'Vazirmatn'
                  ? 'assets/fonts/Vazirmatn-Regular.ttf'
                  : 'assets/fonts/Phosphor.ttf',
            ),
          );
        await loader.load();
      }
      tester.view.physicalSize = settings
          ? (phone ? const Size(390, 1500) : const Size(1200, 1100))
          : const Size(1100, 1500);
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
                title: const Text('PakarController · Simulated fixture'),
              ),
              body: settings
                  ? DeviceSettingsPage(
                      api: api,
                      status: peerFixture(),
                      authorized: peerFixture().peer.has,
                      navigate: (_) {},
                      adoptConnection: (_, _, _) async {},
                    )
                  : TablePage(
                      status: peerFixture(),
                      mapping: CellMap.defaults(),
                      command: (path, [values = const {}]) async {},
                      verifiedCommand: (path, values) async => false,
                    ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      if (settings) {
        await tester.tap(
          phone
              ? find.widgetWithText(ChoiceChip, 'MQTT')
              : find.widgetWithText(ListTile, 'MQTT'),
        );
      } else {
        await tester.enterText(find.byType(TextField).first, '1–3, 12, 80');
      }
      await tester.pumpAndSettle();
      expect(tester.takeException(), isNull);
      expect(
        api.writes,
        0,
        reason: 'Capture must never issue a device mutation',
      );
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
