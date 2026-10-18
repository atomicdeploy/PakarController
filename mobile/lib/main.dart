import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';

import 'icons.dart';

import 'package:shared_preferences/shared_preferences.dart';

import 'api.dart';
import 'board_sound_preferences.dart';
import 'models.dart';
import 'command_readback.dart';
import 'table_layout.dart';
import 'table_workspace.dart';
import 'connection_preferences.dart';
import 'cell_search.dart';
import 'peer_context.dart';
import 'application_artwork.dart';
import 'device_settings_page.dart';
import 'devices_page.dart';
import 'bridge_page.dart';
import 'firmware_page.dart';
import 'pixel_diagnostic.dart';
import 'pixel_diagnostic_page.dart';
import 'mirroring.dart';
import 'app_theme.dart';
import 'workspace_widgets.dart';
export 'app_theme.dart' show studioDark;

final bundledApplicationArtwork = base64Decode(applicationArtworkBase64);

void main() => runApp(const PakarControllerApp());

class PakarControllerApp extends StatelessWidget {
  const PakarControllerApp({super.key, this.api});
  final PakarApi? api;

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'PakarController',
    debugShowCheckedModeBanner: false,
    themeMode: ThemeMode.dark,
    darkTheme: studioTheme(),
    home: ControlShell(api: api),
  );
}

typedef Command = Future<void> Function(
  String path, [
  Map<String, String> values,
]);
typedef VerifiedCommand = Future<bool> Function(
  String path,
  Map<String, String> values,
);

class ControlShell extends StatefulWidget {
  const ControlShell({super.key, this.api});
  final PakarApi? api;
  @override
  State<ControlShell> createState() => _ControlShellState();
}

