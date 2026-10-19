import 'package:flutter/services.dart';

import 'firmware_image.dart';

/// Actual Pealayer preferenceFileAllowed rule, adapted without path guessing.
bool preferenceFileAllowed(String name, Iterable<String> extensions) =>
    extensions.any(
      (extension) => name.toLowerCase().endsWith('.${extension.toLowerCase()}'),
    );

class FirmwareDocument {
  const FirmwareDocument(this.name, this.bytes);
  final String name;
  final Uint8List bytes;
}

/// Pealayer picker selection/cancellation contract delegated to Android SAF.
/// Only a user-created document token can be written, never an arbitrary URI.
class AndroidDocuments {
  static const channel = MethodChannel('ir.digitalogic.pakar/documents');
  Future<FirmwareDocument?> pick({
    int maximum = FirmwareImagePolicy.maximumBytes,
  }) => pickDocument(maximum: maximum);

  Future<FirmwareDocument?> pickDocument({
    int maximum = FirmwareImagePolicy.maximumBytes,
    List<String> extensions = const ['bin'],
  }) async {
    if (maximum < 1 || maximum > FirmwareImagePolicy.maximumBytes) {
      throw const FormatException('Invalid firmware document size bound');
    }
    final value = await channel.invokeMapMethod<String, dynamic>('pick', {
      'maximum': maximum,
    });
    if (value == null) return null;
    final name = value['name'], bytes = value['bytes'];
    if (name is! String ||
        name.isEmpty ||
        name.length > 255 ||
        RegExp(r'[\x00-\x1f\x7f/\\]').hasMatch(name) ||
        !preferenceFileAllowed(name, extensions) ||
        bytes is! Uint8List ||
        bytes.length > maximum) {
      throw const FormatException(
        'Select a bounded document with one of the allowed file extensions',
      );
    }
    return FirmwareDocument(name, bytes);
  }

  Future<bool> export(
    String name,
    Uint8List bytes, {
    required bool Function() stillOwned,
  }) async {
    if (bytes.isEmpty || bytes.length > FirmwareImagePolicy.maximumBytes) {
      throw const FormatException('Invalid firmware export size');
    }
    if (!stillOwned()) {
      throw const FormatException('Board changed before export');
    }
    final token = await channel.invokeMethod<String>('create', {'name': name});
    if (token == null) return false;
    if (!stillOwned()) {
      await channel.invokeMethod<void>('abandon', {'token': token});
      throw const FormatException(
        'Board changed while choosing the destination; a new empty document may remain, but no backup was written',
      );
    }
    // Native consumes the one-use token even on failure. Never replay a write.
    await channel.invokeMethod<void>('write', {'token': token, 'bytes': bytes});
    return true;
  }
}
