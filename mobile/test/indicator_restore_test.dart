import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/main.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'connection_lifecycle_test.dart' show LifecycleApi;
import 'workspace_lifecycle_test.dart' show peer;

class ObservationApi extends LifecycleApi {
  bool pixelsRequested = false;

  @override
  void setUsbObservations({
    bool panel = false,
    bool peripheral = false,
    bool pixels = false,
  }) {
    pixelsRequested = pixels;
    super.setUsbObservations(
      panel: panel,
      peripheral: peripheral,
      pixels: pixels,
    );
  }
}

void main() {
  for (final method in ['network', 'usb']) {
    for (final enabled in [true, false]) {
      testWidgets(
        'Cold $method restore applies indicators=$enabled before any UI edit',
        (tester) async {
          SharedPreferences.setMockInitialValues({
            'table_workspaces': jsonEncode({
              'activity': 'table',
              'connection': {'method': method, 'wanted': true},
              'boards': {
                'board-A': {
                  'indicators': enabled,
                  'selection': [7, 64],
                },
                'board-B': {'indicators': !enabled, 'selection': []},
              },
            }),
          });
          FlutterSecureStorage.setMockInitialValues({});
          final api = ObservationApi()
            ..healthyWs = true
            ..observed = peer('board-A');
          await tester.pumpWidget(PakarControllerApp(api: api));
          await tester.pumpAndSettle();
          var workspace = tester
              .widget<TablePage>(find.byType(TablePage))
              .workspace!;
          expect(workspace.showIndicators, enabled);
          expect(workspace.selection, {7, 64});
          expect(api.pixelsRequested, enabled);

          // Fresh status and a new boot must not need a checkbox toggle.
          api.updates.add(peer('board-A', boot: 'reboot'));
          await tester.pumpAndSettle();
          expect(api.pixelsRequested, enabled);
          await tester.pump(const Duration(seconds: 1));
          final prefs = await SharedPreferences.getInstance();
          final saved = prefs.getString('table_workspaces');
          for (var frame = 0; frame < 3; frame++) {
            api.updates.add(peer('board-A', boot: 'reboot'));
            await tester.pump(const Duration(milliseconds: 500));
          }
          expect(prefs.getString('table_workspaces'), saved);

          // Different boards retain their own opposite display preference.
          api.updates.add(peer('board-B'));
          await tester.pumpAndSettle();
          workspace = tester
              .widget<TablePage>(find.byType(TablePage))
              .workspace!;
          expect(workspace.showIndicators, !enabled);
          expect(api.pixelsRequested, !enabled);
          expect(api.writes, 0);
          await tester.pumpWidget(const SizedBox());
          await tester.pump();
        },
      );
    }
  }
}