class _ControlShellState extends State<ControlShell>
    with WidgetsBindingObserver {
  late final api = widget.api ?? PakarApi();
  final mirror = PeripheralMirror();
  DeviceStatus? status;
  DeviceStatus? presented;
  final workspaces = <String, TableWorkspace>{};
  final pixelWorkspaces = <String, PixelDiagnosticWorkspace>{};
  String? workspaceBoard;
  String savedWorkspaces = '';
  bool foreground = true, liveActive = false;
  int retryAttempt = 0;
  Stopwatch? observedAge;
  Stopwatch? resumeProbeAge;
  String? observedEndpoint;
  Timer? resumeRead;
  CellMap mapping = CellMap(physical: const [], names: const []);
  bool mappingFresh = false;
  StreamSubscription<DeviceStatus>? live;
  Timer? fallback;
  Timer? liveRetry;
  Timer? expiry;
  int? refreshingRevision;
  int? mappingRevision;
  int mappingTicks = 0;
  int observation = 0;
  int liveGeneration = 0;
  String? incident;
  final reportedIncidents = <String>{};
  bool commandPending = false;
  String page = 'overview';
  String message = 'Connecting to ledboard.local';

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    api.onPeripheralEvent = mirror.event;
    api.onTransportRetired = () {
      mirror.observe(null, api.connectionRevision);
      if (mounted) {
        _revoke('Connection changed; waiting for fresh device state');
        if (observedEndpoint != api.baseUrl) {
          setState(() {
            presented = null;
            mapping = CellMap(physical: const [], names: const []);
          });
        }
      }
    };
    api.onPeerChanged = () {
      if (!mounted || !api.connectionWanted) return;
      _revoke('Board/boot identity changed; previous operations retired');
      unawaited(_connectLive());
    };
    _restore();
  }

  Future<void> _restore() async {
    final prefs = await SharedPreferences.getInstance();
    final saved = prefs.getString('table_workspaces');
    if (saved != null && saved.length < 65536) {
      try {
        final cache = jsonDecode(saved);
        final entries = cache is Map ? cache['boards'] : null;
        if (cache is Map &&
            [
              'overview',
              'table',
              'front-panel',
              'hardware',
              'device-settings',
              'connection',
              'devices',
            ].contains(cache['activity'])) {
          page = cache['activity'] as String;
        }
        if (entries is Map<String, dynamic>) {
          for (final entry in entries.entries.take(16)) {
            if (entry.key.length <= 160 &&
                entry.value is Map<String, dynamic>) {
              workspaces[entry.key] = TableWorkspace(
                saved: entry.value as Map<String, dynamic>,
              );
            }
          }
        }
        savedWorkspaces = saved;
      } on FormatException {
        /* Ignore invalid UI cache, never device settings. */
      }
    }
    try {
      await ConnectionPreferences(prefs).restore(api);
    } on Exception catch (error) {
      if (mounted) {
        setState(() => message = 'Preferences could not be restored: $error');
      }
      return;
    }
    if (!mounted) return;
    if (api.connectionWanted) await _startConnection();
  }

  Future<void> _startConnection() async {
    _startSupervision();
    await _refresh();
    if (mounted && api.connectionWanted && foreground) {
      unawaited(_connectLive());
    }
  }

  void _startSupervision() {
    fallback?.cancel();
    fallback = Timer.periodic(const Duration(seconds: 3), (_) {
      if (!foreground || !api.connectionWanted) return;
      if (!api.liveFresh) {
        if (liveActive &&
            (api.liveSilence ?? Duration.zero) >= const Duration(seconds: 8) &&
            (resumeProbeAge?.elapsed ?? const Duration(seconds: 8)) >=
                const Duration(seconds: 8)) {
          unawaited(_connectLive());
        } else if (liveActive) {
          api.requestLiveStatus();
        }
        unawaited(_refresh(silent: true));
      } else if (!commandPending &&
          (!mappingFresh || !mapping.valid || ++mappingTicks >= 20)) {
        unawaited(_syncMapping());
      }
    });
  }

  Future<void> _connectLive() async {
    if (!mounted || !api.connectionWanted || !foreground) return;
    final owner = api.connectionRevision;
    // Adapt donor StatusFrameBatcher's generation retirement: an old socket's
    // error/done callbacks cannot revoke or schedule work for its replacement.
    final generation = ++liveGeneration;
    liveActive = true;
    liveRetry?.cancel();
    await live?.cancel();
    if (!mounted ||
        !api.connectionWanted ||
        owner != api.connectionRevision ||
        generation != liveGeneration) {
      return;
    }
    var ended = false;
    void disconnected() {
      if (ended ||
          !mounted ||
          !api.connectionWanted ||
          owner != api.connectionRevision ||
          generation != liveGeneration) {
        return;
      }
      ended = true;
      liveActive = false;
      // A failed WS does not invalidate a still-fresh authenticated HTTP
      // snapshot. Its own lease expires if the fallback read also fails.
      if (foreground) _retryLive();
    }

    live = api.liveStatus().listen(
      (value) {
        if (mounted &&
            api.connectionWanted &&
            owner == api.connectionRevision &&
            generation == liveGeneration) {
          retryAttempt = 0;
          _observe(value);
        }
      },
      onError: (Object _) => disconnected(),
      onDone: disconnected,
      cancelOnError: true,
    );
  }

  void _retryLive() {
    if (!mounted || !api.connectionWanted || !foreground) return;
    liveRetry?.cancel();
    final seconds = [1, 2, 4, 8, 15][retryAttempt.clamp(0, 4)];
    retryAttempt++;
    liveRetry = Timer(Duration(seconds: seconds), _connectLive);
  }

  Future<void> _refresh({bool silent = false}) async {
    final owner = api.connectionRevision, started = observation;
    if (!mounted ||
        !api.connectionWanted ||
        !foreground ||
        commandPending ||
        refreshingRevision == owner) {
      return;
    }
    refreshingRevision = owner;
    try {
      final value = await api.status();
      if (!mounted ||
          owner != api.connectionRevision ||
          started != observation) {
        return;
      }
      _observe(value);
      if (value.peer.table && (!mappingFresh || !mapping.valid || !silent)) {
        unawaited(_syncMapping());
      }
      if (!mounted || owner != api.connectionRevision) return;
      setState(() {
        if (!silent) message = 'Live device synchronized';
      });
    } catch (error) {
      if (mounted &&
          owner == api.connectionRevision &&
          started == observation) {
        if (error is AuthenticationRequired ||
            error is StaleConnection ||
            observedAge == null ||
            observedAge!.elapsed >= const Duration(seconds: 6)) {
          _revoke(error.toString());
        }
      }
    } finally {
      if (refreshingRevision == owner) refreshingRevision = null;
    }
  }

  Future<void> _syncMapping() async {
    final owner = api.connectionRevision, peer = status;
    if (!mounted ||
        !api.connectionWanted ||
        !foreground ||
        peer?.peer.table != true ||
        mappingRevision == owner) {
      return;
    }
    mappingRevision = owner;
    mappingTicks = 0;
    try {
      final next = await api.mapping();
      if (mounted &&
          owner == api.connectionRevision &&
          status?.peer.table == true &&
          peer!.hostname == status?.hostname &&
          peer.json['board_id'] == status?.json['board_id'] &&
          peer.json['boot_id'] == status?.json['boot_id']) {
        // Fresh same-peer WS ticks must not invalidate an in-flight map read.
        setState(() {
          mapping = next;
          mappingFresh = true;
        });
      }
    } on Exception {
      // Optional read failures do not erase this same boot's confirmed map.
    } finally {
      if (mappingRevision == owner) mappingRevision = null;
    }
  }

  void _observe(DeviceStatus value) {
    if (!mounted || !api.connectionWanted) return;
    if (value.olderThan(status)) return;
    mirror.observe(value, api.connectionRevision);
    final board =
        value.json['board_id'] as String? ?? '${api.baseUrl}|${value.hostname}';
    final changed = workspaceBoard != board;
    if (changed && workspaces.length >= 16 && !workspaces.containsKey(board)) {
      final retired = workspaces.keys.firstWhere(
        (key) => key != workspaceBoard,
      );
      workspaces.remove(retired)?.dispose();
    }
    if (value.peer.table) workspaces.putIfAbsent(board, TableWorkspace.new);
    final bootChanged = presented?.json['boot_id'] != value.json['boot_id'];
    workspaceBoard = board;
    observedAge = Stopwatch()..start();
    observedEndpoint = api.baseUrl;
    resumeRead?.cancel();
    expiry?.cancel();
    ++observation;
    setState(() {
      status = value;
      presented = value;
      if (incident != null) message = 'Live device synchronized';
      incident = null;
      if (changed || !value.peer.table) {
        mapping = CellMap(physical: const [], names: const []);
      }
      if (changed || bootChanged || !value.peer.table) mappingFresh = false;
    });
    _armExpiry();
    if (value.peer.table && (!mappingFresh || !mapping.valid) && foreground) {
      unawaited(_syncMapping());
    }
  }

  void _armExpiry() {
    expiry?.cancel();
    if (!foreground || !api.connectionWanted || status == null) return;
    final remaining =
        const Duration(seconds: 6) - (observedAge?.elapsed ?? Duration.zero);
    expiry = Timer(remaining.isNegative ? Duration.zero : remaining, () {
      if (mounted && foreground) {
        _revoke('Live state unavailable; reconnecting…');
      }
    });
  }

  Future<void> _saveWorkspaces() async {
    final encoded = jsonEncode({
      'activity': page,
      'boards': {
        for (final entry in workspaces.entries)
          entry.key: entry.value.snapshot(),
      },
    });
    if (encoded == savedWorkspaces) return;
    final prefs = await SharedPreferences.getInstance();
    if (await prefs.setString('table_workspaces', encoded)) {
      savedWorkspaces = encoded;
    }
  }

  void _revoke(String reason) {
    mirror.observe(null, api.connectionRevision);
    expiry?.cancel();
    if (status != null) ++observation;
    if (!mounted) return;
    if (status == null && incident == reason) return;
    setState(() {
      status = null;
      message = reason;
      incident = reason;
    });
  }

  void _reportIncident(String key, String reason) {
    if (!mounted || !reportedIncidents.add(key)) return;
    setState(() {
      incident = key;
      message = reason;
    });
  }

  Future<void> _disconnect() async {
    // Retire before awaiting cancellation: no callback may reconnect afterward.
    ++liveGeneration;
    liveActive = false;
    reportedIncidents.clear();
    api.disconnect();
    fallback?.cancel();
    liveRetry?.cancel();
    resumeRead?.cancel();
    _revoke('Disconnected');
    setState(() => presented = null);
    final previous = live;
    live = null;
    await previous?.cancel();
  }

  Future<void> _reconnect() async {
    ++liveGeneration;
    api.connect();
    _revoke('Connecting to ${api.baseUrl}');
    liveRetry?.cancel();
    await live?.cancel();
    if (mounted) await _startConnection();
  }

  Future<void> _adoptConnection(
    int owner,
    String endpoint,
    String user,
    String password, {
    bool verifyCredentialChange = true,
  }) async {
    if (!mounted || owner != api.connectionRevision) {
      throw const StaleConnection();
    }
    final changedLogin = user != api.user || password != api.password;
    // A acknowledged identity/credential operation may close the old socket
    // before its HTTP ACK arrives. Connection revision, not that dead socket,
    // owns this explicit handoff. Never apply it to a replacement connection.
    api.baseUrl = endpoint;
    api.user = user;
    api.password = password;
    final adopted = api.connectionRevision;
    _revoke('Following acknowledged device configuration');
    try {
      final preferences = await SharedPreferences.getInstance();
      if (!mounted || adopted != api.connectionRevision) {
        throw const StaleConnection();
      }
      await ConnectionPreferences(preferences).save(endpoint, user, password);
      if (!mounted || adopted != api.connectionRevision) {
        throw const StaleConnection();
      }
      if (changedLogin && verifyCredentialChange) {
        final actual = await api.getJson('/api/auth');
        if (actual['username'] != user || actual['password_set'] != true) {
          throw const FormatException(
            'Credentials acknowledged but authenticated readback differs',
          );
        }
      }
      await _reconnect();
    } catch (error) {
      if (mounted && adopted == api.connectionRevision) {
        setState(
          () => message =
              '$error No write was replayed. Connection values remain in this session.',
        );
      }
      rethrow;
    }
  }

  Future<void> command(
    String path, [
    Map<String, String> values = const {},
  ]) async {
    final owner = api.connectionRevision, peer = status?.hostname;
    await _command(owner, peer, path, values);
  }

  Future<bool> _command(
    int owner,
    String? peer,
    String path,
    Map<String, String> values,
  ) async {
    if (commandPending) return false;
    try {
      if (!foreground ||
          !api.connectionWanted ||
          owner != api.connectionRevision ||
          peer != status?.hostname ||
          status?.peer.allows(path, values) != true) {
        throw const FormatException(
          'This peer has not freshly acknowledged this capability',
        );
      }
      commandPending = true;
      final fast = await api.fastCommand(
        path,
        values,
        authorize: () =>
            mounted &&
            owner == api.connectionRevision &&
            peer == status?.hostname &&
            status?.peer.allows(path, values) == true,
      );
      late final DeviceStatus actual;
      if (fast != null) {
        actual = fast;
      } else {
        // Fresh preflight may discover a reboot, retiring the original intent.
        final before = await api.status();
        if (!mounted ||
            !api.connectionWanted ||
            owner != api.connectionRevision ||
            peer != before.hostname ||
            !before.peer.allows(path, values)) {
          throw const StaleConnection();
        }
        if (path == '/api/cells') {
          final count = parseCellSearch(values['ids'] ?? '').length;
          final ack = await api.postJson(
            path,
            values,
            authorize: () =>
                mounted &&
                owner == api.connectionRevision &&
                status?.peer.allows(path, values) == true,
          );
          if (count == 0 || ack['accepted'] != true || ack['count'] != count) {
            throw const OperationUncertain();
          }
        } else {
          await api.post(path, values);
        }
        if (!mounted || owner != api.connectionRevision) return false;
        actual = await api.status();
      }
      if (!mounted ||
          !api.connectionWanted ||
          owner != api.connectionRevision ||
          peer != actual.hostname ||
          !actual.peer.allows(path, values)) {
        throw const StaleConnection();
      }
      _observe(actual);
      if (!commandReadbackMatches(actual, path, values)) {
        throw const FormatException(
          'Command acknowledged but device readback differs; not retried',
        );
      }
      if (path == '/api/cells/mapping') {
        final actualMap = await api.mapping();
        final id = int.tryParse(values['logical_id'] ?? '');
        if (!mounted ||
            owner != api.connectionRevision ||
            status?.hostname != peer ||
            id == null ||
            id < 1 ||
            id > actualMap.physical.length ||
            actualMap.physical[id - 1] !=
                int.tryParse(values['physical_cell'] ?? '') ||
            actualMap.names[id - 1] != values['name']) {
          throw const FormatException('Mapping readback differs; not retried');
        }
        setState(() {
          mapping = actualMap;
          mappingFresh = true;
        });
      }
      setState(
        () => message = 'Device acknowledged; fresh state read back (not physical output proof)',
      );
      return true;
    } catch (error) {
      if (mounted && owner == api.connectionRevision) {
        if (error is StaleConnection) {
          _revoke(error.toString());
        } else {
          _reportIncident(
            'command:${error.runtimeType}:$error',
            error.toString(),
          );
        }
      }
      return false;
    } finally {
      commandPending = false;
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    final active = state == AppLifecycleState.resumed;
    api.appForeground = active;
    mirror.foreground(active);
    if (foreground == active) return;
    foreground = active;
    if (!active) {
      fallback?.cancel();
      liveRetry?.cancel();
      expiry?.cancel();
      resumeRead?.cancel();
      unawaited(
        _saveWorkspaces().catchError((Object _) {
          // A UI-cache write failure never resets device settings or telemetry.
        }),
      );
      setState(() {});
      return; // Keep the socket and workspace; Android may suspend transport.
    }
    if (!api.connectionWanted) {
      setState(() {});
      return;
    }
    _startSupervision();
    if (api.liveFresh && status != null) {
      _armExpiry();
      setState(() {});
      return;
    }
    _revoke('Resuming live updates…');
    resumeProbeAge = Stopwatch()..start();
    if (!liveActive) {
      unawaited(_connectLive());
    } else {
      api.requestLiveStatus();
    }
    resumeRead = Timer(const Duration(milliseconds: 1200), () {
      if (mounted && foreground && api.connectionWanted && !api.liveFresh) {
        unawaited(_refresh(silent: true));
      }
    });
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    api.onPeripheralEvent = null;
    api.onTransportRetired = null;
    mirror.dispose();
    api.onPeerChanged = null;
    live?.cancel();
    fallback?.cancel();
    liveRetry?.cancel();
    expiry?.cancel();
    resumeRead?.cancel();
    for (final workspace in workspaces.values) {
      workspace.dispose();
    }
    api.close();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final peer = presented?.peer ?? PeerContext.unknown;
    final owner = api.connectionRevision, hostname = status?.hostname;
    // Retained dialogs hold this owner: they cannot write to a replacement peer.
    Future<void> scoped(
      String path, [
      Map<String, String> values = const {},
    ]) async {
      await _command(owner, hostname, path, values);
    }

    final pages = <(String, String, IconData)>[
      ('overview', 'Overview', PakarIcons.broadcast),
      if (peer.table) ('table', 'Table', PakarIcons.gridFour),
      if (peer.frontPanel) ('front-panel', 'Front panel', PakarIcons.gauge),
      if (peer.has('telemetry') || peer.has('network'))
        ('hardware', 'Hardware', PakarIcons.circuitry),
      if (peer.known) ('device-settings', 'Device settings', PakarIcons.gear),
      ('connection', 'Connection', PakarIcons.gear),
      ('devices', 'Devices', PakarIcons.broadcast),
    ];
    final active = pages.indexWhere((item) => item.$1 == page);
    final index = active < 0 ? 0 : active;
    final wide = MediaQuery.sizeOf(context).width >= 840;
    final destinations = pages
        .map(
          (item) => NavigationDestination(icon: Icon(item.$3), label: item.$2),
        )
        .toList();
    final body = IndexedStack(
      index: index,
      children: [
        for (final item in pages)
          switch (item.$1) {
            'table' => TablePage(
              key: ValueKey('table:$workspaceBoard'),
              status: presented,
              connected: status != null && foreground,
              mappingCurrent: mappingFresh,
              workspace: workspaces[workspaceBoard],
              mapping: mapping,
              command: scoped,
              verifiedCommand: (path, values) =>
                  _command(owner, hostname, path, values),
            ),
            'front-panel' => IgnorePointer(
              key: ValueKey('front:$workspaceBoard'),
              ignoring: status == null || !foreground,
              child: FrontPanelPage(
                status: presented,
                command: scoped,
                mirror: mirror,
              ),
            ),
            'hardware' => HardwarePage(
              key: ValueKey('hardware:$workspaceBoard'),
              status: status,
            ),
            'device-settings' => IgnorePointer(
              key: ValueKey('device-settings:$owner:$workspaceBoard'),
              ignoring: status == null || !foreground,
              child: DeviceSettingsPage(
                api: api,
                status: presented!,
                active:
                    page == 'device-settings' && foreground && status != null,
                authorized: (feature) =>
                    mounted &&
                    foreground &&
                    owner == api.connectionRevision &&
                    status?.hostname == hostname &&
                    status?.peer.has(feature) == true,
                navigate: (target) => setState(() => page = target),
                adoptConnection: (endpoint, user, password) =>
                    _adoptConnection(owner, endpoint, user, password),
              ),
            ),
            'connection' => SettingsPage(
              key: const ValueKey('connection-preferences'),
              api: api,
              reconnect: _reconnect,
              active: page == 'connection' && foreground,
            ),
            'devices' =>
              page != 'devices'
                  ? const SizedBox.shrink(key: ValueKey('devices-hidden'))
                  : DevicesPage(
                      select: (device, password) async {
                        await _adoptConnection(
                          owner,
                          device.endpoint,
                          device.user,
                          password,
                          verifyCredentialChange: false,
                        );
                        if (mounted) setState(() => page = 'overview');
                      },
                    ),
            _ => BoardOverview(
              key: ValueKey('overview:$workspaceBoard'),
              status: status,
            ),
          },
      ],
    );
    final artwork = peer.application?.icon ?? bundledApplicationArtwork;
    return Title(
      title: peer.title,
      color: studioDark.surface,
      child: Scaffold(
        appBar: AppBar(
          leading: Padding(
            padding: const EdgeInsets.all(12),
            child: Image.memory(
              artwork,
              errorBuilder: (_, _, _) =>
                  Image.memory(bundledApplicationArtwork),
            ),
          ),
          title: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                peer.application?.title ?? 'PakarController',
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(fontWeight: FontWeight.w700),
              ),
              Text(
                peer.known
                    ? '${peer.name} · ${peer.hostname}.local'
                    : 'No acknowledged board',
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(fontSize: 11, color: Color(0xff909baa)),
              ),
            ],
          ),
          actions: [
            IconButton(
              onPressed: api.connectionWanted ? _disconnect : _reconnect,
              tooltip: api.connectionWanted ? 'Disconnect' : 'Connect',
              icon: Icon(
                api.connectionWanted ? PakarIcons.check : PakarIcons.link,
              ),
            ),
            PopupMenuButton<String>(
              tooltip: 'Device context menu',
              onSelected: (target) async {
                if (target == 'devices') {
                  setState(() => page = 'devices');
                  return;
                }
                var locked = false;
                final pixelBoard = status?.json['board_id'];
                if (target == 'pixels' && pixelBoard is! String) return;
                if (pixelWorkspaces.length >= 16 &&
                    !pixelWorkspaces.containsKey(pixelBoard)) {
                  pixelWorkspaces.remove(pixelWorkspaces.keys.first);
                }
                final pixelWorkspace = target == 'pixels'
                    ? pixelWorkspaces.putIfAbsent(
                        pixelBoard as String,
                        PixelDiagnosticWorkspace.new,
                      )
                    : null;
                await Navigator.of(context).push(
                  MaterialPageRoute<void>(
                    builder: (context) => StatefulBuilder(
                      builder: (context, update) => PopScope(
                        canPop: !locked,
                        child: Scaffold(
                          appBar: AppBar(
                            title: Text(
                              target == 'bridge'
                                  ? 'Advanced bridge'
                                  : target == 'pixels'
                                  ? 'Pixel diagnostics'
                                  : 'Firmware',
                            ),
                            automaticallyImplyLeading: !locked,
                          ),
                          body: target == 'bridge'
                              ? BridgePage(api: api, observed: () => status)
                              : target == 'pixels'
                              ? PixelDiagnosticPage(
                                  api: api,
                                  observed: () => status,
                                  onObserved: _observe,
                                  mapping: () => mappingFresh ? mapping : null,
                                  workspace: pixelWorkspace,
                                  busyChanged: (value) {
                                    commandPending = value;
                                    if (context.mounted) {
                                      update(() => locked = value);
                                    }
                                  },
                                )
                              : FirmwarePage(
                                  api: api,
                                  authorized: (feature) =>
                                      mounted &&
                                      owner == api.connectionRevision &&
                                      status?.hostname == hostname &&
                                      status?.peer.has(feature) == true,
                                  busyChanged: (value) {
                                    if (context.mounted) {
                                      update(() => locked = value);
                                    }
                                  },
                                ),
                        ),
                      ),
                    ),
                  ),
                );
              },
              itemBuilder: (_) => [
                const PopupMenuItem(
                  value: 'devices',
                  child: ListTile(
                    dense: true,
                    leading: Icon(PakarIcons.broadcast),
                    title: Text('Devices…'),
                  ),
                ),
                if (peer.has('isp') || peer.has('ota'))
                  const PopupMenuItem(
                    value: 'firmware',
                    child: ListTile(
                      dense: true,
                      leading: Icon(PakarIcons.cpu),
                      title: Text('Firmware upload / download…'),
                    ),
                  ),
                if (supportsPixelDiagnostic(status) && peer.has('command-ack'))
                  const PopupMenuItem(
                    value: 'pixels',
                    child: ListTile(
                      dense: true,
                      leading: Icon(PakarIcons.circuitry),
                      title: Text('Pixel diagnostics…'),
                    ),
                  ),
                const PopupMenuItem(
                  value: 'bridge',
                  child: ListTile(
                    dense: true,
                    leading: Icon(PakarIcons.link),
                    title: Text('Advanced MQTT / bridge…'),
                  ),
                ),
              ],
            ),
            IconButton(
              onPressed: api.connectionWanted ? _refresh : null,
              tooltip: 'Refresh device state',
              icon: const Icon(PakarIcons.arrowClockwise),
            ),
          ],
        ),
        body: Row(
          children: [
            if (wide)
              NavigationRail(
                selectedIndex: index,
                onDestinationSelected: (value) =>
                    setState(() => page = pages[value].$1),
                labelType: NavigationRailLabelType.all,
                destinations: destinations
                    .map(
                      (item) => NavigationRailDestination(
                        icon: item.icon,
                        label: Text(item.label),
                      ),
                    )
                    .toList(),
              ),
            Expanded(
              child: Column(
                children: [
                  Expanded(child: body),
                  StatusBar(status: status, message: message),
                ],
              ),
            ),
          ],
        ),
        bottomNavigationBar: wide
            ? null
            : NavigationBar(
                selectedIndex: index,
                onDestinationSelected: (value) =>
                    setState(() => page = pages[value].$1),
                destinations: destinations,
              ),
      ),
    );
  }
}

