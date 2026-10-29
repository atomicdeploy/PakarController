import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/firmware_progress.dart';

Future<void> showProgress(
  WidgetTester tester,
  FirmwareProgress progress, {
  double width = 700,
  double scale = 1,
  bool dark = true,
  bool rtl = false,
}) async {
  tester.view.physicalSize = Size(width, 1200);
  tester.view.devicePixelRatio = 1;
  await tester.pumpWidget(
    MaterialApp(
      theme: dark ? studioTheme() : ThemeData.light(useMaterial3: true),
      home: MediaQuery(
        data: MediaQueryData(textScaler: TextScaler.linear(scale)),
        child: Directionality(
          textDirection: rtl ? TextDirection.rtl : TextDirection.ltr,
          child: Scaffold(
            body: SingleChildScrollView(
              padding: const EdgeInsets.all(12),
              child: FirmwareProgressView(progress),
            ),
          ),
        ),
      ),
    ),
  );
  await tester.pump();
}

void main() {
  testWidgets(
    'Firmware stage uses semantic icon, exact bytes and measured percentage',
    (tester) async {
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      await showProgress(
        tester,
        const FirmwareProgress('AVR · Urboot #7', 'program_avr', 512, 2048),
      );
      expect(find.byIcon(Icons.system_update_alt_rounded), findsOneWidget);
      expect(find.text('Writing firmware'), findsOneWidget);
      expect(find.text('25%'), findsOneWidget);
      expect(find.text('512 / 2048 bytes · this stage'), findsOneWidget);
      expect(
        tester
            .widget<LinearProgressIndicator>(
              find.byType(LinearProgressIndicator),
            )
            .value,
        .25,
      );
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'Unknown verification stays indeterminate without invented completion or ETA',
    (tester) async {
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      await showProgress(
        tester,
        const FirmwareProgress('Board OTA', 'verifying', 100, null),
      );
      expect(find.text('Board OTA · verifying'), findsOneWidget);
      expect(find.text('Verifying firmware'), findsOneWidget);
      expect(
        tester
            .widget<LinearProgressIndicator>(
              find.byType(LinearProgressIndicator),
            )
            .value,
        isNull,
      );
      expect(find.textContaining('%'), findsNothing);
      expect(find.text('Firmware ready'), findsNothing);
      expect(find.textContaining('ETA'), findsNothing);
    },
  );

  testWidgets('Transferred 100 percent is not a verified board update', (
    tester,
  ) async {
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    await showProgress(
      tester,
      const FirmwareProgress('Application', 'uploading to board', 100, 100),
    );
    expect(find.text('Sending firmware'), findsOneWidget);
    expect(find.text('Writing firmware'), findsNothing);
    expect(find.text('100%'), findsOneWidget);
    expect(
      find.text('Transfer finished; board verification pending.'),
      findsOneWidget,
    );
    expect(find.text('Update verified'), findsNothing);
    await showProgress(
      tester,
      const FirmwareProgress('Board OTA', 'complete', 0, null, active: false),
    );
    expect(find.text('Awaiting confirmation'), findsOneWidget);
    expect(find.text('Update verified'), findsNothing);
    await showProgress(
      tester,
      const FirmwareProgress(
        'Board OTA',
        'complete',
        0,
        null,
        active: false,
        verified: true,
      ),
    );
    expect(find.text('Update verified'), findsOneWidget);
    expect(find.text('Firmware ready'), findsOneWidget);
    expect(find.byType(LinearProgressIndicator), findsNothing);
  });

  testWidgets('Failure preserves actual error and stops its busy indicator', (
    tester,
  ) async {
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    await showProgress(
      tester,
      const FirmwareProgress(
        'AVR update',
        'failed',
        0,
        null,
        active: false,
        error: 'Flash readback mismatch',
      ),
    );
    expect(find.text('Flash readback mismatch'), findsOneWidget);
    expect(find.byIcon(Icons.error_outline_rounded), findsNWidgets(2));
    expect(find.text('Update stopped'), findsOneWidget);
    expect(find.byType(LinearProgressIndicator), findsNothing);
  });

  testWidgets(
    'Progress adapts to phone, tablet, RTL and large text in both themes',
    (tester) async {
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      for (final dark in [true, false]) {
        for (final rtl in [false, true]) {
          for (final width in [280.0, 900.0]) {
            await showProgress(
              tester,
              const FirmwareProgress(
                'AVR · Urboot #1234',
                'verify_eeprom',
                1024,
                2048,
              ),
              width: width,
              scale: 2,
              dark: dark,
              rtl: rtl,
            );
            expect(
              tester.takeException(),
              isNull,
              reason: '$width dark=$dark rtl=$rtl',
            );
            expect(find.text('50%'), findsOneWidget);
            if (width == 280) {
              for (final progress in const [
                FirmwareProgress(
                  'Board OTA #23',
                  'awaiting_application',
                  0,
                  null,
                ),
                FirmwareProgress(
                  'Board OTA #23',
                  'complete',
                  0,
                  null,
                  active: false,
                ),
                FirmwareProgress(
                  'AVR · Urboot #1234',
                  'failed',
                  0,
                  null,
                  active: false,
                  error: 'Flash readback mismatch: connection was interrupted.',
                ),
              ]) {
                await showProgress(
                  tester,
                  progress,
                  width: width,
                  scale: 2,
                  dark: dark,
                  rtl: rtl,
                );
                expect(
                  tester.takeException(),
                  isNull,
                  reason: '${progress.phase} $width dark=$dark rtl=$rtl',
                );
              }
            }
          }
        }
      }
      await tester.pumpWidget(const SizedBox());
    },
  );
}
