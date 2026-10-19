import 'dart:async';
import 'dart:convert';
import 'dart:io';

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
import 'automatic_board_connection.dart';
import 'discovery.dart';
import 'device_registry.dart';
import 'cell_search.dart';
import 'cell_assignments_actions.dart';
import 'peer_context.dart';
import 'panel_inventory.dart';
import 'output_panel.dart';
import 'workspace_activity_strip.dart';
import 'application_artwork.dart';
import 'device_settings_page.dart';
import 'devices_page.dart';
import 'bridge_page.dart';
import 'firmware_page.dart';
import 'firmware_progress.dart';
import 'firmware_automation.dart';
import 'app_update_coordinator.dart';
import 'update_center.dart';
import 'app_support.dart';
import 'presentation_assets.dart';
import 'pixel_diagnostic.dart';
import 'cell_pixel_preview.dart';
import 'pixel_diagnostic_page.dart';
import 'mirroring.dart';
import 'app_theme.dart';
import 'workspace_widgets.dart';
import 'peer_identity.dart';
import 'peer_policy.dart';
export 'app_theme.dart' show studioDark;

final bundledApplicationArtwork = base64Decode(applicationArtworkBase64);

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await PeerIdentity.current.initialize();
  await presentationAssets.load();
  final view = WidgetsBinding.instance.platformDispatcher.views.firstOrNull;
  if (view != null) {
    PeerIdentity.current.hardware.addAll({
      'screen_width': view.physicalSize.width.round(),
      'screen_height': view.physicalSize.height.round(),
    });
  }
  runApp(const PakarControllerApp());
}

class PakarControllerApp extends StatelessWidget {
  const PakarControllerApp({super.key, this.api});
  final PakarApi? api;

