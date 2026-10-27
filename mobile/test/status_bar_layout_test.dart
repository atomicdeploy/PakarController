import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

Future<void> pumpFooter(
  WidgetTester tester, {
  required double width,
  double scale = 1,
  double bottomInset = 0,
  TextDirection direction = TextDirection.ltr,
  bool failed = false,
  String message = 'Connected',
  DeviceStatus? status,
  VoidCallback? onConnection,
}) async {
  tester.view.physicalSize = Size(width, 800);
  tester.view.devicePixelRatio = 1;
  await tester.pumpWidget(
    MaterialApp(
      theme: ThemeData.dark(),
      builder: (context, child) => MediaQuery(
        data: MediaQuery.of(context).copyWith(
          textScaler: TextScaler.linear(scale),
          padding: EdgeInsets.only(bottom: bottomInset),
        ),
        child: Directionality(textDirection: direction, child: child!),
      ),
      home: Scaffold(
        body: Column(
          children: [
            const Expanded(child: SizedBox()),
            StatusBar(
              status: status ?? peerFixture(),
              message: message,
              commandFailed: failed,
              onConnection: onConnection,
            ),
          ],
        ),
      ),
    ),
  );
  await tester.pump();
}

void main() {
  testWidgets(
    'Phone footer truncates long feedback without losing its full accessible details',
    (tester) async {
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      const message =
          'The device rejected the command because its configuration changed while the previous preview was active.';
      for (final width in [320.0, 390.0]) {
        await pumpFooter(
          tester,
          width: width,
          failed: true,
          message: message,
          status: peerFixture(hostname: 'ledcontroller-with-a-long-hostname'),
          onConnection: () {},
        );
        final text = find.descendant(
          of: find.byKey(const Key('connection-status-area')),
          matching: find.text(message),
        );
        expect(tester.widget<Text>(text).maxLines, 1);
        expect(tester.widget<Text>(text).overflow, TextOverflow.ellipsis);
        expect(
          tester.renderObject<RenderParagraph>(text).didExceedMaxLines,
          true,
        );
        expect(find.byTooltip(message), findsOneWidget);
        expect(tester.getSize(find.byType(StatusBar)).height, 48);
        expect(
          tester.getRect(find.byKey(const Key('command-status-details'))).right,
          lessThanOrEqualTo(width),
        );
        expect(tester.takeException(), isNull);
        await tester.tap(find.byKey(const Key('command-status-details')));
        await tester.pumpAndSettle();
        expect(find.widgetWithText(SelectableText, message), findsOneWidget);
        await tester.tap(find.text('Close'));
        await tester.pumpAndSettle();
      }
    },
  );
  testWidgets(
    'Entire connection status icon and text open connection, errors stay inspectable',
    (tester) async {
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      var opened = 0;
      await pumpFooter(
        tester,
        width: 800,
        message: 'Live device status received',
        onConnection: () => opened++,
      );
      expect(find.text('Connected'), findsOneWidget);
      await tester.tap(find.byIcon(Icons.check_circle_outline));
      await tester.tap(find.text('Connected'));
      expect(opened, 2);
      await pumpFooter(
        tester,
        width: 800,
        failed: true,
        message: 'Rejected',
        onConnection: () => opened++,
      );
      await tester.tap(find.byKey(const Key('connection-status-area')));
      expect(opened, 3);
      await tester.tap(find.byKey(const Key('command-status-details')));
      await tester.pumpAndSettle();
      expect(find.widgetWithText(SelectableText, 'Rejected'), findsOneWidget);
    },
  );
  testWidgets('Tablet footer is one 48px row without duplicate safe padding', (
    tester,
  ) async {
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    for (final width in [800.0, 1200.0, 1600.0]) {
      for (final inset in [0.0, 24.0]) {
        await pumpFooter(tester, width: width, bottomInset: inset);
        expect(tester.getSize(find.byType(StatusBar)).height, 48);
        expect(
          tester.getRect(find.byTooltip('Connection settings')).right,
          closeTo(width - 12, .1),
        );
        final details = tester.widget<Text>(
          find.descendant(
            of: find.byKey(const Key('status-peer-details')),
            matching: find.byType(Text),
          ),
        );
        expect(details.textAlign, TextAlign.end);
        expect(
          tester.getSize(find.byTooltip('Connection settings')),
          const Size(48, 48),
        );
        expect(tester.takeException(), isNull);
      }
    }
  });

  testWidgets(
    'Long live details do not grow the tablet footer or lose controls',
    (tester) async {
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      var opened = false;
      final status = peerFixture(hostname: 'ledcontroller-long-hostname');
      status.json.addAll({
        'station_connected': true,
        'station_ip': '192.168.100.123',
        'rssi': -57,
        'ethernet': {'ipv4_ready': true, 'ip': '192.168.100.124'},
      });
      await pumpFooter(
        tester,
        width: 800,
        status: status,
        message:
            'Command accepted and device state confirmed for selected cells',
        onConnection: () => opened = true,
      );
      expect(tester.getSize(find.byType(StatusBar)).height, 48);
      expect(
        find.byWidgetPredicate(
          (widget) =>
              widget is Tooltip &&
              widget.message?.contains('ledcontroller-long-hostname') == true,
        ),
        findsOneWidget,
      );
      await tester.tap(find.byTooltip('Connection settings'));
      expect(opened, isTrue);
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets('Compact status still opens complete error details', (
    tester,
  ) async {
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    const message = 'The device rejected this command: permission is required.';
    await pumpFooter(tester, width: 800, failed: true, message: message);
    expect(tester.getSize(find.byType(StatusBar)).height, 48);
    expect(
      tester.getSize(find.byKey(const Key('command-status-details'))).height,
      greaterThanOrEqualTo(48),
    );
    await tester.tap(find.byKey(const Key('command-status-details')));
    await tester.pumpAndSettle();
    expect(
      find.descendant(
        of: find.byType(AlertDialog),
        matching: find.text('Command not completed'),
      ),
      findsOneWidget,
    );
    expect(find.widgetWithText(SelectableText, message), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  testWidgets(
    'Phone and tablet status retain touch targets at large text and RTL',
    (tester) async {
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      for (final width in [320.0, 800.0, 1200.0]) {
        for (final scale in [1.0, 1.8, 3.0]) {
          for (final direction in TextDirection.values) {
            await pumpFooter(
              tester,
              width: width,
              scale: scale,
              direction: direction,
              message: 'Command accepted and device state confirmed',
              onConnection: () {},
            );
            expect(
              tester.getSize(find.byType(StatusBar)).height,
              greaterThanOrEqualTo(48),
            );
            expect(
              tester.getSize(find.byType(StatusBar)).height,
              lessThanOrEqualTo(80),
            );
            expect(
              tester.getSize(find.byTooltip('Connection settings')),
              const Size(48, 48),
            );
            expect(tester.takeException(), isNull);
          }
        }
      }
    },
  );
}
