import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/workspace_activity_strip.dart';

const reduced = [
  ('table', 'Table', Icons.table_chart),
  ('output', 'Output', Icons.lightbulb),
  ('settings', 'Settings', Icons.settings),
];

void main() {
  testWidgets(
    'Reduced phone and portrait-tablet footers retain the original NavigationBar',
    (tester) async {
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      for (final width in [360.0, 800.0]) {
        tester.view.physicalSize = Size(width, 1000);
        var selected = 'output';
        await tester.pumpWidget(
          MaterialApp(
            home: Scaffold(
              bottomNavigationBar: StatefulBuilder(
                builder: (context, update) => WorkspaceActivityStrip(
                  pages: reduced,
                  selected: selected,
                  changed: (id) => update(() => selected = id),
                ),
              ),
            ),
          ),
        );
        await tester.pumpAndSettle();
        expect(find.byType(NavigationBar), findsOneWidget);
        expect(find.byType(SingleChildScrollView), findsNothing);
        expect(
          tester
              .widget<NavigationBar>(find.byType(NavigationBar))
              .selectedIndex,
          1,
        );
        await tester.tap(find.text('Settings'));
        await tester.pumpAndSettle();
        expect(selected, 'settings');
        expect(tester.takeException(), isNull);
        await tester.pumpWidget(const SizedBox());
      }
    },
  );

  testWidgets(
    'Long RTL labels and large text fall back instead of squeezing or hiding destinations',
    (tester) async {
      tester.view.physicalSize = const Size(320, 700);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      const pages = [
        ('table', 'انتخاب خانه‌ها', Icons.table_chart),
        ('output', 'کنترل روشنایی', Icons.lightbulb),
        ('settings', 'تنظیمات دستگاه', Icons.settings),
      ];
      await tester.pumpWidget(
        MaterialApp(
          home: MediaQuery(
            data: const MediaQueryData(
              size: Size(320, 700),
              textScaler: TextScaler.linear(2),
            ),
            child: Directionality(
              textDirection: TextDirection.rtl,
              child: Scaffold(
                bottomNavigationBar: WorkspaceActivityStrip(
                  pages: pages,
                  selected: 'settings',
                  changed: (_) {},
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(find.byType(NavigationBar), findsNothing);
      expect(find.byType(SingleChildScrollView), findsOneWidget);
      expect(find.text('تنظیمات دستگاه'), findsOneWidget);
      final selected = tester.getRect(find.text('تنظیمات دستگاه'));
      expect(selected.left, greaterThanOrEqualTo(0));
      expect(selected.right, lessThanOrEqualTo(320));
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'Reducing and restoring activities preserves selection by ID across footer designs',
    (tester) async {
      tester.view.physicalSize = const Size(380, 800);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final expanded = [
        ...reduced,
        for (var i = 0; i < 5; i++) ('extra-$i', 'Extra $i', Icons.settings),
      ];
      var pages = expanded;
      var changes = 0;
      late StateSetter update;
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            bottomNavigationBar: StatefulBuilder(
              builder: (context, setState) {
                update = setState;
                return WorkspaceActivityStrip(
                  pages: pages,
                  selected: 'output',
                  changed: (_) => changes++,
                );
              },
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      expect(find.byType(NavigationBar), findsNothing);
      update(() => pages = reduced);
      await tester.pumpAndSettle();
      expect(
        tester.widget<NavigationBar>(find.byType(NavigationBar)).selectedIndex,
        1,
      );
      update(() => pages = expanded);
      await tester.pumpAndSettle();
      expect(find.byType(NavigationBar), findsNothing);
      expect(
        changes,
        0,
        reason: 'Layout changes are not navigation or device commands',
      );
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets(
    'Narrowing an overflow footer reveals the retained selected activity',
    (tester) async {
      tester.view.physicalSize = const Size(800, 1000);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final pages = [
        for (var i = 0; i < 8; i++) ('$i', 'Panel $i', Icons.settings),
      ];
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            bottomNavigationBar: WorkspaceActivityStrip(
              pages: pages,
              selected: '7',
              changed: (_) {},
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      tester.view.physicalSize = const Size(360, 800);
      await tester.pumpAndSettle();
      final selected = tester.getRect(find.text('Panel 7'));
      expect(selected.left, greaterThanOrEqualTo(0));
      expect(selected.right, lessThanOrEqualTo(360));
      expect(tester.takeException(), isNull);
    },
  );
}
