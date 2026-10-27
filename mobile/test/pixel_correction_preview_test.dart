import 'dart:async';
import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/pixel_correction_preview.dart';

import 'pixel_corrections_editor_test.dart' show EditorApi, rule;

class RenewalApi extends EditorApi {
  bool uncertain = false, foreignReceipt = false;
  @override
  Future<Map<String, dynamic>> postJson(
    String path,
    Map<String, String> values, {
    bool Function()? authorize,
  }) async {
    final config = jsonDecode(values['configuration']!) as Map;
    final reply = await super.postJson(path, values, authorize: authorize);
    if (config['clear'] != true && config['lease_id'] != null) {
      if (uncertain) throw TimeoutException('fixture response lost');
      if (foreignReceipt) {
        return {
          ...reply,
          'preview': {...reply['preview'] as Map, 'lease_id': 9876},
        };
      }
    }
    return reply;
  }
}

void main() {
  testWidgets(
    'opt-in preview renews continuously without saving or reacquiring',
    (tester) async {
      final api = RenewalApi();
      addTearDown(api.close);
      final preview = PixelCorrectionPreview(
        api: api,
        allowed: () => true,
        configuration: () => {
          'revision': 3,
          'enabled': true,
          'rules': [rule('black', 2)],
        },
        pixels: () => 480,
      );
      await preview.start();
      final lease = preview.lease;
      for (var i = 0; i < 6; i++) {
        await tester.pump(const Duration(milliseconds: 5100));
        await tester.pump();
        expect(preview.enabled, true);
        expect(preview.lease, lease);
        expect(preview.error, isNull);
      }
      expect(api.requests.length, 7);
      expect(api.requests.skip(1).every((r) => r['lease_id'] == lease), true);
      expect(api.saves, 0);
      await preview.stop();
      final count = api.requests.length;
      await tester.pump(const Duration(seconds: 30));
      expect(api.requests.length, count);
      expect(api.activePreview, isNull);
      preview.dispose();
    },
  );

  for (final loss in [
    'foreground',
    'boot',
    'revision',
    'uncertain',
    'foreign',
  ]) {
    testWidgets('renewal stops safely on $loss without mutation replay', (
      tester,
    ) async {
      final api = RenewalApi();
      addTearDown(api.close);
      var allowed = true, revision = 3;
      final preview = PixelCorrectionPreview(
        api: api,
        allowed: () => allowed,
        configuration: () => {
          'revision': revision,
          'enabled': true,
          'rules': [rule('black', 2)],
        },
        pixels: () => 480,
      );
      await preview.start();
      if (loss == 'foreground') allowed = false;
      if (loss == 'boot') api.boot = 'b' * 32;
      if (loss == 'revision') revision = 4;
      if (loss == 'uncertain') api.uncertain = true;
      if (loss == 'foreign') api.foreignReceipt = true;
      await tester.pump(const Duration(milliseconds: 5100));
      await tester.pump();
      expect(preview.enabled, false);
      expect(preview.lease, isNull);
      final count = api.requests.length;
      await tester.pump(const Duration(seconds: 30));
      expect(api.requests.length, count);
      expect(api.saves, 0);
      expect(api.requests.any((r) => r['lease_id'] == 9876), false);
      if (loss == 'boot') expect(api.requests.length, 1);
      if (loss == 'uncertain' || loss == 'foreign') {
        expect(api.requests.where((r) => r['clear'] != true).length, 2);
      }
      preview.dispose();
    });
  }

  testWidgets(
    'in-flight renewal expires safely and late reply cannot restart',
    (tester) async {
      final api = RenewalApi();
      addTearDown(api.close);
      final preview = PixelCorrectionPreview(
        api: api,
        allowed: () => true,
        configuration: () => {
          'revision': 3,
          'enabled': true,
          'rules': [rule('black', 2)],
        },
        pixels: () => 480,
      );
      await preview.start();
      api.hold = Completer<void>();
      await tester.pump(const Duration(milliseconds: 5100));
      expect(preview.pending, true);
      await tester.pump(const Duration(seconds: 5));
      expect(preview.enabled, false);
      final pending = api.hold!;
      api.hold = null;
      pending.complete();
      await tester.pump();
      expect(preview.enabled, false);
      expect(preview.pending, false);
      expect(api.activePreview, isNull);
      final count = api.requests.length;
      await tester.pump(const Duration(seconds: 30));
      expect(api.requests.length, count);
      preview.dispose();
    },
  );
}
