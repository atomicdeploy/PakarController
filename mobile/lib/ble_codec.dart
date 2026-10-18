import 'dart:typed_data';

const pakarBleService = 'a4b68d10-5c3f-4c5a-9f12-50616b617243';

class BleFrame {
  BleFrame(this.kind, this.id, this.offset, this.total, [Uint8List? payload])
    : payload = payload ?? Uint8List(0);
  final int kind, id, offset, total;
  final Uint8List payload;
  Uint8List encode() {
    final value = Uint8List(7 + payload.length);
    final header = ByteData.sublistView(value);
    header.setUint8(0, kind);
    header.setUint16(1, id, Endian.little);
    header.setUint16(3, offset, Endian.little);
    header.setUint16(5, total, Endian.little);
    value.setRange(7, value.length, payload);
    return value;
  }

  static BleFrame decode(Uint8List value) {
    if (value.length < 7 || value.length > 512) {
      throw const FormatException('Invalid BLE fragment size');
    }
    final h = ByteData.sublistView(value);
    final frame = BleFrame(
      h.getUint8(0),
      h.getUint16(1, Endian.little),
      h.getUint16(3, Endian.little),
      h.getUint16(5, Endian.little),
      Uint8List.sublistView(value, 7),
    );
    if (frame.id == 0 ||
        frame.total == 0 ||
        frame.offset > frame.total ||
        frame.kind > 1 ||
        frame.kind == 1 && frame.payload.isNotEmpty ||
        frame.kind == 0 &&
            (frame.payload.isEmpty ||
                frame.offset + frame.payload.length > frame.total)) {
      throw const FormatException('Invalid BLE fragment header');
    }
    return frame;
  }
}

class BleReceived {
  const BleReceived(this.ack, this.message);
  final Uint8List ack;
  final Uint8List? message;
}

/// Ordered bounded reassembly. Byte-exact duplicate wire fragments are ACKed
/// without redispatch; a replayed application command is never synthesized.
class BleAssembler {
  final _completed = <int, Uint8List>{};
  int _id = 0, _offset = 0, _last = 0;
  Uint8List? _bytes;
  BleReceived receive(BleFrame frame, int now) {
    if (frame.kind != 0) throw const FormatException('Expected BLE data');
    final cached = _completed[frame.id];
    if (cached != null) {
      if (cached.length != frame.total || !_matches(cached, frame)) {
        throw const FormatException(
          'BLE duplicate differs from committed bytes',
        );
      }
      return BleReceived(
        BleFrame(1, frame.id, cached.length, cached.length).encode(),
        null,
      );
    }
    if (_bytes != null && now - _last > 5000) resetPartial();
    if (_bytes == null) {
      if (frame.offset != 0) {
        throw const FormatException('BLE message must start at zero');
      }
      _id = frame.id;
      _offset = 0;
      _bytes = Uint8List(frame.total);
    }
    final buffer = _bytes!;
    if (frame.id != _id ||
        frame.total != buffer.length ||
        frame.offset > _offset) {
      throw const FormatException('Out-of-order/interleaved BLE message');
    }
    if (frame.offset < _offset) {
      if (frame.offset + frame.payload.length > _offset ||
          !_matches(buffer, frame)) {
        throw const FormatException('BLE duplicate differs from pending bytes');
      }
    } else {
      buffer.setRange(_offset, _offset + frame.payload.length, frame.payload);
      _offset += frame.payload.length;
    }
    _last = now;
    final ack = BleFrame(1, _id, _offset, buffer.length).encode();
    if (_offset != buffer.length) return BleReceived(ack, null);
    if (_completed.length >= 4) _completed.remove(_completed.keys.first);
    _completed[_id] = buffer;
    resetPartial();
    return BleReceived(ack, buffer);
  }

  bool _matches(Uint8List bytes, BleFrame frame) {
    if (frame.offset + frame.payload.length > bytes.length) return false;
    for (var i = 0; i < frame.payload.length; i++) {
      if (bytes[frame.offset + i] != frame.payload[i]) return false;
    }
    return true;
  }

  void resetPartial() {
    _id = 0;
    _offset = 0;
    _bytes = null;
    _last = 0;
  }

  void reset() {
    resetPartial();
    _completed.clear();
  }
}

int blePayloadBytes(int mtu) => (mtu.clamp(23, 517) - 3).clamp(20, 512) - 7;