class BoardOverview extends StatelessWidget {
  const BoardOverview({super.key, required this.status});
  final DeviceStatus? status;
  @override
  Widget build(BuildContext context) {
    final peer = status?.peer ?? PeerContext.unknown;
    return ListView(
      padding: const EdgeInsets.all(24),
      children: [
        Text(
          peer.known ? peer.name : 'Connect to a board',
          style: Theme.of(context).textTheme.headlineSmall,
        ),
        const SizedBox(height: 12),
        Text(
          peer.known ? '${peer.hostname}.local' : 'Use Connection to choose the hostname and credentials. Controls appear only after individual capability acknowledgment.',
        ),
        if (peer.known) ...[
          const SizedBox(height: 20),
          const Text('Acknowledged capabilities'),
          const SizedBox(height: 8),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              for (final feature in peer.features) Chip(label: Text(feature)),
            ],
          ),
        ],
      ],
    );
  }
}

class TablePage extends StatefulWidget {
  const TablePage({
    super.key,
    required this.status,
    required this.mapping,
    required this.command,
    this.verifiedCommand,
    this.workspace,
    this.connected = true,
    this.mappingCurrent = true,
  });
  final DeviceStatus? status;
  final CellMap mapping;
  final Command command;
  final VerifiedCommand? verifiedCommand;
  final TableWorkspace? workspace;
  final bool connected;
  final bool mappingCurrent;

