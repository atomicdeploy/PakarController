import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/command_readback.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/mirroring.dart';
import 'package:pakarcontroller/output_panel.dart';
import 'package:pakarcontroller/table_workspace.dart';
import 'package:pakarcontroller/workspace_widgets.dart';

import 'peer_fixture.dart';
import 'mirroring_test.dart' show mirroredPeer, SilentOutput;

Widget page(Widget child, {double scale = 1}) => MaterialApp(
  theme: ThemeData.dark(useMaterial3: true),
  builder: (context, child) => MediaQuery(
    data: MediaQuery.of(context).copyWith(textScaler: TextScaler.linear(scale)),
    child: child!,
  ),
  home: Scaffold(body: child),
);

void expectCall(
  (String, Map<String, String>) actual,
  String path,
  Map<String, String> values,
) {
  expect(actual.$1, path);
  expect(actual.$2, values);
}

void main() {
  testWidgets(
    'Hardware outputs retain readback and prevent duplicate pending writes',
    (tester) async {
      final status = peerFixture(features: ['inputs', 'outputs']);
      status.json.addAll({
        'active_outputs': 0,
        'active_inputs': 1,
        'input_9': false,
        'programming_latch': false,
      });
      final completion = Completer<bool>();
      final calls = <(String, Map<String, String>)>[];
      Widget controls() => page(
        HardwareIoSection(
          status: status,
          connected: true,
          features: const {'inputs', 'outputs'},
          command: (path, values) {
            calls.add((path, values));
            return completion.future;
          },
        ),
      );
      await tester.pumpWidget(controls());
      final first = find.byKey(const ValueKey('hardware-output-1'));
      await tester.tap(first);
      await tester.pump();
      expect(calls, hasLength(1));
      expectCall(calls.single, '/api/outputs', {'mask': '1'});
      expect(tester.widget<FilterChip>(first).selected, false);
      expect(tester.widget<FilterChip>(first).onSelected, isNull);
      completion.complete(true);
      await tester.pumpAndSettle();
      expect(tester.widget<FilterChip>(first).selected, false);
      status.json['active_outputs'] = 1;
      await tester.pumpWidget(controls());
      expect(tester.widget<FilterChip>(first).selected, true);
      expect(find.text('1 · On'), findsOneWidget);
    },
  );

  testWidgets(
    'Hardware outputs require fresh admitted mask AVR and released latch',
    (tester) async {
      var calls = 0;
      for (final patch in <Map<String, Object?>>[
        {'active_outputs': null},
        {'active_outputs': -1},
        {'active_outputs': 256},
        {'avr_online': false},
        {'programming_latch': true},
        {'programming_latch': null},
      ]) {
        final status = peerFixture(features: ['outputs']);
        status.json.addAll({
          'active_outputs': 0,
          'programming_latch': false,
          ...patch,
        });
        await tester.pumpWidget(
          page(
            HardwareIoSection(
              status: status,
              connected: true,
              features: const {'outputs'},
              command: (_, _) async {
                calls++;
                return true;
              },
            ),
          ),
        );
        for (final chip in tester.widgetList<FilterChip>(
          find.byType(FilterChip),
        )) {
          expect(chip.onSelected, isNull, reason: '$patch');
        }
      }
      for (final status in [
        null,
        peerFixture(features: ['identity']),
      ]) {
        await tester.pumpWidget(
          page(
            HardwareIoSection(
              status: status,
              connected: false,
              features: const {'outputs'},
              command: (_, _) async {
                calls++;
                return true;
              },
            ),
          ),
        );
        expect(
          tester.widget<FilterChip>(find.byType(FilterChip).first).onSelected,
          isNull,
        );
      }
      expect(calls, 0);
    },
  );

  test(
    'Output readback requires exact observed mask and current AVR authority',
    () {
      final status = peerFixture(features: ['outputs']);
      status.json.addAll({'active_outputs': 1, 'programming_latch': false});
      expect(
        commandReadbackMatches(status, '/api/outputs', {'mask': '1'}),
        true,
      );
      expect(
        commandReadbackMatches(status, '/api/outputs', {'mask': '2'}),
        false,
      );
      expect(
        commandReadbackMatches(status, '/api/outputs', {'mask': '-1'}),
        false,
      );
      status.json['programming_latch'] = true;
      expect(
        commandReadbackMatches(status, '/api/outputs', {'mask': '1'}),
        false,
      );
      status.json['programming_latch'] = false;
      status.json['avr_online'] = false;
      expect(
        commandReadbackMatches(status, '/api/outputs', {'mask': '1'}),
        false,
      );
    },
  );

  test('Text release uses text ACK without inventing display availability', () {
    final text = peerFixture(features: ['text'], displayAck: false).peer;
    expect(text.allows('/api/display', {'release': 'true'}), isTrue);
    expect(text.allows('/api/display', {'value': '123'}), isFalse);
    expect(text.allows('/api/display', {'release': 'false'}), isFalse);
    final display = peerFixture(features: ['display']).peer;
    expect(display.allows('/api/display', {'value': '123'}), isTrue);
    expect(display.allows('/api/display', {'release': 'true'}), isFalse);
  });

  testWidgets('Overview cards wrap to columns without command side effects', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(1100, 900);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    String? destination;
    await tester.pumpWidget(
      page(
        BoardOverview(
          status: peerFixture(),
          connected: true,
          navigate: (value) => destination = value,
        ),
      ),
    );
    final cards = find.byType(WorkspaceActionCard);
    expect(
      tester.getTopLeft(cards.at(0)).dy,
      tester.getTopLeft(cards.at(1)).dy,
    );
    expect(
      tester.getTopLeft(cards.at(1)).dx,
      greaterThan(tester.getTopLeft(cards.at(0)).dx),
    );
    await tester.tap(find.text('Front panel'));
    expect(destination, 'front-panel');
    expect(tester.takeException(), isNull);
  });

  testWidgets(
    'Front panel prioritizes board controls and keeps mirror preferences once',
    (tester) async {
      tester.view.physicalSize = const Size(1100, 1800);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final mirror = PeripheralMirror(output: SilentOutput())
        ..observe(mirroredPeer(), 1);
      addTearDown(mirror.dispose);
      await tester.pumpWidget(
        page(
          FrontPanelPage(
            status: mirroredPeer(),
            mirror: mirror,
            command: (_, [values = const {}]) async {
              fail('Layout must not send a board command');
            },
          ),
        ),
      );
      await tester.pumpAndSettle();
      final body = tester.widget<WorkspaceBody>(find.byType(WorkspaceBody));
      expect(body.children[1], isA<WorkspaceGrid>());
      expect(
        (body.children.last as WorkspaceSection).title,
        'Mirror on this device',
      );
      final display = find.byWidgetPredicate(
        (widget) =>
            widget is WorkspaceSection && widget.title == '7-segment display',
      );
      final text = find.byWidgetPredicate(
        (widget) => widget is WorkspaceSection && widget.title == 'Text',
      );
      expect(tester.getTopLeft(display).dy, tester.getTopLeft(text).dy);
      expect(
        tester.getTopLeft(text).dx,
        greaterThan(tester.getTopLeft(display).dx),
      );
      final melody = find.byWidgetPredicate(
        (widget) =>
            widget is WorkspaceSection && widget.title == 'Board melody',
      );
      final live = find.byWidgetPredicate(
        (widget) =>
            widget is WorkspaceSection && widget.title == 'Live front panel',
      );
      expect(tester.getTopLeft(melody).dy, tester.getTopLeft(live).dy);
      expect(
        tester.getTopLeft(live).dx,
        greaterThan(tester.getTopLeft(melody).dx),
      );
      expect(
        find.widgetWithText(SwitchListTile, 'Front panel'),
        findsOneWidget,
      );
      expect(
        find.widgetWithText(SwitchListTile, '7-segment display'),
        findsOneWidget,
      );
      expect(
        find.widgetWithText(SwitchListTile, 'Exact-tone buzzer'),
        findsOneWidget,
      );
      expect(
        find.widgetWithText(SwitchListTile, 'Follow board Silent mode'),
        findsOneWidget,
      );
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
      mirror.dispose(); // Retire the lease before the widget-test invariant.
    },
  );

  testWidgets(
    'Stop text keeps its slot across active inactive and pending states',
    (tester) async {
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      tester.view.devicePixelRatio = 1;
      for (final width in [390.0, 1100.0]) {
        tester.view.physicalSize = Size(width, 1800);
        final status = peerFixture(features: ['text']);
        final completion = Completer<void>();
        var calls = 0;
        Widget controls() => page(
          FrontPanelPage(
            status: status,
            command: (path, [values = const {}]) {
              expect(path, '/api/display');
              expect(values, {'release': 'true'});
              calls++;
              return completion.future;
            },
          ),
        );
        await tester.pumpWidget(controls());
        await tester.pumpAndSettle();
        final stop = find.widgetWithText(TextButton, 'Stop text');
        final present = find.widgetWithText(FilledButton, 'Present');
        final textCard = find.byWidgetPredicate(
          (widget) => widget is WorkspaceSection && widget.title == 'Text',
        );
        final stopRect = tester.getRect(stop);
        final presentRect = tester.getRect(present);
        final cardRect = tester.getRect(textCard);
        for (final active in [null, false, true]) {
          status.json['tm1637_host_active'] = active;
          await tester.pumpWidget(controls());
          await tester.pumpAndSettle();
          expect(tester.getRect(stop), stopRect);
          expect(tester.getRect(present), presentRect);
          expect(tester.getRect(textCard), cardRect);
          expect(
            tester.widget<TextButton>(stop).onPressed != null,
            active == true,
          );
        }
        await tester.tap(stop);
        await tester.pump();
        expect(calls, 1);
        expect(tester.widget<TextButton>(stop).onPressed, isNull);
        expect(tester.getRect(stop), stopRect);
        status.json['tm1637_host_active'] = false;
        await tester.pumpWidget(controls());
        completion.complete();
        await tester.pumpAndSettle();
        expect(tester.widget<TextButton>(stop).onPressed, isNull);
        expect(tester.getRect(stop), stopRect);
        expect(tester.getRect(present), presentRect);
        expect(tester.getRect(textCard), cardRect);
        expect(calls, 1);
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      }
    },
  );

  testWidgets(
    'Front panel exposes native number text release and exact tone APIs',
    (tester) async {
      final status = peerFixture()..json['tm1637_host_active'] = true;
      final calls = <(String, Map<String, String>)>[];
      final workspace = FrontPanelWorkspace();
      await tester.pumpWidget(
        page(
          FrontPanelPage(
            status: status,
            workspace: workspace,
            command: (path, [values = const {}]) async {
              calls.add((path, values));
            },
          ),
        ),
      );
      await tester.enterText(
        find.byKey(const ValueKey('front-panel-number')),
        '123',
      );
      await tester.ensureVisible(find.text('Show number'));
      await tester.tap(find.text('Show number'));
      await tester.pumpAndSettle();
      expectCall(calls.last, '/api/display', {'value': '123'});
      expect(workspace.number, 123);
      await tester.ensureVisible(find.text('Stop text'));
      await tester.tap(find.text('Stop text'));
      await tester.pumpAndSettle();
      expectCall(calls.last, '/api/display', {'release': 'true'});
      await tester.ensureVisible(find.text('Custom tone'));
      await tester.tap(find.text('Custom tone'));
      await tester.pumpAndSettle();
      await tester.ensureVisible(
        find.byKey(const PageStorageKey('front-panel-frequency')),
      );
      await tester.enterText(
        find.byKey(const PageStorageKey('front-panel-frequency')),
        '440',
      );
      await tester.enterText(
        find.byKey(const PageStorageKey('front-panel-duration')),
        '90',
      );
      await tester.ensureVisible(find.text('Play tone'));
      await tester.tap(find.text('Play tone'));
      await tester.pumpAndSettle();
      expectCall(calls.last, '/api/buzzer', {
        'frequency': '440',
        'duration_ms': '90',
      });
      expect(workspace.frequency, 440);
      await tester.pumpWidget(const SizedBox());
      workspace.dispose();
    },
  );

  testWidgets('Invalid and unavailable front panel controls never send', (
    tester,
  ) async {
    var calls = 0;
    await tester.pumpWidget(
      page(
        FrontPanelPage(
          status: peerFixture(),
          command: (_, [values = const {}]) async {
            calls++;
          },
        ),
      ),
    );
    await tester.enterText(
      find.byKey(const ValueKey('front-panel-number')),
      '-',
    );
    await tester.pump();
    await tester.ensureVisible(find.text('Show number'));
    expect(
      tester
          .widget<FilledButton>(
            find.widgetWithText(FilledButton, 'Show number'),
          )
          .onPressed,
      isNull,
    );
    await tester.scrollUntilVisible(
      find.text('Custom tone'),
      300,
      scrollable: find.byType(Scrollable).first,
    );
    await tester.ensureVisible(find.text('Custom tone'));
    await tester.pumpAndSettle();
    expect(find.text('Custom tone').hitTestable(), findsOneWidget);
    await tester.tap(find.text('Custom tone'));
    await tester.pumpAndSettle();
    await tester.ensureVisible(
      find.byKey(const PageStorageKey('front-panel-frequency')),
    );
    await tester.enterText(
      find.byKey(const PageStorageKey('front-panel-frequency')),
      '99999',
    );
    await tester.pump();
    expect(
      tester
          .widget<FilledButton>(find.widgetWithText(FilledButton, 'Play tone'))
          .onPressed,
      isNull,
    );
    await tester.pumpWidget(
      page(
        FrontPanelPage(
          status: null,
          connected: false,
          seenFeatures: const {'buzzer', 'text', 'display'},
          command: (_, [values = const {}]) async {
            calls++;
          },
        ),
      ),
    );
    for (final button in tester.widgetList<FilledButton>(
      find.byType(FilledButton),
    )) {
      expect(button.onPressed, isNull);
    }
    expect(calls, 0);
  });

  test('Front panel bounds and durable drafts match native display and tone controls', () {
    final workspace = FrontPanelWorkspace(
      saved: {
        'number': 456,
        'hold_ms': 2300,
        'interval_seconds': 23,
        'frequency': 440,
        'duration_ms': 90,
        'speed': 5000,
      },
    );
    final restored = FrontPanelWorkspace(saved: workspace.snapshot());
    expect(restored.number, 456);
    expect(restored.holdMs, 2300);
    expect(restored.intervalSeconds, 23);
    expect(restored.frequency, 440);
    expect(restored.durationMs, 90);
    expect(restored.speed, 5000);
    restored.frequency = 0;
    restored.durationMs = 5001;
    restored.holdMs = -1;
    expect(restored.frequency, 440);
    expect(restored.durationMs, 90);
    expect(restored.holdMs, 2300);
    workspace.dispose();
    restored.dispose();
  });

  testWidgets(
    'Output brightness sends once after drag and effects use shared APIs',
    (tester) async {
      final status = peerFixture();
      status.json['led_output'] = {'enabled': true, 'mode': 'effects'};
      status.json['brightness'] = 200;
      final calls = <(String, Map<String, String>)>[];
      await tester.pumpWidget(
        page(
          OutputPanel(
            status: status,
            connected: true,
            openDiagnostics: () {},
            command: (path, [values = const {}]) async {
              calls.add((path, values));
            },
          ),
        ),
      );
      await tester.ensureVisible(find.text('Fire'));
      await tester.tap(find.text('Fire'));
      await tester.pumpAndSettle();
      expectCall(calls.last, '/api/effect', {'value': 'fire'});
      await tester.tap(find.text('State colors'));
      await tester.pumpAndSettle();
      expectCall(calls.last, '/api/effect', {'value': 'none'});
      await tester.ensureVisible(
        find.byKey(const ValueKey('output-brightness')),
      );
      final slider = tester.widget<Slider>(
        find.byKey(const ValueKey('output-brightness')),
      );
      slider.onChanged!(140);
      slider.onChanged!(120);
      expect(calls.length, 2);
      slider.onChangeEnd!(120);
      await tester.pumpAndSettle();
      expectCall(calls.last, '/api/brightness', {'value': '120'});
      expect(
        tester
            .widget<Slider>(find.byKey(const ValueKey('output-brightness')))
            .value,
        200,
        reason: 'Command completion does not fabricate board readback',
      );
    },
  );

  testWidgets(
    'Panel cards fit narrow wide RTL and enlarged text without clipping',
    (tester) async {
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      for (final width in [320.0, 1100.0]) {
        tester.view.physicalSize = Size(width, 1200);
        tester.view.devicePixelRatio = 1;
        for (final scale in [1.0, 2.0]) {
          for (final rtl in [false, true]) {
            for (final child in <Widget>[
              BoardOverview(
                status: peerFixture(),
                connected: true,
                navigate: (_) {},
              ),
              HardwarePage(status: peerFixture(), navigate: (_) {}),
              FrontPanelPage(
                status: peerFixture(),
                command: (_, [values = const {}]) async {},
              ),
            ]) {
              await tester.pumpWidget(
                page(
                  Directionality(
                    textDirection: rtl ? TextDirection.rtl : TextDirection.ltr,
                    child: child,
                  ),
                  scale: scale,
                ),
              );
              await tester.pumpAndSettle();
              expect(
                tester.takeException(),
                isNull,
                reason:
                    '${child.runtimeType} width=$width scale=$scale rtl=$rtl',
              );
              await tester.pumpWidget(const SizedBox());
            }
          }
        }
      }
    },
  );
}
