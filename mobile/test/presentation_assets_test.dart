import 'dart:convert';

import 'package:crypto/crypto.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/presentation_assets.dart';

void main() {
  const text = '{"revision":1,"palette":{"primary":"00ff00"}}';
  final hash = sha256.convert(utf8.encode(text)).toString();
  test(
    'Data palette activates atomically and persists exact verified bytes',
    () async {
      String? saved;
      final assets = PresentationAssets(save: (value) async => saved = value);
      await assets.applyText(text, hash, () => true);
      expect(assets.value['primary'], 0x00ff00);
      expect(assets.revision, 1);
      expect(jsonDecode(saved!)['bundle'], text);
      await assets.applyText(text, hash, () => true); // Same digest: no replay.
      expect(() => assets.value['primary'] = 0, throwsUnsupportedError);
      assets.dispose();
    },
  );
  test(
    'Wrong digest, scripts, invalid roles and rollback are rejected',
    () async {
      final assets = PresentationAssets(save: (_) async {});
      await expectLater(
        assets.applyText(text, '0' * 64, () => true),
        throwsFormatException,
      );
      for (final text in [
        '{"revision":1,"palette":{"script":"abcdef"}}',
        '{"revision":1,"palette":{"primary":"ffffff"},"code":"x"}',
        '{"revision":0,"palette":{"primary":"ffffff"}}',
      ]) {
        await expectLater(
          assets.applyText(
            text,
            sha256.convert(utf8.encode(text)).toString(),
            () => true,
          ),
          throwsFormatException,
        );
      }
      expect(assets.revision, 0);
      assets.dispose();
    },
  );
  test(
    'Retirement during storage rolls back without changing live theme',
    () async {
      var authorized = true;
      final saves = <String>[];
      final assets = PresentationAssets(
        save: (value) async {
          saves.add(value);
          authorized = false;
        },
      );
      await expectLater(
        assets.applyText(text, hash, () => authorized),
        throwsStateError,
      );
      expect(assets.value, isEmpty);
      expect(assets.revision, 0);
      expect(saves.last, '');
      assets.dispose();
    },
  );
}