  @override
  State<TablePage> createState() => _TablePageState();
}

class _TablePageState extends State<TablePage> {
  DeviceStatus? get status => widget.status;
  CellMap get mapping => widget.mapping;
  Command get command => widget.command;
  late final workspace = widget.workspace ?? TableWorkspace();
  TextEditingController get query => workspace.query;
  ScrollController get tableScroll => workspace.tableScroll;
  ScrollController get pageScroll => workspace.pageScroll;
  Set<int> get selection => workspace.selection;
  set selection(Set<int> value) => workspace.selection = value;
  String? searchError;
  bool get multiple => workspace.multiple;
  set multiple(bool value) => workspace.multiple = value;
  bool get showIndicators => workspace.showIndicators;
  set showIndicators(bool value) => workspace.showIndicators = value;
  String get cellEffect => workspace.cellEffect;
  set cellEffect(String value) => workspace.cellEffect = value;
  bool applying = false;
  bool outputPending = false;
  String? batchMessage;

  @override
  void initState() {
    super.initState();
    try {
      selection = parseCellSearch(query.text);
    } on FormatException catch (error) {
      searchError = error.message;
    }
  }

  @override
  void dispose() {
    if (widget.workspace == null) workspace.dispose();
    super.dispose();
  }

  void select(int id) {
    FocusScope.of(context).unfocus();
    setState(() {
      if (multiple) {
        if (!selection.remove(id)) selection.add(id);
      } else {
        selection = {id};
      }
      query.text = formatCellSelection(selection);
      searchError = null;
    });
  }

  void replaceSelection(Set<int> ids) {
    FocusScope.of(context).unfocus();
    setState(() {
      selection = ids;
      query.text = formatCellSelection(selection);
      searchError = null;
    });
  }

  Future<void> setOutputEnabled(bool enabled) async {
    if (!widget.connected ||
        outputPending ||
        status?.ledOutputEnabled == null) {
      return;
    }
    setState(() => outputPending = true);
    try {
      await command('/api/led/output', {'enabled': '$enabled'});
      // Only a fresh peer observation changes the switch, never the request.
    } finally {
      if (mounted) setState(() => outputPending = false);
    }
  }

