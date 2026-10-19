import 'models.dart';

/// Presentation only: once this selected board has acknowledged a panel, a
/// transient transport/peripheral outage dims its controls instead of removing
/// the user's workspace. Never use this inventory to authorize an operation.
/// Adapted from PCController PeerSession's retained verified catalog.
class PanelInventory {
  String? _owner;
  final _features = <String>{};
  bool known = false, avrObserved = false;
  String role = '';
  bool tableSelection = false;
  String? get ownerKey => _owner;

  bool has(String feature) => _features.contains(feature);
  Set<String> get features => Set.unmodifiable(_features);
  bool get frontPanel =>
      has('display') || has('text') || has('buzzer') || has('lcd');

  void observe(DeviceStatus status) {
    final peer = status.peer;
    if (!peer.known) return;
    final board = status.json['board_id'];
    final owner =
        '${board is String && board.isNotEmpty ? board : peer.hostname}/'
        '${peer.kind}/${peer.application?.id}/${peer.role}';
    if (_owner != owner) {
      clear();
      _owner = owner;
    }
    known = true;
    role = peer.role;
    tableSelection |= peer.tableSelection;
    avrObserved |= status.json['avr_online'] is bool;
    _features.addAll(peer.features);
  }

  void clear() {
    _owner = null;
    _features.clear();
    known = avrObserved = false;
    role = '';
    tableSelection = false;
  }
}
