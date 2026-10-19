import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/app_theme.dart';
import 'package:pakarcontroller/app_update_coordinator.dart';
import 'package:pakarcontroller/app_updates_page.dart';
import 'package:pakarcontroller/firmware_automation.dart';
import 'package:pakarcontroller/firmware_automation_page.dart';
import 'package:pakarcontroller/firmware_feed.dart';

import 'app_update_coordinator_test.dart'
    show PackageUpdates, PackageDiscovery, package;
import 'firmware_page_test.dart' show FirmwareApi;

AppUpdateCoordinator updatePageFixture() => AppUpdateCoordinator(
  updates: PackageUpdates(),
  discovery: PackageDiscovery(),
  saveSnapshot: (_) async {},
  approve: (_) async => false,
)..package = {...package(version: 8), 'version_name': '0.2.8'};

FirmwareAutomation automationPageFixture(FirmwareApi api) => FirmwareAutomation(
  api,
  authorized: (_) => false,
  save: (_) async {},
  initial: {
    'policy': const FirmwareAutomationPolicy(
      boardId: 'ledboard-fixture',
      applicationId: 'pakar.table',
    ).toJson(),
    'consumed': <String>[],
  },
);

class ReviewAutomation extends FirmwareAutomation {
  ReviewAutomation(super.api)
    : super(authorized: (_) => true, save: (_) async {});
  final applications = <bool>[];
  @override
  Future<void> reviewPending({bool apply = false}) async =>
      applications.add(apply);
}

void main() {
  testWidgets(
    'Raw ESP offer exposes immediate upload with confirmation, never a misleading Stage action',
    (tester) async {
      final api = FirmwareApi();
      final model = ReviewAutomation(api);
      addTearDown(api.close);
      addTearDown(model.dispose);
      model.pending = FirmwareOffer({
        'target': 'esp',
        'kind': 'binary',
        'sha256': 'a' * 64,
      });
      await tester.pumpWidget(
        MaterialApp(
          theme: studioTheme(),
          home: Scaffold(
            body: FirmwareAutomationPage(automation: model, api: api),
          ),
        ),
      );
      await tester.scrollUntilVisible(
        find.text('Upload ESP image'),
        400,
        scrollable: find.byType(Scrollable).first,
      );
      await tester.pumpAndSettle();
      await tester.ensureVisible(
        find.widgetWithText(OutlinedButton, 'Upload ESP image'),
      );
      await tester.pumpAndSettle();
      expect(find.text('Stage offer'), findsNothing);
      await tester.tap(find.text('Upload ESP image'));
      await tester.pumpAndSettle();
      expect(find.text('Upload this ESP image?'), findsOneWidget);
      await tester.tap(find.text('Cancel'));
      await tester.pumpAndSettle();
      expect(model.applications, isEmpty);
      await tester.tap(find.text('Upload ESP image'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Confirm'));
      await tester.pumpAndSettle();
      expect(model.applications, [true]);
      expect(api.mutations, 0);
      expect(tester.takeException(), isNull);
    },
  );

  for (final width in [390.0, 1200.0]) {
    testWidgets(
      'App updates are usable offline at width $width without writes',
      (tester) async {
        final model = updatePageFixture();
        addTearDown(model.dispose);
        tester.view.physicalSize = Size(width, 1000);
        tester.view.devicePixelRatio = 1;
        addTearDown(tester.view.resetPhysicalSize);
        addTearDown(tester.view.resetDevicePixelRatio);
        await tester.pumpWidget(
          MaterialApp(
            theme: studioTheme(),
            home: Scaffold(body: AppUpdatesPage(coordinator: model)),
          ),
        );
        await tester.pumpAndSettle();
        expect(find.text('App updates'), findsOneWidget);
        expect(find.text('Import APK'), findsOneWidget);
        final preferences = find.text('Save preferences');
        await tester.scrollUntilVisible(
          preferences,
          400,
          scrollable: find.byType(Scrollable).first,
        );
        await tester.pumpAndSettle();
        expect(tester.takeException(), isNull);
        expect((model.updates as PackageUpdates).native, isEmpty);
        expect(model.sharing, false);
        expect(model.settings['auto_download'], false);
      },
    );

    testWidgets('Board automation is separate and opt-in at width $width', (
      tester,
    ) async {
      final api = FirmwareApi();
      final model = automationPageFixture(api);
      addTearDown(api.close);
      addTearDown(model.dispose);
      tester.view.physicalSize = Size(width, 1000);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      await tester.pumpWidget(
        MaterialApp(
          theme: studioTheme(),
          home: Scaffold(
            body: FirmwareAutomationPage(automation: model, api: api),
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(find.text('Automatic firmware updates'), findsOneWidget);
      expect(find.text('ledboard-fixture'), findsOneWidget);
      await tester.scrollUntilVisible(
        find.text('Update activity'),
        400,
        scrollable: find.byType(Scrollable).first,
      );
      await tester.pumpAndSettle();
      expect(tester.takeException(), isNull);
      expect(model.policy.enabled, false);
      expect(model.policy.autoApply, false);
      expect(api.mutations, 0);
    });
  }

  testWidgets('Permission-required package keeps install and discard actions', (
    tester,
  ) async {
    final model = updatePageFixture();
    addTearDown(model.dispose);
    model.updates.state = {
      'state': 'permission_required',
      'transfer_id': 'same-verified-package',
      'install_permission': false,
    };
    await tester.pumpWidget(
      MaterialApp(
        theme: studioTheme(),
        home: Scaffold(body: AppUpdatesPage(coordinator: model)),
      ),
    );
    await tester.pumpAndSettle();
    expect(find.text('Install update'), findsOneWidget);
    expect(find.text('Discard package'), findsOneWidget);
    expect(find.text('Allow app installs'), findsOneWidget);
    expect((model.updates as PackageUpdates).native, isEmpty);
    expect(tester.takeException(), isNull);
  });
}