  Future<void> setOutputMode(String mode) async {
    if (!widget.connected ||
        outputPending ||
        status?.ledOutputMode == null ||
        !const ['static', 'effects'].contains(mode)) {
      return;
    }
    setState(() => outputPending = true);
    try {
      await command('/api/led/output', {'mode': mode});
    } finally {
      if (mounted) setState(() => outputPending = false);
    }
  }

  Future<void> rainbowTest() async {
    if (!widget.connected || outputPending || status?.ledOutputMode == null) {
      return;
    }
    setState(() => outputPending = true);
    try {
      // Only a pattern command: keep observed output On/Off and mode unchanged.
      await command('/api/effect', {'value': 'rainbow'});
    } finally {
      if (mounted) setState(() => outputPending = false);
    }
  }

  Future<void> applySelection(String state) async {
    final send = widget.verifiedCommand;
    if (send == null ||
        !widget.connected ||
        applying ||
        selection.isEmpty ||
        searchError != null ||
        status?.peer.has('cell-batch') != true) {
      return;
    }
    final ids = selection.toList()..sort();
    setState(() {
      applying = true;
      batchMessage = null;
    });
    var applied = false;
    try {
      applied = await send('/api/cells', {
        'ids': ids.join(','),
        'state': state,
        'effect': cellEffect,
      });
    } on Exception {
      // A transport failure may happen after the atomic mutation. Never fan
      // out/replay; require a fresh peer observation before another intent.
    }
    if (mounted) {
      setState(() {
        applying = false;
        batchMessage = applied
            ? '${ids.length} cell states read back after one board-side update; physical output not verified.'
            : 'Batch outcome unverified. No per-cell fallback or retry was sent.';
      });
    }
  }

  @override
  Widget build(BuildContext context) => status?.peer.table != true
      ? const Center(child: Text('No acknowledged table'))
      : PageStorage(
          bucket: workspace.bucket,
          child: ListView(
            key: const PageStorageKey('table-page-scroll'),
            controller: pageScroll,
            primary: false,
            padding: const EdgeInsets.all(16),
            children: [
              Row(
                children: [
                  const Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          '80-cell table',
                          style: TextStyle(
                            fontSize: 24,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                        Text(
                          'Display: 1, 11, 2, 12… · logical IDs and physical mapping unchanged',
                        ),
                      ],
                    ),
                  ),
                  FilledButton.icon(
                    onPressed:
                        !widget.connected ||
                            status?.peer.modes.any(
                                  (mode) => mode.id == 'first-wins',
                                ) !=
                                true
                        ? null
                        : () => command('/api/game/reset'),
                    icon: const Icon(PakarIcons.arrowCounterClockwise),
                    label: const Text('Reset'),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  for (final mode in status!.peer.modes)
                    ChoiceChip(
                      label: Text(mode.label),
                      selected: status!.mode == mode.id,
                      onSelected: widget.connected
                          ? (_) => command('/api/mode', {'value': mode.id})
                          : null,
                    ),
                ],
              ),
              const SizedBox(height: 12),
              if (status!.ledOutputEnabled != null)
                SwitchListTile.adaptive(
                  title: const Text('LED output service'),
                  subtitle: SizedBox(
                    height: 40,
                    child: Text(
                      outputPending
                          ? 'Waiting for device; showing last observed state.'
                          : 'Off clears the strip; modes and settings stay available.',
                      maxLines: 2,
                    ),
                  ),
                  value: status!.ledOutputEnabled!,
                  onChanged: !widget.connected || outputPending
                      ? null
                      : setOutputEnabled,
                ),
              if (status!.ledOutputMode != null) ...[
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  crossAxisAlignment: WrapCrossAlignment.center,
                  children: [
                    for (final mode in const ['static', 'effects'])
                      ChoiceChip(
                        label: Text(mode == 'static' ? 'Static' : 'Effects'),
                        selected: status!.ledOutputMode == mode,
                        onSelected: !widget.connected || outputPending
                            ? null
                            : (_) => setOutputMode(mode),
                      ),
                    OutlinedButton(
                      onPressed: !widget.connected || outputPending
                          ? null
                          : rainbowTest,
                      child: const Text('Rainbow test'),
                    ),
                  ],
                ),
                SizedBox(
                  height: 36,
                  child: Text(
                    status!.ledOutputMode == 'static'
                        ? 'Static cells: six matching pixels. Rainbow test animates explicitly.'
                        : 'Effects: animated output enabled explicitly.',
                    style: Theme.of(context).textTheme.bodySmall,
                    maxLines: 2,
                  ),
                ),
                const SizedBox(height: 12),
              ],
              TextField(
                controller: query,
                decoration: InputDecoration(
                  labelText: 'Find logical cell',
                  hintText: '12, 3–8',
                  errorText: searchError,
                ),
                onChanged: (text) => setState(() {
                  try {
                    selection = parseCellSearch(text);
                    searchError = null;
                  } on FormatException catch (error) {
                    searchError = error.message;
                  }
                }),
                onSubmitted: (_) {
                  if (searchError == null) {
                    setState(() => query.text = formatCellSelection(selection));
                  }
                },
              ),
              Wrap(
                spacing: 8,
                runSpacing: 4,
                children: [
                  FilterChip(
                    label: const Text('Multiple selection'),
                    selected: multiple,
                    onSelected: (value) => setState(() => multiple = value),
                  ),
                  FilterChip(
                    label: const Text('LED indicators'),
                    selected: showIndicators,
                    onSelected: (value) =>
                        setState(() => showIndicators = value),
                  ),
                  TextButton(
                    onPressed: applying
                        ? null
                        : () => replaceSelection({
                            for (var id = 1; id <= 80; id++) id,
                          }),
                    child: const Text('Select all'),
                  ),
                  TextButton(
                    onPressed: applying
                        ? null
                        : () => replaceSelection({
                            for (var id = 1; id <= 80; id++)
                              if (!selection.contains(id)) id,
                          }),
                    child: const Text('Invert selection'),
                  ),
                  TextButton(
                    onPressed: applying
                        ? null
                        : () => setState(() {
                            selection = {};
                            query.clear();
                            searchError = null;
                          }),
                    child: const Text('Clear selection'),
                  ),
                  for (final value in ['white', 'red', 'off'])
                    FilledButton.tonal(
                      onPressed:
                          !widget.connected ||
                              selection.isEmpty ||
                              applying ||
                              searchError != null ||
                              widget.verifiedCommand == null ||
                              !status!.peer.has('cell-batch')
                          ? null
                          : () => applySelection(value),
                      child: Text('Set $value'),
                    ),
                ],
              ),
              DropdownButtonFormField<String>(
                initialValue: cellEffect,
                decoration: const InputDecoration(
                  labelText: 'Cell transition · board-side',
                ),
                items: [
                  for (final effect in const ['immediate', 'fade', 'pulse'])
                    DropdownMenuItem(value: effect, child: Text(effect)),
                ],
                onChanged: applying
                    ? null
                    : (value) => setState(() => cellEffect = value!),
              ),
              if (!status!.peer.has('cell-batch'))
                const Text(
                  'This board has not acknowledged atomic cell updates. Update its firmware to enable selection actions.',
                ),
              const SizedBox(height: 12),
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(12),
                  child: LayoutBuilder(
                    builder: (context, bounds) {
                      // Keep 44px cell targets rather than squeezing 20 columns
                      // onto a phone. Horizontal panning owns its own gestures.
                      final minimumWidth = 48.0 * tableDisplayWidth;
                      final width = bounds.maxWidth < minimumWidth
                          ? minimumWidth
                          : bounds.maxWidth;
                      final pitch = width / tableDisplayWidth;
                      return Scrollbar(
                        controller: tableScroll,
                        thumbVisibility: true,
                        child: SingleChildScrollView(
                          key: const ValueKey('table-cell-scroll'),
                          controller: tableScroll,
                          primary: false,
                          scrollDirection: Axis.horizontal,
                          // Physical cell order is left-to-right even when the
                          // surrounding application is displayed in RTL.
                          reverse:
                              Directionality.of(context) == TextDirection.rtl,
                          child: SizedBox(
                            key: const ValueKey('table-cell-canvas'),
                            width: width,
                            height: pitch * tableDisplayHeight + 12,
                            child: Stack(
                              children: [
                                for (var id = 1; id <= 80; id++)
                                  Positioned(
                                    left: tableCellPosition(id).x * pitch + 2,
                                    top: tableCellPosition(id).y * pitch + 2,
                                    width: pitch - 4,
                                    height: pitch - 4,
                                    child: CellButton(
                                      key: ValueKey('cell-$id'),
                                      id: id,
                                      highlighted: selection.contains(id),
                                      showIndicator: showIndicators,
                                      name:
                                          mapping.names.elementAtOrNull(
                                            id - 1,
                                          ) ??
                                          '$id',
                                      physical:
                                          mapping.physical.elementAtOrNull(
                                            id - 1,
                                          ) ??
                                          0,
                                      state:
                                          status?.cells.elementAtOrNull(
                                            id - 1,
                                          ) ??
                                          'unknown',
                                      onPressed: () => select(id),
                                      onLongPress:
                                          !widget.connected ||
                                              !widget.mappingCurrent ||
                                              !mapping.valid
                                          ? null
                                          : () => _editMapping(context, id),
                                    ),
                                  ),
                              ],
                            ),
                          ),
                        ),
                      );
                    },
                  ),
                ),
              ),
              // Command feedback never inserts content above the cell canvas.
              SizedBox(
                height: 48,
                child: Text(
                  batchMessage ?? '',
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                ),
              ),
            ],
          ),
        );

  Future<void> _editMapping(BuildContext context, int id) async {
    final physical = TextEditingController(
      text: '${mapping.physical.elementAtOrNull(id - 1) ?? id}',
    );
    final name = TextEditingController(
      text: mapping.names.elementAtOrNull(id - 1) ?? '$id',
    );
    final save = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text('Logical cell $id'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: name,
              maxLength: 15,
              decoration: const InputDecoration(labelText: 'Display name'),
            ),
            TextField(
              controller: physical,
              keyboardType: TextInputType.number,
              decoration: const InputDecoration(
                labelText: 'Physical chain cell',
              ),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Save'),
          ),
        ],
      ),
    );
    if (save == true) {
      await command('/api/cells/mapping', {
        'logical_id': '$id',
        'physical_cell': physical.text,
        'name': name.text,
      });
    }
    physical.dispose();
    name.dispose();
  }
}