  @override
  Widget build(BuildContext context) =>
      ValueListenableBuilder<Map<String, int>>(
        valueListenable: presentationAssets,
        builder: (context, palette, _) => MaterialApp(
          title: 'PakarController',
          debugShowCheckedModeBanner: false,
          themeMode: ThemeMode.dark,
          darkTheme: studioTheme(palette: palette),
          home: ControlShell(api: api),
        ),
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
  const ControlShell({super.key, this.api, this.discovery});
  final PakarApi? api;
  final DeviceDiscovery? discovery;
  @override
  State<ControlShell> createState() => _ControlShellState();
}

class _ControlShellState extends State<ControlShell>
    with WidgetsBindingObserver {
  late final api = widget.api ?? PakarApi();
  late final discovery =
      widget.discovery ?? DeviceDiscovery(catalog: DiscoveryCatalog());
  late final automatic = AutomaticBoardConnector(api, discovery);
  bool automaticSelecting = false;
  DateTime? lastAutomaticAttempt;
  bool restoredWanted = true;
  String? lastSavedNetworkRoute;
  final mirror = PeripheralMirror();
  late final AppUpdateCoordinator appUpdates = AppUpdateCoordinator(
    approve: _approveAppUpdate,
    supportInfo: () => {
      'enabled': support.running,
      if (support.running) 'port': support.port,
    },
  );
  late final AppSupportSession support = AppSupportSession(
    api: () => api,
    observed: () =>
        commandPending ||
            manualMaintenanceBusy ||
            firmwareAutomation?.busy == true
        ? null
        : status,
    applyAssets: presentationAssets.applyText,
    health: () => {
      'connected': status != null,
      'websocket_live': api.liveFresh,
      'busy': commandPending || manualMaintenanceBusy,
      'updating': appUpdates.updates.busy,
      'selection_count': workspaces[workspaceBoard]?.selection.length ?? 0,
    },
  );
  FirmwareAutomation? firmwareAutomation;
  bool manualMaintenanceBusy = false;
  DeviceStatus? status;
  DeviceStatus? presented;
  final panelInventory = PanelInventory();
  final workspaces = <String, TableWorkspace>{};
  final connectionDraft = ConnectionDraft();
  String? workspaceBoard;
  WorkspaceCacheWriter? workspaceWriter;
  Timer? workspaceSave, workspaceSaveLimit;
  (bool, bool, bool, double)? mirrorChoices;
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
  bool commandPending = false;
  int commandAttempt = 0;
  String? commandFailure;
  String page = 'overview';
  String message = 'Connecting to ledboard.local';

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    unawaited(appUpdates.load());
    unawaited(_restoreFirmwareAutomation());
    mirror.addListener(_mirrorPreferencesChanged);
    connectionDraft.addListener(_scheduleWorkspaceSave);
    api.onPeripheralEvent = mirror.event;
    api.onPeripheralStreamLost = () =>
        mirror.observe(null, api.connectionRevision);
    api.onTransportRetired = () {
      mirror.observe(null, api.connectionRevision);
      if (mounted) {
        _revoke(
          api.unsupportedPeer?.toString() ??
              'Connection changed; waiting for fresh device state',
        );
        if (observedEndpoint != api.baseUrl) {
          setState(() {
            presented = null;
            panelInventory.clear();
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
    api.onConfigurationChanged = () => unawaited(_refresh(silent: true));
    _restore();
  }

  Future<bool> _approveAppUpdate(Map<String, dynamic> manifest) async {
    if (!mounted || !foreground) return false;
    return await showDialog<bool>(
          context: context,
          builder: (dialog) => AlertDialog(
            title: const Text('Receive app update?'),
            content: Text(
              'Build ${manifest['version_code']} · ${manifest['bytes']} bytes',
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(dialog, false),
                child: const Text('Decline'),
              ),
              FilledButton(
                onPressed: () => Navigator.pop(dialog, true),
                child: const Text('Receive'),
              ),
            ],
          ),
        ) ??
        false;
  }

  Future<void> _restoreFirmwareAutomation() async {
    // Maintenance client delivery must not start a saved board update job
    // while this installation is serving an active production board.
    if (const bool.fromEnvironment('PAKAR_BOARD_PRODUCTION_HOLD')) return;
    final prefs = await SharedPreferences.getInstance();
    if (!mounted) return;
    Map<String, dynamic>? initial;
    try {
      final raw = jsonDecode(
        prefs.getString('pakar.firmware_automation') ?? '{}',
      );
      if (raw is Map<String, dynamic>) initial = raw;
    } on Object {
      /* A corrupt policy never authorizes a firmware update. */
    }
    final service = FirmwareAutomation(
      api,
      initial: initial,
      authorized: (feature) =>
          mounted &&
          !manualMaintenanceBusy &&
          api.connectionWanted &&
          api.latestStatus?.peer.has(feature) == true,
      save: (value) async {
        if (!await prefs.setString(
          'pakar.firmware_automation',
          jsonEncode(value),
        )) {
          throw StateError('Firmware preferences could not be saved');
        }
      },
    );
    setState(() => firmwareAutomation = service);
    await service.setForeground(foreground);
  }

  Future<void> _restore() async {
    final prefs = await SharedPreferences.getInstance();
    if (!mounted) return;
    final saved = prefs.getString('table_workspaces');
    if (saved != null && saved.length < 1048576) {
      try {
        final cache = jsonDecode(saved);
        final entries = cache is Map ? cache['boards'] : null;
        if (cache is Map) {
          connectionDraft.restore(cache['connection_draft']);
          final connection = cache['connection'];
          if (connection is Map) {
            if (connection['method'] == 'usb') {
              api.connectionMethod = ConnectionMethod.usb;
            }
            if (connection['method'] == 'network') {
              api.connectionMethod = ConnectionMethod.network;
            }
            if (connection['method'] == 'bluetooth') {
              api.connectionMethod = ConnectionMethod.bluetooth;
            }
            restoredWanted = connection['wanted'] != false;
          }
          final phone = cache['phone'];
          if (phone is Map) {
            mirror.panelEnabled = phone['front_panel'] != false;
            mirror.segmentsEnabled = phone['segments'] != false;
            mirror.followBoardMute = phone['follow_mute'] == true;
            final volume = phone['volume'];
            if (volume is num &&
                volume.isFinite &&
                volume >= 0 &&
                volume <= 1) {
              mirror.volume = volume.toDouble();
            }
            // PCM sessions/notes are never restored or started by a UI cache.
          }
        }
        if (cache is Map &&
            [
              'overview',
              'table',
              'output',
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
              workspaces[entry.key] = _ownWorkspace(
                TableWorkspace(saved: entry.value as Map<String, dynamic>),
              );
            }
          }
        }
      } on FormatException {
        /* Ignore invalid UI cache, never device settings. */
      }
    }
    if (!mounted) return;
    workspaceWriter = WorkspaceCacheWriter(
      saved: saved ?? '',
      write: (encoded) => prefs.setString('table_workspaces', encoded),
    );
    try {
      await ConnectionPreferences(prefs).restore(api);
      // Explicit Disconnect remains latched in this live session. A previous
      // session's incidental disconnected state does not disable startup auto.
      if (!restoredWanted && !api.automaticDiscovery) api.disconnect();
    } on Exception catch (error) {
      if (mounted) {
        setState(() => message = 'Preferences could not be restored: $error');
      }
      return;
    }
    if (!mounted) return;
    if (api.connectionWanted) {
      await _startConnection();
    } else {
      setState(() => message = 'Disconnected');
    }
  }

  Future<void> _startConnection() async {
    _startSupervision();
    unawaited(_connectLive());
    await _refresh();
    if (mounted && api.connectionWanted && foreground && !liveActive) {
      unawaited(_connectLive());
    }
  }

  void _startSupervision() {
    fallback?.cancel();
    fallback = Timer.periodic(const Duration(seconds: 3), (_) {
      if (!foreground || !api.connectionWanted || api.unsupportedPeer != null) {
        return;
      }
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
    if (!mounted ||
        !api.connectionWanted ||
        !foreground ||
        api.unsupportedPeer != null) {
      return;
    }
    final owner = api.connectionRevision;
    // Adapt donor StatusFrameBatcher's generation retirement: an old socket's
    // error/done callbacks cannot revoke or schedule work for its replacement.
    final generation = ++liveGeneration;
    liveActive = true;
    liveRetry?.cancel();
    final previous = live;
    live = null;
    // Closing the underlying socket first breaks the async-generator stream
    // cancellation handshake. A dead peer must not stall a replacement read.
    await api.retireLiveSocket();
    try {
      await previous?.cancel().timeout(const Duration(seconds: 1));
    } on TimeoutException {
      // The old generation has no socket or authority and cannot revoke us.
    }
    if (!mounted ||
        !api.connectionWanted ||
        !foreground ||
        owner != api.connectionRevision ||
        generation != liveGeneration) {
      if (generation == liveGeneration) liveActive = false;
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
      if (api.isBluetooth) {
        _revoke('Bluetooth connection lost');
      }
      // A failed WS does not invalidate a still-fresh authenticated HTTP
      // snapshot. Its own lease expires if the fallback read also fails.
      if (api.unsupportedPeer != null) {
        _revoke(api.unsupportedPeer.toString());
      } else if (foreground) {
        _retryLive();
      }
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
    if (!mounted ||
        !api.connectionWanted ||
        !foreground ||
        api.unsupportedPeer != null) {
      return;
    }
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
        if (!silent) message = 'Live device status received';
      });
    } catch (error) {
      if (mounted &&
          owner == api.connectionRevision &&
          started == observation) {
        if (error is UnsupportedPeerVersion ||
            error is AuthenticationRequired ||
            error is StaleConnection ||
            observedAge == null ||
            observedAge!.elapsed >= const Duration(seconds: 6)) {
          _revoke(error.toString());
        }
        if (error is SocketException || error is TimeoutException) {
          unawaited(_tryAutomaticConnection());
        }
      }
    } finally {
      if (refreshingRevision == owner) refreshingRevision = null;
    }
  }

  bool get automaticHandoffBlocked {
    // Automatic discovery never retires an editor/import/maintenance dialog.
    if (page == 'connection' ||
        page == 'device-settings' ||
        connectionDraft.endpoint != null ||
        connectionDraft.user != null ||
        Navigator.of(context).canPop()) {
      return true;
    }
    final owner = workspaceBoard ?? api.networkRecovery?['board_id'];
    final settings = workspaces[owner]?.settings.snapshot();
    return settings?['identity'] != null ||
        (settings?['edits'] is Map && (settings!['edits'] as Map).isNotEmpty);
  }

  Future<void> _tryAutomaticConnection({bool fromScan = false}) async {
    if (!mounted ||
        !foreground ||
        automaticSelecting ||
        !automatic.allowed ||
        automaticHandoffBlocked ||
        commandPending ||
        manualMaintenanceBusy ||
        firmwareAutomation?.busy == true) {
      return;
    }
    final now = DateTime.now();
    if (!fromScan &&
        lastAutomaticAttempt != null &&
        now.difference(lastAutomaticAttempt!) < const Duration(seconds: 30)) {
      return;
    }
    lastAutomaticAttempt = now;
    automaticSelecting = true;
    final owner = api.connectionRevision;
    try {
      final saved = await DeviceRegistry().load();
      if (!mounted ||
          owner != api.connectionRevision ||
          !automatic.allowed ||
          automaticHandoffBlocked) {
        return;
      }
      final choice = fromScan
          ? automatic.currentChoice()
          : await automatic.discover(
              seeds: saved
                  .where(
                    (d) =>
                        d.location == DeviceLocation.local &&
                        !d.endpoint.startsWith('ble://'),
                  )
                  .map((d) => d.identity),
            );
      if (!mounted ||
          choice == null ||
          owner != api.connectionRevision ||
          !automatic.allowed ||
          automaticHandoffBlocked) {
        return;
      }
      // Discovery never transfers credentials from the previous board. Only a
      // public full-status compatible board may be selected automatically.
      await _adoptConnection(
        owner,
        choice.endpoint,
        'admin',
        '',
        verifyCredentialChange: false,
        bluetoothBoard: choice.peer.boardId,
        networkOrigin: choice.route,
      );
    } on Object {
      // A missed/ambiguous sweep never revokes healthy UI or retries commands.
    } finally {
      automaticSelecting = false;
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
    final route = api.networkRecovery;
    final routeSignature = route == null ? null : jsonEncode(route);
    if (routeSignature != null && routeSignature != lastSavedNetworkRoute) {
      lastSavedNetworkRoute = routeSignature;
      unawaited(
        SharedPreferences.getInstance()
            .then(
              (prefs) => ConnectionPreferences(prefs).saveNetworkRecovery(api),
            )
            .catchError((Object _) {
              if (lastSavedNetworkRoute == routeSignature) {
                lastSavedNetworkRoute = null;
              }
            }),
      );
    }
    final board =
        value.json['board_id'] as String? ?? '${api.baseUrl}|${value.hostname}';
    final changed = workspaceBoard != board;
    if (changed && workspaces.length >= 16 && !workspaces.containsKey(board)) {
      final retired = workspaces.keys.firstWhere(
        (key) => key != workspaceBoard,
      );
      workspaces.remove(retired)?.dispose();
    }
    if (value.peer.known) {
      workspaces.putIfAbsent(board, () => _ownWorkspace(TableWorkspace()));
    }
    final bootChanged = presented?.json['boot_id'] != value.json['boot_id'];
    panelInventory.observe(value);
    workspaceBoard = board;
    observedAge = Stopwatch()..start();
    observedEndpoint = api.baseUrl;
    // An HTTP fallback read is not proof the suspended WebSocket recovered.
    if (api.liveFresh) resumeRead?.cancel();
    expiry?.cancel();
    ++observation;
    setState(() {
      status = value;
      presented = value;
      if (incident != null && !commandPending) {
        message = 'Live device status received';
      }
      incident = null;
      if (changed) {
        commandFailure = null;
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
        _revoke('Board updates timed out; reconnecting…');
      }
    });
  }

  TableWorkspace _ownWorkspace(TableWorkspace workspace) {
    workspace.addListener(_scheduleWorkspaceSave);
    return workspace;
  }

  void _navigate(String target) {
    setState(() => page = target);
    _scheduleWorkspaceSave();
  }

  void _mirrorPreferencesChanged() {
    final choices = (
      mirror.panelEnabled,
      mirror.segmentsEnabled,
      mirror.followBoardMute,
      mirror.volume,
    );
    if (choices == mirrorChoices) return;
    mirrorChoices = choices;
    _scheduleWorkspaceSave();
  }

  void _scheduleWorkspaceSave() {
    final activeWorkspace = workspaces[workspaceBoard];
    api.setUsbObservations(
      panel: page == 'front-panel' && mirror.panelEnabled,
      peripheral:
          page == 'front-panel' &&
          (mirror.segmentsEnabled || mirror.buzzerEnabled),
      pixels: page == 'table' && activeWorkspace?.showIndicators == true,
    );
    if (!mounted || workspaceWriter == null) return;
    if (!foreground) {
      _flushWorkspaceSave();
      return;
    }
    // No preferences write/JSON encoding per telemetry frame or scroll pixel.
    // Save after a short quiet period, or at most every 2s during a long drag.
    workspaceSave?.cancel();
    workspaceSave = Timer(
      const Duration(milliseconds: 350),
      _flushWorkspaceSave,
    );
    workspaceSaveLimit ??= Timer(
      const Duration(seconds: 2),
      _flushWorkspaceSave,
    );
  }

  void _flushWorkspaceSave() {
    workspaceSave?.cancel();
    workspaceSaveLimit?.cancel();
    workspaceSave = workspaceSaveLimit = null;
    unawaited(
      _saveWorkspaces().catchError((Object _) {
        // Keep the actual workspace on storage failure; retry on the next edit
        // or lifecycle flush. Never reset UI/device settings or replay a command.
      }),
    );
  }

  Future<void> _saveWorkspaces() async {
    final encoded = jsonEncode({
      'activity': page,
      'connection_draft': connectionDraft.snapshot(),
      'connection': {
        'method': api.connectionMethod.name,
        'wanted': api.connectionWanted,
      },
      'phone': {
        'front_panel': mirror.panelEnabled,
        'segments': mirror.segmentsEnabled,
        'follow_mute': mirror.followBoardMute,
        'volume': mirror.volume,
      },
      'boards': {
        for (final entry in workspaces.entries)
          entry.key: entry.value.snapshot(),
      },
    });
    await workspaceWriter?.save(encoded);
  }

  void _revoke(String reason) {
    if (status != null) support.recordIncident(SupportIncident.connectionLost);
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

  Future<void> _disconnect() async {
    // Retire before awaiting cancellation: no callback may reconnect afterward.
    ++liveGeneration;
    liveActive = false;
    commandFailure = null;
    api.disconnect();
    _flushWorkspaceSave();
    fallback?.cancel();
    liveRetry?.cancel();
    resumeRead?.cancel();
    _revoke('Disconnected');
    setState(() {
      presented = null;
      panelInventory.clear();
    });
    final previous = live;
    live = null;
    await api.retireLiveSocket();
    try {
      await previous?.cancel().timeout(const Duration(seconds: 1));
    } on TimeoutException {
      /* Explicit Disconnect already retired its owner. */
    }
  }

  Future<void> _reconnect() async {
    ++liveGeneration;
    api.connect();
    _scheduleWorkspaceSave();
    _revoke('Connecting to ${api.baseUrl}');
    liveRetry?.cancel();
    final previous = live;
    live = null;
    await api.retireLiveSocket();
    try {
      await previous?.cancel().timeout(const Duration(seconds: 1));
    } on TimeoutException {
      /* The previous generation cannot block Connect. */
    }
    if (mounted) await _startConnection();
  }

  Future<void> _adoptConnection(
    int owner,
    String endpoint,
    String user,
    String password, {
    bool verifyCredentialChange = true,
    String bluetoothBoard = '',
    String networkOrigin = '',
  }) async {
    if (!mounted || owner != api.connectionRevision) {
      throw const StaleConnection();
    }
    final changedLogin = user != api.user || password != api.password;
    // A acknowledged identity/credential operation may close the old socket
    // before its HTTP ACK arrives. Connection revision, not that dead socket,
    // owns this explicit handoff. Never apply it to a replacement connection.
    api.selectTransport(
      endpoint.startsWith('ble://')
          ? ConnectionMethod.bluetooth
          : endpoint.startsWith('usb://')
          ? ConnectionMethod.usb
          : ConnectionMethod.network,
      endpoint,
      board: bluetoothBoard,
    );
    api.user = user;
    api.password = password;
    if (networkOrigin.isNotEmpty && bluetoothBoard.isNotEmpty) {
      api.restoreNetworkRecovery({
        'endpoint': endpoint,
        'board_id': bluetoothBoard,
        'origin': networkOrigin,
      });
    }
    final adopted = api.connectionRevision;
    _revoke('Following acknowledged device configuration');
    try {
      final preferences = await SharedPreferences.getInstance();
      if (!mounted || adopted != api.connectionRevision) {
        throw const StaleConnection();
      }
      await ConnectionPreferences(preferences)
          .save(endpoint, user, password, bluetoothBoard: bluetoothBoard);
      if (!mounted || adopted != api.connectionRevision) {
        throw const StaleConnection();
      }
      if (changedLogin &&
          verifyCredentialChange &&
          api.connectionMethod == ConnectionMethod.network) {
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
    Map<String, String> values, {
    bool propagateFailure = false,
  }) async {
    final attempt = ++commandAttempt;
    if (commandPending) {
      setState(
        () => commandFailure =
            'Another command is still pending. This command was not sent.',
      );
      if (propagateFailure) {
        throw const FormatException(
          'Another command is still pending. This command was not sent.',
        );
      }
      return false;
    }
    setState(() {
      commandFailure = null;
      message = 'Sending command…';
    });
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
      if (attempt == commandAttempt) {
        setState(() {
          commandFailure = null;
          message = 'Device acknowledged; fresh state read back (not physical output proof)';
        });
      }
      return true;
    } catch (error) {
      if (mounted && owner == api.connectionRevision) {
        support.recordIncident(
          error is OperationUncertain
              ? SupportIncident.commandUncertain
              : SupportIncident.commandRejected,
        );
        if (attempt == commandAttempt) {
          setState(() => commandFailure = error.toString());
        }
        if (error is StaleConnection) {
          _revoke(error.toString());
        }
      }
      if (propagateFailure) rethrow;
      return false;
    } finally {
      commandPending = false;
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    final active = state == AppLifecycleState.resumed;
    api.appForeground = active;
    support.setForeground(active);
    presentationAssets.setForeground(active);
    mirror.foreground(active);
    unawaited(appUpdates.setForeground(active).catchError(appUpdates.failure));
    final automation = firmwareAutomation;
    if (automation != null) unawaited(automation.setForeground(active));
    if (foreground == active) return;
    foreground = active;
    if (!active) {
      fallback?.cancel();
      liveRetry?.cancel();
      expiry?.cancel();
      resumeRead?.cancel();
      _flushWorkspaceSave();
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
    // A suspended socket does not invalidate a still-fresh HTTP observation.
    // Its original deadline remains in force while an immediate read recovers.
    if (status != null &&
        observedAge != null &&
        observedAge!.elapsed < const Duration(seconds: 6)) {
      _armExpiry();
      setState(() => message = 'Checking the board connection…');
    } else {
      _revoke('Checking the board connection…');
      unawaited(_refresh(silent: true));
    }
    resumeProbeAge = Stopwatch()..start();
    final oldSocket = liveGeneration;
    final probing = liveActive && api.requestLiveStatus();
    if (!probing) {
      unawaited(_connectLive());
    }
    resumeRead = Timer(const Duration(milliseconds: 750), () {
      if (mounted && foreground && api.connectionWanted && !api.liveFresh) {
        // Replace only the unresponsive pre-suspend socket, not a replacement
        // already connecting or a healthy socket that answered the probe.
        if (probing && oldSocket == liveGeneration) {
          unawaited(_connectLive());
        }
        unawaited(_refresh(silent: true));
      }
    });
  }

  @override
  void dispose() {
    // Capture synchronously before child controllers/models are disposed; the
    // ordered writer may finish its small platform write after unmounting.
    _flushWorkspaceSave();
    WidgetsBinding.instance.removeObserver(this);
    api.onPeripheralEvent = null;
    api.onTransportRetired = null;
    mirror.removeListener(_mirrorPreferencesChanged);
    mirror.dispose();
    appUpdates.dispose();
    support.dispose();
    firmwareAutomation?.dispose();
    connectionDraft.dispose();
    api.onPeerChanged = null;
    api.onConfigurationChanged = null;
    live?.cancel();
    fallback?.cancel();
    liveRetry?.cancel();
    expiry?.cancel();
    resumeRead?.cancel();
    workspaceSave?.cancel();
    workspaceSaveLimit?.cancel();
    for (final workspace in workspaces.values) {
      workspace.dispose();
    }
    api.close();
    super.dispose();
  }

  Future<void> _openPixelDiagnostics() async {
    final board = status?.json['board_id'];
    if (!supportsPixelDiagnostic(status) ||
        status?.peer.has('command-ack') != true ||
        board is! String ||
        commandPending) {
      return;
    }
    var locked = false;
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (context) => StatefulBuilder(
          builder: (context, update) => PopScope(
            canPop: !locked,
            child: Scaffold(
              appBar: AppBar(
                title: const Text('Pixel diagnostics'),
                automaticallyImplyLeading: !locked,
              ),
              body: PixelDiagnosticPage(
                api: api,
                observed: () => status,
                onObserved: _observe,
                mapping: () => mappingFresh ? mapping : null,
                workspace: workspaces[board]?.pixels,
                busyChanged: (value) {
                  commandPending = value;
                  if (context.mounted) update(() => locked = value);
                },
              ),
            ),
          ),
        ),
      ),
    );
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
      if (panelInventory.tableSelection)
        ('table', 'Table', PakarIcons.gridFour),
      if (panelInventory.has('table')) ('output', 'Output', PakarIcons.gauge),
      if (panelInventory.frontPanel)
        ('front-panel', 'Front panel', PakarIcons.gauge),
      if (panelInventory.known &&
          (panelInventory.features.isNotEmpty || panelInventory.avrObserved))
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
              key: ValueKey('table:${panelInventory.ownerKey}'),
              status: presented,
              connected: status?.peer.tableSelection == true && foreground,
              seenFeatures: panelInventory.features,
              layoutAcknowledged: panelInventory.has('table'),
              mappingCurrent: mappingFresh,
              workspace: workspaces[workspaceBoard],
              mapping: mapping,
              command: scoped,
              verifiedCommand: (path, values) => _command(
                owner,
                hostname,
                path,
                values,
                propagateFailure: true,
              ),
              readStatus: () async {
                final observed = await api.status();
                if (!mounted ||
                    owner != api.connectionRevision ||
                    observed.hostname != presented?.hostname ||
                    !api.connectionWanted) {
                  throw const StaleConnection();
                }
                _observe(observed);
                return observed;
              },
              ownerCurrent: () =>
                  mounted &&
                  api.connectionWanted &&
                  owner == api.connectionRevision,
            ),
            'output' => OutputPanel(
              key: ValueKey('output:${panelInventory.ownerKey}'),
              status: presented,
              connected: status != null && foreground,
              workspace: workspaces[workspaceBoard],
              command: scoped,
              openDiagnostics: () => _openPixelDiagnostics(),
            ),
            'front-panel' => IgnorePointer(
              key: ValueKey('front:${panelInventory.ownerKey}'),
              ignoring: status == null || !foreground,
              child: FrontPanelPage(
                status: presented,
                seenFeatures: panelInventory.features,
                connected: status != null && foreground,
                command: scoped,
                mirror: mirror,
                workspace: workspaces[workspaceBoard]?.frontPanel,
              ),
            ),
            'hardware' => HardwarePage(
              key: ValueKey('hardware:$workspaceBoard'),
              status: status,
              inventory: panelInventory,
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
                navigate: _navigate,
                adoptConnection: (endpoint, user, password) =>
                    _adoptConnection(owner, endpoint, user, password),
                workspace: workspaces[workspaceBoard]?.settings,
              ),
            ),
            'connection' => SettingsPage(
              key: const ValueKey('connection-preferences'),
              api: api,
              reconnect: _reconnect,
              active: page == 'connection' && foreground,
              draft: connectionDraft,
              openDevices: () => _navigate('devices'),
            ),
            'devices' =>
              page != 'devices'
                  ? const SizedBox.shrink(key: ValueKey('devices-hidden'))
                  : DevicesPage(
                      api: api,
                      discovery: discovery,
                      onDiscovered: () =>
                          unawaited(_tryAutomaticConnection(fromScan: true)),
                      select: (device, password) async {
                        await _adoptConnection(
                          api.connectionRevision,
                          device.endpoint,
                          device.user,
                          password,
                          verifyCredentialChange: false,
                          bluetoothBoard: device.boardId,
                        );
                        if (mounted) _navigate('overview');
                      },
                    ),
            _ => BoardOverview(
              key: ValueKey('overview:$workspaceBoard'),
              status: presented,
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
                  _navigate('devices');
                  return;
                }
                if (target == 'updates') {
                  final updateBoard = status?.json['board_id'];
                  await Navigator.of(context).push(
                    MaterialPageRoute<void>(
                      builder: (_) => UpdateCenter(
                        api: api,
                        apps: appUpdates,
                        support: support,
                        automation: firmwareAutomation,
                        boardProductionHold: const bool.fromEnvironment(
                          'PAKAR_BOARD_PRODUCTION_HOLD',
                        ),
                        boardAvailable: peer.has('isp') || peer.has('ota'),
                        authorized: (feature) =>
                            mounted &&
                            firmwareAutomation?.busy != true &&
                            status?.json['board_id'] == updateBoard &&
                            status?.hostname == hostname &&
                            status?.peer.has(feature) == true,
                        busyChanged: (value) => manualMaintenanceBusy = value,
                      ),
                    ),
                  );
                  return;
                }
                if (target == 'pixels') {
                  await _openPixelDiagnostics();
                  return;
                }
                var locked = false;
                final pixelBoard = status?.json['board_id'];
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
                                  : 'Firmware',
                            ),
                            automaticallyImplyLeading: !locked,
                          ),
                          body: target == 'bridge'
                              ? BridgePage(api: api, observed: () => status)
                              : FirmwarePage(
                                  api: api,
                                  authorized: (feature) =>
                                      mounted &&
                                      firmwareAutomation?.busy != true &&
                                      status?.json['board_id'] == pixelBoard &&
                                      status?.hostname == hostname &&
                                      status?.peer.has(feature) == true,
                                  busyChanged: (value) {
                                    manualMaintenanceBusy = value;
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
                  value: 'updates',
                  child: ListTile(
                    dense: true,
                    leading: Icon(PakarIcons.cloud),
                    title: Text('Updates & support…'),
                  ),
                ),
                const PopupMenuItem(
                  value: 'devices',
                  child: ListTile(
                    dense: true,
                    leading: Icon(PakarIcons.broadcast),
                    title: Text('Devices…'),
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
                onDestinationSelected: (value) => _navigate(pages[value].$1),
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
                  if (status != null &&
                      (status!.peer.has('ota') || status!.peer.has('isp')))
                    if (FirmwareProgress.observed(status!) case final progress?
                        when progress.active)
                      Padding(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 16,
                          vertical: 8,
                        ),
                        child: FirmwareProgressView(progress),
                      ),
                  StatusBar(
                    status: status,
                    retained: presented,
                    endpoint: api.baseUrl,
                    message: commandFailure ?? message,
                    commandFailed: commandFailure != null,
                  ),
                ],
              ),
            ),
          ],
        ),
        bottomNavigationBar: wide
            ? null
            : WorkspaceActivityStrip(
                pages: pages,
                selected: pages[index].$1,
                changed: _navigate,
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
    this.readStatus,
    this.ownerCurrent,
    this.seenFeatures = const {},
    this.workspace,
    this.connected = true,
    this.mappingCurrent = true,
    this.layoutAcknowledged = false,
  });
  final DeviceStatus? status;
  final CellMap mapping;
  final Command command;
  final VerifiedCommand? verifiedCommand;
  final Future<DeviceStatus> Function()? readStatus;
  final bool Function()? ownerCurrent;
  final Set<String> seenFeatures;
  final TableWorkspace? workspace;
  final bool connected;
  final bool mappingCurrent;
  final bool layoutAcknowledged;

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
  final searchFocus = FocusNode();
  bool get multiple => workspace.multiple;
  set multiple(bool value) => workspace.multiple = value;
  bool get showIndicators => workspace.showIndicators;
  set showIndicators(bool value) => workspace.showIndicators = value;
  String get cellEffect => workspace.cellEffect;
  set cellEffect(String value) => workspace.cellEffect = value;
  bool applying = false;
  String? batchMessage;

  @override
  void initState() {
    super.initState();
    searchFocus.addListener(() {
      if (!searchFocus.hasFocus) canonicalizeSearch();
    });
    try {
      selection = parseCellSearch(query.text);
    } on FormatException catch (error) {
      searchError = error.message;
    }
  }

  @override
  void dispose() {
    searchFocus.dispose();
    if (widget.workspace == null) workspace.dispose();
    super.dispose();
  }

  void canonicalizeSearch() {
    if (!mounted) return;
    setState(() {
      try {
        selection = parseCellSearch(query.text);
        query.text = formatCellSelection(selection);
        searchError = null;
      } on FormatException catch (error) {
        searchError = error.message;
      }
    });
  }

  void insertSearchSeparator(String separator) {
    final value = query.value;
    final range = value.selection.isValid
        ? value.selection
        : TextSelection.collapsed(offset: value.text.length);
    final next = const CellSearchInputFormatter().formatEditUpdate(
      value,
      TextEditingValue(
        text: value.text.replaceRange(range.start, range.end, separator),
        selection: TextSelection.collapsed(
          offset: range.start + separator.length,
        ),
      ),
    );
    query.value = next;
    searchFocus.requestFocus();
    updateSearch(next.text);
  }

  void updateSearch(String text) => setState(() {
    try {
      selection = parseCellSearch(text);
      searchError = null;
    } on FormatException catch (error) {
      searchError = error.message;
    }
  });

  void select(int id) {
    FocusScope.of(context).unfocus();
    setState(() {
      if (multiple) {
        final next = selection.toSet();
        if (!next.remove(id)) next.add(id);
        selection = next;
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
    String? failure;
    try {
      applied = await send('/api/cells', {
        'ids': ids.join(','),
        'state': state,
        'effect': cellEffect,
      });
    } catch (error) {
      failure = error.toString();
      // A transport failure may happen after the atomic mutation. Never fan
      // out/replay; require a fresh peer observation before another intent.
    }
    if (mounted) {
      setState(() {
        applying = false;
        batchMessage = applied
            ? '${ids.length} cell states read back after one board-side update; physical output not verified.'
            : failure ?? 'The command did not return a verified result. Nothing was retried.';
      });
    }
  }

  @override
  Widget build(BuildContext context) =>
      status?.peer.table != true && !widget.layoutAcknowledged
      ? const Center(child: Text('No acknowledged table'))
      : PageStorage(
          bucket: workspace.bucket,
          child: ListView(
            key: const PageStorageKey('table-page-scroll'),
            controller: pageScroll,
            primary: false,
            padding: const EdgeInsets.all(16),
            children: [
              const WorkspaceHeader(title: 'Table', icon: PakarIcons.gridFour),
              if ((status?.peer.has('cell-lock') == true ||
                      status?.peer.has('cell-assignments') == true ||
                      widget.seenFeatures.contains('cell-lock') ||
                      widget.seenFeatures.contains('cell-assignments')) &&
                  widget.verifiedCommand != null &&
                  widget.readStatus != null)
                CellAssignmentsActions(
                  key: ValueKey('cell-assignments:${status!.json['board_id']}'),
                  status: status!,
                  connected: widget.connected && !applying,
                  command: widget.verifiedCommand!,
                  readStatus: widget.readStatus!,
                  ownerCurrent: widget.ownerCurrent,
                  seenFeatures: widget.seenFeatures,
                ),
              TextField(
                controller: query,
                focusNode: searchFocus,
                keyboardType: const TextInputType.numberWithOptions(
                  signed: true,
                ),
                textInputAction: TextInputAction.done,
                autocorrect: false,
                enableSuggestions: false,
                inputFormatters: const [CellSearchInputFormatter()],
                decoration: InputDecoration(
                  labelText: 'Find logical cell',
                  hintText: '12, 3-8',
                  errorText: searchError,
                  suffixIcon: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      for (final separator in ['-', ','])
                        IconButton(
                          tooltip: separator == '-'
                              ? 'Insert range dash'
                              : 'Insert list comma',
                          onPressed: () => insertSearchSeparator(separator),
                          icon: Text(separator),
                        ),
                    ],
                  ),
                ),
                onChanged: updateSearch,
                onSubmitted: (_) => canonicalizeSearch(),
              ),
              Wrap(
                key: const ValueKey('table-colour-actions'),
                spacing: 8,
                runSpacing: 4,
                children: [
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
              Wrap(
                key: const ValueKey('table-selection-actions'),
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
                                      pixels: cellPixelPreview(
                                        status,
                                        mapping,
                                        id,
                                        mappingCurrent: widget.mappingCurrent,
                                      ),
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
    this.pixels,
  });
  final int id;
  final int physical;
  final String name;
  final String state;
  final VoidCallback? onPressed;
  final VoidCallback? onLongPress;
  final bool highlighted;
  final bool showIndicator;
  final CellPixelPreview? pixels;

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
          'Logical ${widget.id} · physical ${widget.physical > 0 ? widget.physical : 'unknown'} · ${widget.pixels?.description ?? widget.state}',
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
                    left: 4,
                    right: 4,
                    bottom: 4,
                    child: Row(
                      children: [
                        for (var pixel = 0; pixel < 6; ++pixel)
                          Expanded(
                            child: Container(
                              key: ValueKey('cell-${widget.id}-pixel-$pixel'),
                              height: 5,
                              margin: const EdgeInsets.symmetric(
                                horizontal: .5,
                              ),
                              decoration: BoxDecoration(
                                borderRadius: BorderRadius.circular(2),
                                color: widget.pixels == null
                                    ? studioDark.outline
                                    : Color(
                                        0xff000000 |
                                            widget.pixels!.colors[pixel],
                                      ),
                                border: Border.all(
                                  color: studioDark.outline,
                                  width: .5,
                                ),
                              ),
                            ),
                          ),
                      ],
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
    this.seenFeatures = const {},
    this.connected = true,
    this.workspace,
  });
  final DeviceStatus? status;
  final Command command;
  final PeripheralMirror? mirror;
  final Set<String> seenFeatures;
  final bool connected;
  final FrontPanelWorkspace? workspace;
  @override
  State<FrontPanelPage> createState() => _FrontPanelPageState();
}

class _FrontPanelPageState extends State<FrontPanelPage> {
  late final workspace = widget.workspace ?? FrontPanelWorkspace();
  TextEditingController get text => workspace.text;
  double get speed => workspace.speed;
  set speed(double value) => workspace.speed = value;
  String get repeat => workspace.repeat;
  set repeat(String value) => workspace.repeat = value;
  bool get forceScroll => workspace.forceScroll;
  set forceScroll(bool value) => workspace.forceScroll = value;
  String? get selectedCue => workspace.selectedCue;
  set selectedCue(String? value) => workspace.selectedCue = value;
  bool audioPending = false;
  List<String> seenCues = const [];
  bool seenPanelMirror = false,
      seenSegmentsMirror = false,
      seenBuzzerMirror = false;

  bool shown(String feature) =>
      widget.seenFeatures.contains(feature) ||
      widget.status?.peer.has(feature) == true;
  bool ready(String feature) =>
      widget.connected && widget.status?.peer.has(feature) == true;

  @override
  void initState() {
    super.initState();
    _rememberCues();
  }

  @override
  void didUpdateWidget(covariant FrontPanelPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    _rememberCues();
  }

  void _rememberCues() {
    final current = widget.status?.audioCues;
    if (current != null && current.isNotEmpty) seenCues = current;
  }

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
    if (widget.workspace == null) workspace.dispose();
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
    controller: workspace.scroll,
    padding: const EdgeInsets.all(16),
    children: [
      const Text(
        'Front panel',
        style: TextStyle(fontSize: 24, fontWeight: FontWeight.w700),
      ),
      const SizedBox(height: 12),
      if (widget.mirror != null) _mirrorControls(widget.mirror!),
      if (widget.mirror?.panelEnabled == true && seenPanelMirror)
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
                if (shown('inputs'))
                  Wrap(
                    spacing: 8,
                    runSpacing: 8,
                    children: [
                      for (var i = 0; i < 9; i++)
                        Chip(
                          avatar: Icon(
                            !ready('inputs')
                                ? Icons.help_outline
                                : i == 8
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
                if (shown('outputs'))
                  Wrap(
                    spacing: 8,
                    children: [
                      for (var i = 0; i < 8; i++)
                        Chip(
                          avatar: Icon(
                            Icons.circle,
                            size: 12,
                            color:
                                ready('outputs') &&
                                    (widget.status!.activeOutputs & (1 << i)) !=
                                        0
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
      if (shown('lcd'))
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
                for (final line
                    in widget.status?.lcdLines.isNotEmpty == true
                        ? widget.status!.lcdLines
                        : const ['Display unavailable', '                '])
                  Text(
                    line,
                    textDirection: TextDirection.ltr,
                    style: const TextStyle(fontFamily: 'monospace'),
                  ),
              ],
            ),
          ),
        ),
      if (shown('display') && widget.mirror?.segmentsEnabled != false)
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: SizedBox(
              height: 120,
              child:
                  (widget.mirror?.segments ??
                              widget.status?.segments ??
                              const [])
                          .length ==
                      4
                  ? SevenSegmentPreview(
                      segments:
                          widget.mirror?.segments ?? widget.status!.segments,
                      brightness:
                          widget.mirror?.brightness ??
                          widget.status!.segmentBrightness,
                      active: ready('display'),
                    )
                  : const Center(
                      child: Text('Waiting for a seven-segment frame'),
                    ),
            ),
          ),
        ),
      if (shown('text'))
        IgnorePointer(
          ignoring: !ready('text'),
          child: Opacity(
            opacity: ready('text') ? 1 : .5,
            child: Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  children: [
                    TextField(
                      controller: text,
                      maxLength: 40,
                      decoration: const InputDecoration(
                        labelText: 'Marquee text',
                      ),
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
                          onPressed: !ready('text')
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
          ),
        ),
      const SizedBox(height: 12),
      if (shown('buzzer') && seenCues.isNotEmpty) _melodyControls(),
    ],
  );

  Widget _melodyControls() {
    final cues = seenCues;
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
              onChanged:
                  audioPending ||
                      !ready('buzzer') ||
                      widget.status?.audioCues.isNotEmpty != true
                  ? null
                  : (value) => setState(() => selectedCue = value),
            ),
            const SizedBox(height: 12),
            FilledButton.icon(
              onPressed:
                  audioPending ||
                      !ready('buzzer') ||
                      widget.status?.audioCues.contains(cue) != true
                  ? null
                  : () => sendCue(cue),
              icon: const Icon(PakarIcons.musicNote),
              label: Text(audioPending ? 'Sending cue…' : 'Send melody'),
            ),
          ],
        ),
      ),
    );
  }

  Widget _mirrorControls(PeripheralMirror mirror) {
    seenPanelMirror |= mirror.panelAvailable;
    seenSegmentsMirror |= mirror.segmentsAvailable;
    seenBuzzerMirror |= mirror.buzzerAvailable;
    final requested = mirror.segmentsAvailable
        ? mirror.panelPresentation
        : null;
    return Card(
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
            if (mirror.panelEnabled && seenSegmentsMirror)
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: 16),
                child: SizedBox(
                  height: 40,
                  child: Row(
                    children: [
                      Text(
                        'Requested',
                        style: TextStyle(
                          color: Theme.of(context).colorScheme.onSurfaceVariant,
                        ),
                      ),
                      const SizedBox(width: 16),
                      Expanded(
                        child: Tooltip(
                          message: requested == null
                              ? '—'
                              : '${requested.text}\n${requested.kind} · ${requested.blink} · ${requested.marquee}',
                          child: Text(
                            requested?.preview ?? '—',
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: const TextStyle(
                              fontFamily: 'monospace',
                              fontSize: 22,
                            ),
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            if (seenPanelMirror)
              SwitchListTile.adaptive(
                title: const Text('Front panel'),
                subtitle: const Text('Live menu, ownership and physical keys'),
                value: mirror.panelEnabled,
                onChanged: mirror.setPanel,
              ),
            if (seenSegmentsMirror)
              SwitchListTile.adaptive(
                title: const Text('7-segment display'),
                subtitle: const Text(
                  'Actual segment masks, decimal points and brightness',
                ),
                value: mirror.segmentsEnabled,
                onChanged: mirror.setSegments,
              ),
            if (seenBuzzerMirror) ...[
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
            ] else if (shown('buzzer'))
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
  const HardwarePage({super.key, required this.status, this.inventory});
  final DeviceStatus? status;
  final PanelInventory? inventory;

  @override
  Widget build(BuildContext context) {
    final peer = status?.peer ?? PeerContext.unknown;
    final features = {...?inventory?.features, ...peer.features}.toList()
      ..sort();
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
      if (inventory?.avrObserved == true ||
          status?.peer.known == true &&
              status?.json.containsKey('avr_online') == true)
        ('AVR', fact('avr_online', 'Online', 'Offline'), PakarIcons.cpu),
      if (inventory?.has('display') == true ||
          status?.peer.has('display') == true)
        ('TM1637', fact('tm1637_ack', 'Ready', 'No ACK'), PakarIcons.gauge),
      if (inventory?.has('network') == true ||
          status?.peer.has('network') == true) ...[
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
      if (inventory?.has('inputs') == true ||
          status?.peer.has('inputs') == true)
        ('Inputs', mask('active_inputs'), PakarIcons.keyboard),
      if (inventory?.has('outputs') == true ||
          status?.peer.has('outputs') == true)
        ('Outputs', mask('active_outputs'), PakarIcons.lightning),
    ];
    if (values.isEmpty && features.isEmpty) {
      return const Center(child: Text('No acknowledged hardware measurements'));
    }
    return ListView(
      key: PageStorageKey('hardware:${inventory?.ownerKey ?? peer.hostname}'),
      padding: const EdgeInsets.all(16),
      children: [
        if (features.isNotEmpty)
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'Capabilities',
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                  const SizedBox(height: 8),
                  Wrap(
                    spacing: 8,
                    runSpacing: 8,
                    children: [
                      for (final feature in features)
                        Tooltip(
                          message: peer.has(feature)
                              ? 'Currently acknowledged'
                              : 'Previously acknowledged; currently unavailable',
                          child: Opacity(
                            opacity: peer.has(feature) ? 1 : 0.45,
                            child: Chip(
                              key: ValueKey('capability:$feature'),
                              label: Text(feature),
                            ),
                          ),
                        ),
                    ],
                  ),
                ],
              ),
            ),
          ),
        if (values.isNotEmpty)
          GridView(
            shrinkWrap: true,
            physics: const NeverScrollableScrollPhysics(),
            gridDelegate: SliverGridDelegateWithMaxCrossAxisExtent(
              maxCrossAxisExtent: 280,
              mainAxisExtent:
                  56 + MediaQuery.textScalerOf(context).scale(18) * 4,
            ),
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
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                                style: const TextStyle(
                                  color: Color(0xff909baa),
                                ),
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
    this.draft,
    this.openDevices,
  });
  final PakarApi api;
  final Future<void> Function() reconnect;
  final bool active;
  final ConnectionDraft? draft;
  final VoidCallback? openDevices;
  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  late final endpoint = TextEditingController(
    text: widget.draft?.endpoint ?? widget.api.baseUrl,
  );
  late final user = TextEditingController(
    text: widget.draft?.user ?? widget.api.user,
  );
  late final password = TextEditingController(text: widget.api.password);
  late String observedEndpoint, observedUser, observedPassword;
  bool saving = false;
  String? error;

  @override
  void initState() {
    super.initState();
    observedEndpoint = widget.api.baseUrl;
    observedUser = widget.api.user;
    observedPassword = widget.api.password;
  }

  @override
  void didUpdateWidget(covariant SettingsPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    // The shell restores its profile asynchronously. Follow only pristine
    // fields; telemetry/reconnect must never overwrite a user's draft.
    if (endpoint.text == observedEndpoint) {
      endpoint.text = widget.draft?.endpoint ?? widget.api.baseUrl;
    }
    if (user.text == observedUser) {
      user.text = widget.draft?.user ?? widget.api.user;
    }
    if (password.text == observedPassword) password.text = widget.api.password;
    observedEndpoint = widget.api.baseUrl;
    observedUser = widget.api.user;
    observedPassword = widget.api.password;
  }

  void edited(String _) =>
      widget.draft?.edit(endpoint.text, user.text, widget.api);

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
      WorkspaceSection(
        title: 'Board connection',
        icon: PakarIcons.link,
        children: [
          DropdownButtonFormField<ConnectionMethod>(
            key: ValueKey(widget.api.connectionMethod),
            initialValue: widget.api.connectionMethod,
            decoration: const InputDecoration(
              labelText: 'One selected transport',
            ),
            items: const [
              DropdownMenuItem(
                value: ConnectionMethod.usb,
                child: Text('USB OTG · offline direct board'),
              ),
              DropdownMenuItem(
                value: ConnectionMethod.network,
                child: Text('Network · Wi-Fi / Ethernet'),
              ),
              DropdownMenuItem(
                value: ConnectionMethod.bluetooth,
                child: Text('Bluetooth LE · offline board control'),
              ),
            ],
            onChanged: saving
                ? null
                : (value) async {
                    if (value == null) return;
                    if (value == ConnectionMethod.usb) {
                      widget.openDevices?.call();
                      return;
                    }
                    final preferences = await SharedPreferences.getInstance();
                    if (!mounted) return;
                    final previous =
                        preferences.getString(
                          value == ConnectionMethod.bluetooth
                              ? 'last_bluetooth_endpoint'
                              : 'last_network_endpoint',
                        ) ??
                        (value == ConnectionMethod.network
                            ? 'http://ledboard.local'
                            : null);
                    if (previous == null) {
                      widget.openDevices?.call();
                      return;
                    }
                    widget.api.selectTransport(
                      value,
                      previous,
                      board: value == ConnectionMethod.bluetooth
                          ? preferences.getString('bluetooth_board_id') ?? ''
                          : '',
                    );
                    endpoint.text = previous;
                    if (mounted) setState(() {});
                  },
          ),
          const SizedBox(height: 12),
          if (widget.api.connectionMethod == ConnectionMethod.network)
            SwitchListTile.adaptive(
              key: const Key('automatic-board-connection'),
              contentPadding: EdgeInsets.zero,
              title: const Text('Automatically connect to one available board'),
              subtitle: const Text(
                'Keep the selected healthy board. Choose automatically only when one compatible board is available; turn off for manual connections.',
              ),
              value: widget.api.automaticDiscovery,
              onChanged: saving || !widget.active
                  ? null
                  : (enabled) async {
                      setState(() {
                        saving = true;
                        error = null;
                      });
                      try {
                        final prefs = await SharedPreferences.getInstance();
                        await ConnectionPreferences(prefs)
                            .saveAutomaticDiscovery(enabled);
                        if (mounted) {
                          setState(
                            () => widget.api.automaticDiscovery = enabled,
                          );
                        }
                      } on Object catch (failure) {
                        if (mounted) setState(() => error = '$failure');
                      } finally {
                        if (mounted) setState(() => saving = false);
                      }
                    },
            ),
          if (widget.api.connectionMethod == ConnectionMethod.network)
            SwitchListTile.adaptive(
              key: const Key('command-http-fallback'),
              contentPadding: EdgeInsets.zero,
              title: const Text('Allow HTTP fallback for commands'),
              subtitle: const Text(
                'Prefer WebSocket for cells and pixels. When off, commands require WebSocket. Applies to the next command without reconnecting.',
              ),
              value: widget.api.commandHttpFallback,
              onChanged: saving || !widget.active
                  ? null
                  : (enabled) async {
                      final api = widget.api;
                      setState(() {
                        saving = true;
                        error = null;
                      });
                      try {
                        final prefs = await SharedPreferences.getInstance();
                        await ConnectionPreferences(prefs)
                            .saveCommandHttpFallback(enabled);
                        if (mounted && identical(api, widget.api)) {
                          setState(() => api.commandHttpFallback = enabled);
                        }
                      } on Object catch (failure) {
                        if (mounted) setState(() => error = '$failure');
                      } finally {
                        if (mounted) setState(() => saving = false);
                      }
                    },
            ),
          if (widget.api.isBluetooth) ...[
            SelectableText(
              widget.api.baseUrl.startsWith('ble://')
                  ? widget.api.baseUrl
                  : 'Select a Pakar Bluetooth board',
            ),
            const SizedBox(height: 12),
            FilledButton.icon(
              onPressed: widget.openDevices,
              icon: const Icon(PakarIcons.radio),
              label: const Text('Choose Bluetooth board'),
            ),
          ] else ...[
            TextField(
              controller: endpoint,
              onChanged: edited,
              decoration: const InputDecoration(
                labelText: 'Device hostname or URL',
              ),
            ),
            const SizedBox(height: 10),
            TextField(
              controller: user,
              onChanged: edited,
              decoration: const InputDecoration(labelText: 'User'),
            ),
            const SizedBox(height: 10),
            TextField(
              controller: password,
              obscureText: true,
              decoration: const InputDecoration(
                labelText: 'Password (optional)',
              ),
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
                          widget.draft?.clear();
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
        ],
      ),
      BoardSoundPreferences(api: widget.api, active: widget.active),
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
  const StatusBar({
    super.key,
    required this.status,
    required this.message,
    this.retained,
    this.endpoint = '',
    this.commandFailed = false,
  });
  final DeviceStatus? status;
  final DeviceStatus? retained;
  final String endpoint;
  final String message;
  final bool commandFailed;
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
          !commandFailed && status?.peer.known == true
              ? PakarIcons.check
              : PakarIcons.warning,
          size: 16,
          color: commandFailed ? Theme.of(context).colorScheme.error : null,
        ),
        const SizedBox(width: 8),
        Expanded(
          child: InkWell(
            key: const Key('command-status-details'),
            onTap: commandFailed
                ? () => showDialog<void>(
                    context: context,
                    builder: (context) => AlertDialog(
                      title: const Text('Command not completed'),
                      content: SelectableText(message),
                      actions: [
                        TextButton(
                          onPressed: () => Navigator.pop(context),
                          child: const Text('Close'),
                        ),
                      ],
                    ),
                  )
                : null,
            child: Tooltip(
              message: message,
              child: Text(
                message,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: commandFailed
                    ? TextStyle(color: Theme.of(context).colorScheme.error)
                    : null,
              ),
            ),
          ),
        ),
        Flexible(
          child: Text(
            status == null
                ? (retained?.hostname.isNotEmpty == true
                      ? '${retained!.hostname} · awaiting fresh state'
                      : Uri.tryParse(endpoint)?.host ?? '')
                : [
                    if (status!.hostname.isNotEmpty) status!.hostname,
                    if (status!.json['ethernet'] is Map &&
                        status!.json['ethernet']['ipv4_ready'] == true &&
                        status!.json['ethernet']['ip'] is String)
                      'Ethernet ${status!.json['ethernet']['ip']}',
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
