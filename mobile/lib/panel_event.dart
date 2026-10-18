/// Requested ESP presentation, not confirmed physical segment/ACK telemetry.
/// Owner retirement and receipt-relative expiry adapt the existing donor-derived
/// PeripheralMirror path; this object never establishes a board capability.
class PanelEvent {
  const PanelEvent({
    required this.id,
    required this.uptimeMs,
    required this.kind,
    required this.value,
    required this.text,
    required this.preview,
    required this.lifetimeMs,
    required this.blink,
    required this.marquee,
    required this.priority,
  });
  final int id, uptimeMs, value, lifetimeMs, priority;
  final String kind, text, preview, blink, marquee;
  static const kinds = {
    'rest',
    'cell',
    'mode',
    'effect',
    'pixel',
    'network',
    'progress',
    'fault',
    'complete',
    'output',
    'menu',
    'scan',
    'state',
  };
  static bool _uint(Object? value, int maximum) =>
      value is int && value >= 0 && value <= maximum;
  static bool _ascii(Object? value, int maximum) =>
      value is String &&
      value.length <= maximum &&
      value.codeUnits.every((c) => c >= 0x20 && c <= 0x7e);
  static PanelEvent? decode(Map<String, dynamic> raw) {
    final p = raw['presentation'];
    if (raw['type'] != 'panel-event' ||
        raw['hardware_confirmed'] != false ||
        !_uint(raw['event_id'], 0xffffffff) ||
        !_uint(raw['uptime_ms'], 0xffffffff) ||
        !_uint(raw['value'], 0xffff) ||
        !kinds.contains(raw['kind']) ||
        !_ascii(raw['text'], 40) ||
        p is! Map ||
        !_ascii(p['text'], 4) ||
        (p['text'] as String).length != 4 ||
        !_uint(p['lifetime_ms'], 0xffff) ||
        !_uint(p['priority'], 255) ||
        !{'steady', 'slow', 'double-fast'}.contains(p['blink']) ||
        !{'once', 'none'}.contains(p['marquee'])) {
      return null;
    }
    return PanelEvent(
      id: raw['event_id'],
      uptimeMs: raw['uptime_ms'],
      kind: raw['kind'],
      value: raw['value'],
      text: raw['text'],
      preview: p['text'],
      lifetimeMs: p['lifetime_ms'],
      blink: p['blink'],
      marquee: p['marquee'],
      priority: p['priority'],
    );
  }
}