class CellButton extends StatefulWidget {
  const CellButton({
    super.key,
    required this.id,
    required this.name,
    required this.physical,
    required this.state,
    required this.onPressed,
    required this.onLongPress,
    this.highlighted = false,
    this.showIndicator = false,
  });
  final int id;
  final int physical;
  final String name;
  final String state;
  final VoidCallback? onPressed;
  final VoidCallback? onLongPress;
  final bool highlighted;
  final bool showIndicator;

  @override
  State<CellButton> createState() => _CellButtonState();
}

class _CellButtonState extends State<CellButton> {
  bool hovered = false, focused = false;

  @override
  Widget build(BuildContext context) {
    // Adapt cell_surface's tint-free inside outline, independent of LED state.
    final background = hovered && !widget.highlighted
        ? const Color(0xff222b38)
        : tableCellPosition(widget.id).column.isEven
        ? const Color(0xff121820)
        : const Color(0xff1b2330);
    final outline = widget.highlighted
        ? const Color(0xffffc75b)
        : focused
        ? studioDark.primary
        : hovered
        ? const Color(0xff66758c)
        : studioDark.outline;
    return Tooltip(
      message:
          'Logical ${widget.id} · physical ${widget.physical > 0 ? widget.physical : 'unknown'} · ${widget.state}',
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 140),
        decoration: BoxDecoration(
          color: background,
          borderRadius: BorderRadius.circular(9),
          border: Border.all(color: outline, width: 2),
        ),
        child: Material(
          color: Colors.transparent,
          child: InkWell(
            onTap: widget.onPressed,
            onLongPress: widget.onLongPress,
            onHover: (value) => setState(() => hovered = value),
            onFocusChange: (value) => setState(() => focused = value),
            overlayColor: const WidgetStatePropertyAll(Colors.transparent),
            borderRadius: BorderRadius.circular(9),
            child: Stack(
              children: [
                Center(
                  child: FittedBox(
                    child: Text(
                      widget.name,
                      style: TextStyle(
                        color: studioDark.onSurface,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ),
                ),
                if (widget.showIndicator)
                  Positioned(
                    right: 4,
                    bottom: 4,
                    child: Container(
                      width: 5,
                      height: 5,
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        color: widget.state == 'white'
                            ? Colors.white
                            : widget.state == 'red'
                            ? studioDark.error
                            : studioDark.outline,
                      ),
                    ),
                  ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class FrontPanelPage extends StatefulWidget {
  const FrontPanelPage({
    super.key,
    required this.status,
    required this.command,
    this.mirror,
  });
  final DeviceStatus? status;
  final Command command;
  final PeripheralMirror? mirror;
  @override
  State<FrontPanelPage> createState() => _FrontPanelPageState();
}

class _FrontPanelPageState extends State<FrontPanelPage> {
  final text = TextEditingController(text: 'PAKAR CONTROLLER');
  double speed = 260;
  String repeat = 'once';
  bool forceScroll = false;
  String? selectedCue;
  bool audioPending = false;

  Future<void> sendCue(String cue) async {
    if (audioPending || widget.status?.audioCues.contains(cue) != true) return;
    setState(() => audioPending = true);
    try {
      await widget.command('/api/audio', {'cue': cue});
    } finally {
      if (mounted) setState(() => audioPending = false);
    }
  }

  @override
  void dispose() {
    text.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => widget.mirror == null
      ? _contents(context)
      : AnimatedBuilder(
          animation: widget.mirror!,
          builder: (_, _) => _contents(context),
        );

  Widget _contents(BuildContext context) => ListView(
    padding: const EdgeInsets.all(16),
    children: [
      const Text(
        'Front panel',
        style: TextStyle(fontSize: 24, fontWeight: FontWeight.w700),
      ),
      const SizedBox(height: 12),
      if (widget.mirror != null) _mirrorControls(widget.mirror!),
      if (widget.mirror?.panelEnabled == true && widget.mirror!.panelAvailable)
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'Live front panel',
                  style: TextStyle(fontWeight: FontWeight.w700),
                ),
                Text(
                  'Menu page ${widget.mirror!.panel['menu_page']} · '
                  '${widget.mirror!.panel['menu_active'] == true ? 'Menu open' : 'Menu closed'} · '
                  '${widget.mirror!.panel['running'] == true ? 'Running' : 'Stopped'}',
                ),
                Text(
                  widget.mirror!.panel['captured'] == true
                      ? 'Host owns key capture'
                      : 'Board owns keys',
                ),
                const SizedBox(height: 12),
                if (widget.status?.peer.has('inputs') == true)
                  Wrap(
                    spacing: 8,
                    runSpacing: 8,
                    children: [
                      for (var i = 0; i < 9; i++)
                        Chip(
                          avatar: Icon(
                            i == 8
                                ? (widget.status?.json['input_9'] == true
                                      ? Icons.radio_button_checked
                                      : Icons.radio_button_unchecked)
                                : ((widget.status!.activeInputs & (1 << i)) != 0
                                      ? Icons.radio_button_checked
                                      : Icons.radio_button_unchecked),
                            size: 16,
                          ),
                          label: Text('Key ${i + 1}'),
                        ),
                    ],
                  ),
                if (widget.status?.peer.has('outputs') == true)
                  Wrap(
                    spacing: 8,
                    children: [
                      for (var i = 0; i < 8; i++)
                        Chip(
                          avatar: Icon(
                            Icons.circle,
                            size: 12,
                            color:
                                (widget.status!.activeOutputs & (1 << i)) != 0
                                ? Theme.of(context).colorScheme.secondary
                                : Theme.of(context).colorScheme.outline,
                          ),
                          label: Text('LED ${i + 1}'),
                        ),
                    ],
                  ),
              ],
            ),
          ),
        ),
      if (widget.status?.lcdLines.isNotEmpty == true)
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'LCD display',
                  style: Theme.of(context).textTheme.titleMedium,
                ),
                const SizedBox(height: 8),
                for (final line in widget.status!.lcdLines)
                  Text(
                    line,
                    textDirection: TextDirection.ltr,
                    style: const TextStyle(fontFamily: 'monospace'),
                  ),
              ],
            ),
          ),
        ),
      if (widget.status?.peer.has('display') == true &&
          widget.mirror?.segmentsEnabled != false)
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child:
                (widget.mirror?.segments ?? widget.status!.segments).length == 4
                ? SevenSegmentPreview(
                    segments:
                        widget.mirror?.segments ?? widget.status!.segments,
                    brightness:
                        widget.mirror?.brightness ??
                        widget.status!.segmentBrightness,
                    active: true,
                  )
                : const Text('Display state unavailable'),
          ),
        ),
      if (widget.status?.peer.has('text') == true)
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              children: [
                TextField(
                  controller: text,
                  maxLength: 40,
                  decoration: const InputDecoration(labelText: 'Marquee text'),
                ),
                DropdownButtonFormField<String>(
                  initialValue: repeat,
                  decoration: const InputDecoration(labelText: 'Repeat'),
                  items: const [
                    DropdownMenuItem(value: 'once', child: Text('Once')),
                    DropdownMenuItem(value: 'loop', child: Text('Loop')),
                    DropdownMenuItem(
                      value: 'interval',
                      child: Text('Interval (5 seconds)'),
                    ),
                  ],
                  onChanged: (value) =>
                      setState(() => repeat = value ?? 'once'),
                ),
                CheckboxListTile(
                  title: const Text('Scroll short text'),
                  value: forceScroll,
                  onChanged: (value) =>
                      setState(() => forceScroll = value ?? false),
                ),
                Row(
                  children: [
                    Expanded(
                      child: Slider(
                        value: speed,
                        min: 80,
                        max: 1000,
                        divisions: 46,
                        label: '${speed.round()} ms',
                        onChanged: (value) => setState(() => speed = value),
                      ),
                    ),
                    FilledButton.icon(
                      onPressed: widget.status == null
                          ? null
                          : () => widget.command('/api/display', {
                              'text': text.text,
                              'speed_ms': '${speed.round()}',
                              'hold_ms': '1500',
                              'repeat': repeat,
                              'force_scroll': '$forceScroll',
                              'interval_seconds': '5',
                            }),
                      icon: const Icon(PakarIcons.play),
                      label: const Text('Present'),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      const SizedBox(height: 12),
      if (widget.status?.audioCues.isNotEmpty == true) _melodyControls(),
    ],
  );

  Widget _melodyControls() {
    final cues = widget.status!.audioCues;
    final cue = cues.contains(selectedCue) ? selectedCue! : cues.first;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Board melody'),
            DropdownButtonFormField<String>(
              key: ValueKey('audio-cues:${cues.join(',')}:$cue'),
              initialValue: cue,
              decoration: const InputDecoration(
                labelText: 'Acknowledged AVR cue',
              ),
              items: [
                for (final item in cues)
                  DropdownMenuItem(value: item, child: Text(item)),
              ],
              onChanged: audioPending
                  ? null
                  : (value) => setState(() => selectedCue = value),
            ),
            const SizedBox(height: 12),
            FilledButton.icon(
              onPressed: audioPending ? null : () => sendCue(cue),
              icon: const Icon(PakarIcons.musicNote),
              label: Text(audioPending ? 'Sending cue…' : 'Send melody'),
            ),
          ],
        ),
      ),
    );
  }

  Widget _mirrorControls(PeripheralMirror mirror) => Card(
    child: Padding(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Padding(
            padding: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
            child: Text(
              'Mirror on this phone',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.w700),
            ),
          ),
          if (mirror.panelAvailable)
            SwitchListTile.adaptive(
              title: const Text('Front panel'),
              subtitle: const Text('Live menu, ownership and physical keys'),
              value: mirror.panelEnabled,
              onChanged: mirror.setPanel,
            ),
          if (mirror.segmentsAvailable)
            SwitchListTile.adaptive(
              title: const Text('7-segment display'),
              subtitle: const Text(
                'Actual segment masks, decimal points and brightness',
              ),
              value: mirror.segmentsEnabled,
              onChanged: mirror.setSegments,
            ),
          if (mirror.buzzerAvailable) ...[
            SwitchListTile.adaptive(
              title: const Text('Exact-tone buzzer'),
              subtitle: const Text(
                'Phone speaker · live AVR frequency and cadence · foreground only',
              ),
              value: mirror.buzzerEnabled,
              onChanged: mirror.setBuzzer,
            ),
            SwitchListTile.adaptive(
              title: const Text('Follow board Silent mode'),
              subtitle: const Text(
                'Otherwise phone audio remains independent of the physical buzzer',
              ),
              value: mirror.followBoardMute,
              onChanged: mirror.setFollowBoardMute,
            ),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16),
              child: Row(
                children: [
                  const Text('Phone volume'),
                  Expanded(
                    child: Slider(
                      value: mirror.volume,
                      label: '${(mirror.volume * 100).round()}%',
                      divisions: 20,
                      onChanged: mirror.setVolume,
                    ),
                  ),
                  Text('${(mirror.volume * 100).round()}%'),
                ],
              ),
            ),
            if (mirror.buzzerEnabled)
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: 16),
                child: TextButton.icon(
                  onPressed: () => mirror.setBuzzer(false),
                  icon: const Icon(Icons.volume_off_outlined),
                  label: const Text('Stop phone audio'),
                ),
              ),
          ] else if (widget.status?.peer.has('buzzer') == true)
            const Padding(
              padding: EdgeInsets.all(16),
              child: Text(
                'Exact-tone mirroring awaits the AVR timestamp capability acknowledgment.',
              ),
            ),
          if (mirror.failure != null)
            Padding(
              padding: const EdgeInsets.all(16),
              child: Text(
                mirror.failure!,
                style: TextStyle(color: Theme.of(context).colorScheme.error),
              ),
            ),
        ],
      ),
    ),
  );
}

