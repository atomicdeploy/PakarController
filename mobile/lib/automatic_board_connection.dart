import 'api.dart';
import 'discovery.dart';
import 'peer_context.dart';

import 'dart:io';

/// Discovery is a read-only suggestion, not a connection owner or actuator.
/// Adapt PCController's generation/enabled fences and bounded verified sweep.
class AutomaticBoardChoice {
  const AutomaticBoardChoice(this.peer, this.endpoint, this.route);
  final DiscoveredDevice peer;
  final String endpoint, route;
}

AutomaticBoardChoice? uniqueAutomaticBoard(DiscoveryCatalog catalog) {
  // Use this sweep's exact probes, never retained catalog/online badges. Collapse
  // station/AP/Ethernet routes by board_id before deciding ambiguity.
  // Classify the exact route BEFORE merging: a host bridge for the same board
  // must not erase an available physical LAN route (or lend it compatibility).
  final available = coalesceDevices(
    catalog.currentVerified
        .where(
          (p) =>
              p.compatible &&
              p.kind == 'board' &&
              !p.authRequired &&
              RegExp(r'^[!-~]{1,80}$').hasMatch(p.boardId) &&
              {'http', 'https'}.contains(Uri.parse(p.endpoint).scheme),
        )
        .toList(),
  );
  final ids = available.map((p) => p.boardId).toSet();
  if (ids.length != 1) return null;
  final peer = available.first;
  final uri = Uri.parse(peer.endpoint);
  final loopback =
      uri.host.toLowerCase() == 'localhost' ||
      InternetAddress.tryParse(uri.host)?.isLoopback == true;
  final named = !loopback && validHostname(peer.hostname)
      ? uri.replace(host: '${peer.hostname}.local').toString()
      : peer.endpoint;
  return AutomaticBoardChoice(
    peer,
    PakarApi.normalizeEndpoint(named),
    peer.endpoint,
  );
}

class AutomaticBoardConnector {
  AutomaticBoardConnector(this.api, this.discovery);
  final PakarApi api;
  final DeviceDiscovery discovery;
  Future<AutomaticBoardChoice?>? _pending;

  bool get allowed =>
      api.automaticDiscovery &&
      api.connectionWanted &&
      api.appForeground &&
      api.connectionMethod == ConnectionMethod.network &&
      api.unsupportedPeer == null &&
      !api.liveFresh &&
      api.freshStatus == null;

  AutomaticBoardChoice? currentChoice() =>
      allowed && discovery.verificationComplete
      ? uniqueAutomaticBoard(discovery.catalog)
      : null;

  Future<AutomaticBoardChoice?> discover({
    Iterable<DiscoveredDevice> seeds = const [],
  }) {
    if (!allowed) return Future.value();
    if (_pending != null) return _pending!;
    final owner = api.connectionRevision;
    late final Future<AutomaticBoardChoice?> task;
    task =
        (() async {
          await discovery.scan(seeds: seeds);
          if (owner != api.connectionRevision || !allowed) return null;
          return currentChoice();
        })().whenComplete(() {
          if (identical(_pending, task)) _pending = null;
        });
    return _pending = task;
  }
}