class SevenSegmentPreview extends StatelessWidget {
  const SevenSegmentPreview({
    super.key,
    required this.segments,
    required this.brightness,
    required this.active,
  });
  final List<int> segments;
  final int brightness;
  final bool active;

  @override
  Widget build(BuildContext context) => Semantics(
    label: 'Live TM1637 preview',
    child: SizedBox(
      height: 120,
      child: CustomPaint(
        painter: SegmentPainter(segments, brightness, active),
        child: const SizedBox.expand(),
      ),
    ),
  );
}

class SegmentPainter extends CustomPainter {
  SegmentPainter(this.masks, this.brightness, this.active);
  final List<int> masks;
  final int brightness;
  final bool active;
  static const lines = [
    [8.0, 5.0, 28.0, 5.0],
    [31.0, 8.0, 31.0, 27.0],
    [31.0, 32.0, 31.0, 51.0],
    [8.0, 54.0, 28.0, 54.0],
    [5.0, 32.0, 5.0, 51.0],
    [5.0, 8.0, 5.0, 27.0],
    [8.0, 29.5, 28.0, 29.5],
  ];

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRRect(
      RRect.fromRectAndRadius(Offset.zero & size, const Radius.circular(12)),
      Paint()..color = const Color(0xff130c0d),
    );
    final scale = (size.height - 24) / 60;
    final width = 36 * scale;
    final startX = (size.width - width * 4) / 2;
    final alpha = active ? (0.2 + 0.8 * brightness.clamp(0, 7) / 7) : .08;
    for (var digit = 0; digit < 4; digit++) {
      final mask = masks.elementAtOrNull(digit) ?? 0;
      for (var bit = 0; bit < 7; bit++) {
        final line = lines[bit];
        canvas.drawLine(
          Offset(startX + (digit * 36 + line[0]) * scale, 12 + line[1] * scale),
          Offset(startX + (digit * 36 + line[2]) * scale, 12 + line[3] * scale),
          Paint()
            ..color = (mask & (1 << bit)) != 0
                ? Color.fromRGBO(255, 74, 58, alpha)
                : const Color(0xff4d2023)
            ..strokeWidth = 4 * scale
            ..strokeCap = StrokeCap.round,
        );
      }
      canvas.drawCircle(
        Offset(startX + (digit * 36 + 34) * scale, 12 + 54 * scale),
        2 * scale,
        Paint()
          ..color = (mask & 0x80) != 0
              ? Color.fromRGBO(255, 74, 58, alpha)
              : const Color(0xff4d2023),
      );
    }
  }

  @override
  bool shouldRepaint(covariant SegmentPainter oldDelegate) =>
      oldDelegate.masks != masks ||
      oldDelegate.brightness != brightness ||
      oldDelegate.active != active;
}

class HardwarePage extends StatelessWidget {
  const HardwarePage({super.key, required this.status});
  final DeviceStatus? status;

  @override
  Widget build(BuildContext context) {
    String fact(String key, String yes, String no) =>
        status?.json.containsKey(key) == true
        ? status!.json[key] == true
              ? yes
              : no
        : 'Unavailable';
    String mask(String key) => status?.json[key] is int
        ? '0x${(status!.json[key] as int).toRadixString(16).padLeft(2, '0')}'
        : 'Unavailable';
    final values = [
      if (status?.peer.known == true &&
          status?.json.containsKey('avr_online') == true)
        ('AVR', fact('avr_online', 'Online', 'Offline'), PakarIcons.cpu),
      if (status?.peer.has('display') == true)
        ('TM1637', fact('tm1637_ack', 'Ready', 'No ACK'), PakarIcons.gauge),
      if (status?.peer.has('network') == true) ...[
        (
          'mDNS',
          fact('mdns_started', 'Started', 'Stopped'),
          PakarIcons.broadcast,
        ),
        ('SSDP', fact('ssdp_started', 'Started', 'Stopped'), PakarIcons.radio),
        (
          'MQTT',
          fact('mqtt_connected', 'Connected', 'Offline'),
          PakarIcons.cloud,
        ),
        (
          'NTP',
          fact('ntp_synced', 'Synchronized', 'Not synchronized'),
          PakarIcons.clock,
        ),
      ],
      if (status?.peer.has('inputs') == true)
        ('Inputs', mask('active_inputs'), PakarIcons.keyboard),
      if (status?.peer.has('outputs') == true)
        ('Outputs', mask('active_outputs'), PakarIcons.lightning),
    ];
    if (values.isEmpty) {
      return const Center(child: Text('No acknowledged hardware measurements'));
    }
    return GridView.extent(
      padding: const EdgeInsets.all(16),
      maxCrossAxisExtent: 280,
      childAspectRatio: 1.8,
      children: [
        for (final value in values)
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Row(
                children: [
                  Icon(value.$3, size: 30),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Text(
                          value.$1,
                          style: const TextStyle(color: Color(0xff909baa)),
                        ),
                        Text(
                          value.$2,
                          maxLines: 2,
                          overflow: TextOverflow.ellipsis,
                          style: const TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          ),
      ],
    );
  }
}

class SettingsPage extends StatefulWidget {
  const SettingsPage({
    super.key,
    required this.api,
    required this.reconnect,
    this.active = true,
  });
  final PakarApi api;
  final Future<void> Function() reconnect;
  final bool active;
  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  late final endpoint = TextEditingController(text: widget.api.baseUrl);
  late final user = TextEditingController(text: widget.api.user);
  late final password = TextEditingController(text: widget.api.password);
  bool saving = false;
  String? error;

  @override
  void dispose() {
    endpoint.dispose();
    user.dispose();
    password.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => WorkspaceBody(
    children: [
      const WorkspaceHeader(
        title: 'Preferences',
        icon: PakarIcons.gear,
        subtitle: 'Application connection',
      ),
      BoardSoundPreferences(api: widget.api, active: widget.active),
      WorkspaceSection(
        title: 'Network connection',
        icon: PakarIcons.link,
        children: [
          TextField(
            controller: endpoint,
            decoration: const InputDecoration(
              labelText: 'Device hostname or URL',
            ),
          ),
          const SizedBox(height: 10),
          TextField(
            controller: user,
            decoration: const InputDecoration(labelText: 'User'),
          ),
          const SizedBox(height: 10),
          TextField(
            controller: password,
            obscureText: true,
            decoration: const InputDecoration(labelText: 'Password (optional)'),
          ),
          const SizedBox(height: 12),
          if (error != null)
            Text(
              error!,
              style: TextStyle(color: Theme.of(context).colorScheme.error),
            ),
          Align(
            alignment: Alignment.centerRight,
            child: FilledButton.icon(
              onPressed: saving
                  ? null
                  : () async {
                      setState(() {
                        saving = true;
                        error = null;
                      });
                      try {
                        final prefs = await SharedPreferences.getInstance();
                        final settings = ConnectionPreferences(prefs);
                        await settings.save(
                          endpoint.text,
                          user.text,
                          password.text,
                        );
                        await settings.restore(widget.api);
                        await widget.reconnect();
                      } on Exception catch (failure) {
                        if (mounted) {
                          setState(() => error = failure.toString());
                        }
                      } finally {
                        if (mounted) setState(() => saving = false);
                      }
                    },
              icon: const Icon(PakarIcons.floppyDisk),
              label: const Text('Save and reconnect'),
            ),
          ),
        ],
      ),
    ],
  );
}

class StatusPill extends StatelessWidget {
  const StatusPill({super.key, required this.ok, required this.label});
  final bool ok;
  final String label;
  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(horizontal: 6),
    child: Chip(
      avatar: Icon(
        PakarIcons.circle,
        size: 12,
        color: ok ? const Color(0xff38d27a) : const Color(0xffff5c68),
      ),
      label: Text(label),
    ),
  );
}

class StatusBar extends StatelessWidget {
  const StatusBar({super.key, required this.status, required this.message});
  final DeviceStatus? status;
  final String message;
  @override
  Widget build(BuildContext context) => Container(
    decoration: const BoxDecoration(
      color: Color(0xff0d1016),
      border: Border(top: BorderSide(color: Color(0xff29313d))),
    ),
    padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
    child: Row(
      children: [
        Icon(
          status?.peer.known == true ? PakarIcons.check : PakarIcons.warning,
          size: 16,
        ),
        const SizedBox(width: 8),
        Expanded(
          child: Text(message, maxLines: 1, overflow: TextOverflow.ellipsis),
        ),
        Flexible(
          child: Text(
            status == null
                ? 'State unavailable'
                : [
                    if (status!.hostname.isNotEmpty) status!.hostname,
                    if (status!.json['station_connected'] == true &&
                        status!.stationIp.isNotEmpty)
                      status!.stationIp,
                    if (status!.json['station_connected'] == true &&
                        status!.rssi != null)
                      '${status!.rssi} dBm',
                  ].join(' · '),
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
          ),
        ),
      ],
    ),
  );
}
