import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;

import 'api.dart';
import 'peer_identity.dart';
import 'device_registry.dart';
import 'firmware.dart';
import 'firmware_feed.dart';
import 'firmware_progress.dart';
import 'models.dart';

/// Public configuration only. Credentials and an in-flight operation are never
/// serialized. A persisted policy authorizes this exact board AND application,
/// not whichever device discovery happens to find next.
class FirmwareAutomationPolicy {
  const FirmwareAutomationPolicy({
    this.enabled = false,
    this.boardId = '',
    this.applicationId = '',
    this.catalogUrl = '',
    this.feedEndpoint = '',
    this.downloadOrigin = '',
    this.socketIo = false,
    this.namespace = '/',
    this.event = 'firmware-offer',
    this.autoStage = false,
    this.autoApply = false,
    this.pollSeconds = 300,
  });
  final bool enabled, socketIo, autoStage, autoApply;
  final String boardId,
      applicationId,
      catalogUrl,
      feedEndpoint,
      downloadOrigin,
      namespace,
      event;
  final int pollSeconds;

  FirmwareFeedOptions options({String token = ''}) => FirmwareFeedOptions(
    endpoint: feedEndpoint,
    origin: downloadOrigin,
    socketIo: socketIo,
    namespace: namespace,
    event: event,
    token: token,
  );

  void validate() {
    bool identity(String value, int maximum) =>
        value.isNotEmpty &&
        value.length <= maximum &&
        RegExp(r'^[!-~]+$').hasMatch(value);
    if (pollSeconds < 30 ||
        pollSeconds > 86400 ||
        autoApply && !autoStage ||
        enabled && (!identity(boardId, 80) || !identity(applicationId, 96)) ||
        enabled && catalogUrl.isEmpty && feedEndpoint.isEmpty) {
      throw const FormatException(
        'Choose an exact board/application, source and update policy',
      );
    }
    if (catalogUrl.isNotEmpty) {
      final url = Uri.tryParse(catalogUrl);
      if (url == null ||
          !{'http', 'https'}.contains(url.scheme) ||
          url.host.isEmpty ||
          url.userInfo.isNotEmpty ||
          url.hasFragment ||
          url.hasQuery ||
          catalogUrl.length > 2048 ||
          RegExp(r'[\x00-\x20\x7f-\uffff\\]').hasMatch(catalogUrl)) {
        throw const FormatException(
          'Use a direct configured HTTP(S) catalog without credentials or query',
        );
      }
    }
    // Reuse the actual exact-origin offer admission for catalogs too. A dummy
    // loopback feed validates that origin when no socket source is configured.
    if (enabled || downloadOrigin.isNotEmpty || feedEndpoint.isNotEmpty) {
      FirmwareFeedOptions(
        endpoint: feedEndpoint.isEmpty ? 'ws://127.0.0.1/' : feedEndpoint,
        origin: downloadOrigin,
        socketIo: socketIo,
        namespace: namespace,
        event: event,
      ).validate();
    }
  }

  Map<String, dynamic> toJson() => {
    'enabled': enabled,
    'board_id': boardId,
    'application_id': applicationId,
    'catalog_url': catalogUrl,
    'feed_endpoint': feedEndpoint,
    'download_origin': downloadOrigin,
    'socket_io': socketIo,
    'namespace': namespace,
    'event': event,
    'auto_stage': autoStage,
    'auto_apply': autoApply,
    'poll_seconds': pollSeconds,
  };

  static FirmwareAutomationPolicy parse(Object? raw) {
    if (raw is! Map) return const FirmwareAutomationPolicy();
    String text(String key, [String fallback = '']) =>
        raw[key] is String ? raw[key] as String : fallback;
    final policy = FirmwareAutomationPolicy(
      enabled: raw['enabled'] == true,
      boardId: text('board_id'),
      applicationId: text('application_id'),
      catalogUrl: text('catalog_url'),
      feedEndpoint: text('feed_endpoint'),
      downloadOrigin: text('download_origin'),
      socketIo: raw['socket_io'] == true,
      namespace: text('namespace', '/'),
      event: text('event', 'firmware-offer'),
      autoStage: raw['auto_stage'] == true,
      autoApply: raw['auto_apply'] == true,
      pollSeconds: raw['poll_seconds'] is int
          ? raw['poll_seconds'] as int
          : 300,
    );
    policy.validate();
    return policy;
  }
}

/// App-lifetime foreground delivery, not a page-owned socket or an Android
/// background-service promise. Import actual hostbridge session retirement and
/// downloader bounds through FirmwareFeed/FirmwareController. Read-only polling
/// and reconnect may repeat; a consumed firmware digest never repeats a write.
class FirmwareAutomation extends ChangeNotifier {
  FirmwareAutomation(
    this.api, {
    required this.authorized,
    required this.save,
    Map<String, dynamic>? initial,
    http.Client? catalogClient,
    this.reconnectDelay = const Duration(seconds: 5),
  }) : _client = catalogClient ?? http.Client(),
       _ownsClient = catalogClient == null {
    try {
      policy = FirmwareAutomationPolicy.parse(initial?['policy']);
      final prior = initial?['consumed'];
      if (prior != null && (prior is! List || prior.length > 128) ||
          policy.enabled && prior == null) {
        throw const FormatException(
          'Missing or oversized consumed firmware receipts',
        );
      }
      if (prior is List) {
        for (final key in prior) {
          if (key is String &&
              key.length <= 300 &&
              RegExp(r'^[!-~]+$').hasMatch(key)) {
            _consumed.add(key);
          } else {
            throw const FormatException('Malformed consumed firmware receipt');
          }
        }
      }
    } on Object {
      policy = const FirmwareAutomationPolicy();
      error =
          'Saved firmware policy is invalid; automatic delivery is disabled';
    }
    _status = api.statusUpdates.listen(_observe);
    feed.addListener(_feedChanged);
  }
  final PakarApi api;
  final bool Function(String feature) authorized;
  final Future<void> Function(Map<String, dynamic>) save;
  final http.Client _client;
  final bool _ownsClient;
  final Duration reconnectDelay;
  late FirmwareAutomationPolicy policy;
  final feed = FirmwareFeed();
  final _consumed = <String>[];
  late final StreamSubscription<DeviceStatus> _status;
  Timer? _poll, _reconnect;
  Future<void>? _reconciling;
  Future<void> _saving = Future<void>.value();
  FirmwareController? _controller;
  String _feedToken = '', _downloadBearer = '';
  String session = '', state = 'Stopped', error = '';
  String? _boot;
  int? _owner;
  int _epoch = 0;
  bool _foreground = true,
      _closed = false,
      _reading = false,
      _connecting = false;
  bool busy = false, uncertain = false;
  RemoteJob? job;
  RemoteJob? _submittedJob;
  FirmwareProgress? progress;
  FirmwareOffer? pending;
  bool get running => _controller != null && session.isNotEmpty;
  bool get foreground => _foreground;
  Map<String, dynamic> toJson() => {
    'policy': policy.toJson(),
    'consumed': List<String>.of(_consumed),
  };

  Future<void> _persist() {
    // Serialize settings/receipt writes; a late old offer cannot overwrite a
    // newer configured policy after an awaited SharedPreferences write.
    final snapshot = jsonDecode(jsonEncode(toJson())) as Map<String, dynamic>;
    final task = _saving.catchError((Object _) {}).then((_) => save(snapshot));
    _saving = task;
    return task;
  }

  Future<void> configure(
    FirmwareAutomationPolicy value, {
    String feedToken = '',
    String downloadBearer = '',
  }) async {
    value.validate();
    if (RegExp(r'[^!-~]').hasMatch(feedToken) ||
        feedToken.length > 256 ||
        RegExp(r'[^!-~]').hasMatch(downloadBearer) ||
        downloadBearer.length > 256) {
      throw const FormatException(
        'Firmware credentials must be bounded printable tokens',
      );
    }
    await stop();
    if (_closed) return;
    final previous = policy;
    policy = value;
    try {
      await _persist();
    } on Object {
      policy = previous;
      error =
          'Firmware policy could not be saved; no automatic delivery started';
      notifyListeners();
      rethrow;
    }
    _feedToken = feedToken;
    _downloadBearer = downloadBearer;
    _submittedJob = null;
    uncertain = false;
    error = '';
    await reconcile();
  }

  Future<void> setForeground(bool value) async {
    _foreground = value;
    if (!value) {
      await stop();
      if (!_foreground) {
        state = policy.enabled
            ? 'Paused while app is in background'
            : 'Stopped';
      }
      if (!_closed) notifyListeners();
    } else {
      await reconcile();
    }
  }

  bool _matches(DeviceStatus? status) =>
      !_closed &&
      _foreground &&
      policy.enabled &&
      api.connectionWanted &&
      api.connectionMethod == ConnectionMethod.network &&
      status != null &&
      status.peer.known &&
      status.json['board_id'] == policy.boardId &&
      status.peer.application?.id == policy.applicationId &&
      status.json['boot_id'] is String &&
      RegExp(r'^[!-~]{1,80}$').hasMatch(status.json['boot_id']);

  bool _owned(int generation) =>
      generation == _epoch &&
      !uncertain &&
      _owner == api.connectionRevision &&
      _boot == api.latestStatus?.json['boot_id'] &&
      _matches(api.latestStatus);

  void _observe(DeviceStatus status) {
    if (_closed) return;
    // Inspection remains read-only even after an uncertain ACK. Neither a
    // reconnect nor a new boot resurrects a dispatch, and no absent counter is
    // converted to zero/completed progress.
    if (_matches(status)) _updateObservation(status);
    if (!running) {
      if (_matches(status)) notifyListeners();
      if (_matches(status) && !uncertain) unawaited(reconcile());
      return;
    }
    if (!_owned(_epoch)) {
      unawaited(_retireAndReconcile());
      return;
    }
    notifyListeners();
  }

  void _updateObservation(DeviceStatus status) {
    progress = FirmwareProgress.observed(status);
    final raw = status.json['remote_firmware'];
    if (raw is Map<String, dynamic>) {
      try {
        final observed = RemoteJob.parse(raw);
        job = observed.boot == status.json['boot_id'] ? observed : null;
      } on FormatException {
        job = null;
      }
    } else {
      job = null;
    }
    final expected = _submittedJob, actual = job;
    if (expected != null &&
        actual != null &&
        actual.id == expected.id &&
        actual.hash == expected.hash &&
        actual.targets == expected.targets) {
      if (actual.phase == 'complete' && !actual.busy) {
        state = 'Board verified update completion';
      } else if (actual.phase == 'failed' || actual.phase == 'cancelled') {
        state = 'Board update ${actual.phase}';
      }
    }
  }

  Future<void> _retireAndReconcile() async {
    await stop();
    await reconcile();
  }

  /// Does not connect to a discovered device or change the user's route. The
  /// existing selected route must first ACK this policy's exact identity.
  Future<void> reconcile() {
    final active = _reconciling;
    if (active != null) return active;
    late final Future<void> task;
    task = _reconcile().whenComplete(() {
      if (identical(_reconciling, task)) _reconciling = null;
    });
    return _reconciling = task;
  }

  Future<void> _reconcile() async {
    if (_closed || !_foreground || !policy.enabled || uncertain) return;
    final observed = api.latestStatus;
    if (!_matches(observed)) {
      if (running) await stop();
      state = 'Waiting for the configured board';
      if (!_closed) notifyListeners();
      return;
    }
    if (running && _owned(_epoch)) return;
    await stop();
    if (!api.liveFresh) {
      try {
        // A cached manifest is not admission. Preserve HTTP-only fallback by
        // obtaining a new status read when a live WS lease is unavailable.
        await api.status();
      } on Object {
        state = 'Waiting for fresh board identity';
        if (!_closed) notifyListeners();
        return;
      }
    }
    if (!_matches(api.latestStatus) || uncertain) return;
    _owner = api.connectionRevision;
    _boot = api.latestStatus!.json['boot_id'];
    session = randomIdentifier();
    final generation = _epoch;
    _controller = FirmwareController(
      api,
      authorized: (feature) => _owned(generation) && authorized(feature),
    );
    state = 'Listening · foreground';
    error = '';
    _updateObservation(api.latestStatus!);
    notifyListeners();
    _poll = Timer.periodic(Duration(seconds: policy.pollSeconds), (_) {
      unawaited(checkNow());
    });
    await _startFeed(generation);
    if (_owned(generation) && policy.catalogUrl.isNotEmpty) await checkNow();
  }

  Future<void> _startFeed(int generation) async {
    if (!_owned(generation) ||
        policy.feedEndpoint.isEmpty ||
        feed.running ||
        _connecting) {
      return;
    }
    _connecting = true;
    session = randomIdentifier();
    pending = null;
    try {
      await feed.start(
        policy.options(token: _feedToken),
        policy.boardId,
        _boot!,
        armed: true,
        sessionId: session,
        deliver: (offer) => acceptOffer(offer),
      );
    } on Object {
      if (_owned(generation)) {
        error = 'Firmware feed unavailable; read-only reconnect scheduled';
        _scheduleReconnect(generation);
        notifyListeners();
      }
    } finally {
      _connecting = false;
    }
  }

  void _feedChanged() {
    if (_closed) return;
    if (running && !feed.running && !_connecting && _owned(_epoch)) {
      _scheduleReconnect(_epoch);
    }
    notifyListeners();
  }

  void _scheduleReconnect(int generation) {
    if (_reconnect != null || policy.feedEndpoint.isEmpty) return;
    _reconnect = Timer(reconnectDelay, () {
      _reconnect = null;
      if (_owned(generation)) unawaited(_startFeed(generation));
    });
  }

  Future<void> checkNow() async {
    if (!running) await reconcile();
    final generation = _epoch;
    if (!_owned(generation) || _reading || busy || policy.catalogUrl.isEmpty) {
      return;
    }
    _reading = true;
    try {
      error = '';
      if (!api.liveFresh) {
        await api.status();
        if (!_owned(generation)) return;
      }
      final request = http.Request('GET', Uri.parse(policy.catalogUrl))
        ..followRedirects = false;
      request.headers.addAll({
        ...PeerIdentity.current.headers,
        'Accept': 'application/json',
        'X-Pakar-Firmware-Session': session,
        'X-Pakar-Board': policy.boardId,
        'X-Pakar-Boot': _boot!,
      });
      if (_feedToken.isNotEmpty) {
        request.headers['Authorization'] = 'Bearer $_feedToken';
      }
      final response = await _client
          .send(request)
          .timeout(const Duration(seconds: 10));
      if (!_owned(generation) ||
          response.statusCode != 200 ||
          (response.contentLength ?? 0) > 65536 ||
          (response.headers['content-encoding'] != null &&
              response.headers['content-encoding'] != 'identity')) {
        await response.stream.listen((_) {}).cancel();
        throw const FormatException(
          'Firmware catalog rejected or owner changed',
        );
      }
      final bytes = <int>[];
      final reader = StreamIterator(response.stream);
      final deadline = Stopwatch()..start();
      try {
        while (true) {
          final remaining = const Duration(seconds: 15) - deadline.elapsed;
          if (remaining <= Duration.zero) {
            throw TimeoutException('Catalog deadline');
          }
          if (!await reader.moveNext().timeout(remaining)) break;
          if (!_owned(generation) ||
              bytes.length + reader.current.length > 65536) {
            throw const FormatException(
              'Firmware catalog limit or owner changed',
            );
          }
          bytes.addAll(reader.current);
        }
      } finally {
        await reader.cancel();
      }
      if (!_owned(generation)) return;
      if (response.contentLength != null &&
          response.contentLength != bytes.length) {
        throw const FormatException('Truncated firmware catalog');
      }
      final raw = jsonDecode(utf8.decode(bytes));
      final offers = raw is Map && raw['offers'] is List
          ? raw['offers'] as List
          : [raw];
      if (offers.length > 16) {
        throw const FormatException('Firmware catalog offer limit');
      }
      for (final rawOffer in offers) {
        if (!_owned(generation) || busy || uncertain) break;
        final offer = FirmwareOffer.parse(
          rawOffer,
          _optionsForCatalog,
          session,
          policy.boardId,
          _boot!,
        );
        await acceptOffer(offer);
      }
    } on Object {
      if (_owned(generation)) {
        error = 'Firmware catalog unavailable or invalid; no write retried';
      }
    } finally {
      _reading = false;
      if (!_closed) notifyListeners();
    }
  }

  FirmwareFeedOptions get _optionsForCatalog => FirmwareFeedOptions(
    endpoint: policy.feedEndpoint.isEmpty
        ? 'ws://127.0.0.1/'
        : policy.feedEndpoint,
    origin: policy.downloadOrigin,
    socketIo: policy.socketIo,
    namespace: policy.namespace,
    event: policy.event,
  );

  String _key(FirmwareOffer offer) =>
      '${policy.boardId}|${policy.applicationId}|${offer.kind}|${offer.target}|${offer.hash}';

  Future<void> acceptOffer(FirmwareOffer offer) async {
    final generation = _epoch;
    if (!_owned(generation) || busy) return;
    FirmwareOffer.parse(
      offer.json,
      _optionsForCatalog,
      session,
      policy.boardId,
      _boot!,
    );
    if (_consumed.contains(_key(offer))) return;
    // Automatic programming uses only the full pinned board transaction. Raw
    // AVR may stage, but raw ESP is an immediate flash and requires review.
    if (!policy.autoStage || offer.kind == 'binary' && offer.target == 'esp') {
      pending = offer;
      state = 'Firmware offer ready for review';
      notifyListeners();
      return;
    }
    await _deliver(offer, apply: policy.autoApply && offer.kind == 'manifest');
  }

  Future<void> reviewPending({bool apply = false}) async {
    final offer = pending;
    if (offer == null) return;
    if (offer.kind == 'binary' && offer.target == 'esp' && !apply) {
      throw const FormatException(
        'ESP binary installation requires explicit Apply',
      );
    }
    await _deliver(offer, apply: apply);
  }

  Future<void> _deliver(FirmwareOffer offer, {required bool apply}) async {
    final generation = _epoch, controller = _controller;
    if (!_owned(generation) || controller == null || busy || uncertain) return;
    void admit() {
      if (!_owned(generation)) throw const StaleConnection();
      FirmwareOffer.parse(
        offer.json,
        _optionsForCatalog,
        session,
        policy.boardId,
        _boot!,
      );
    }

    admit();
    final key = _key(offer);
    if (_consumed.contains(key)) return;
    busy = true;
    var submitted = false;
    notifyListeners();
    try {
      if (!api.liveFresh) await api.status();
      admit();
      final current = await controller.status();
      admit();
      if (current.busy || current.phase == 'ready') {
        pending = offer;
        state = 'Board maintenance owns the update slot';
        return;
      }
      final previous = List<String>.of(_consumed);
      if (_consumed.length >= 128) _consumed.removeAt(0);
      _consumed.add(key);
      try {
        await _persist();
      } on Object {
        _consumed
          ..clear()
          ..addAll(previous);
        throw const FormatException(
          'Cannot persist firmware receipt; no upload submitted',
        );
      }
      if (!api.liveFresh) await api.status();
      admit();
      pending = null;
      state = apply ? 'Submitting pinned automatic update' : 'Staging firmware';
      if (offer.kind == 'manifest') {
        submitted = true;
        job = await controller.stage(
          offer.url,
          offer.hash,
          offer.target == 'all'
              ? 3
              : offer.target == 'esp'
              ? 1
              : 2,
          _downloadBearer,
          autoApply: apply,
        );
        _submittedJob = job;
        state = apply
            ? 'Board accepted update · awaiting verified completion'
            : 'Board accepted stage · Apply remains explicit';
      } else {
        await controller.fileUrl(
          offer.url,
          offer.hash,
          offer.target == 'esp',
          bearer: _downloadBearer,
          progress: (phase, done, total) {
            if (_owned(generation)) {
              progress = FirmwareProgress('Application', phase, done, total);
              notifyListeners();
            }
          },
          beforeUpload: () {
            admit();
            submitted = true;
          },
        );
        state = offer.target == 'avr'
            ? 'AVR image staged · not programmed'
            : 'ESP update accepted · awaiting reboot/readback';
      }
      error = '';
    } on Object {
      if (submitted) {
        uncertain = true;
        error = 'Update outcome needs inspection; the consumed offer will not replay';
        state = 'Inspection required';
        await feed.stop();
      } else if (_owned(generation)) {
        error = 'Firmware offer rejected before upload; no write retried';
      }
    } finally {
      busy = false;
      if (!_closed) notifyListeners();
    }
  }

  Future<void> stop() async {
    ++_epoch;
    _poll?.cancel();
    _poll = null;
    _reconnect?.cancel();
    _reconnect = null;
    _controller?.dispose();
    _controller = null;
    _owner = null;
    _boot = null;
    session = '';
    pending = null;
    job = null;
    progress = null;
    await feed.stop();
    state = 'Stopped';
    if (!_closed) notifyListeners();
  }

  @override
  void dispose() {
    _closed = true;
    feed.removeListener(_feedChanged);
    unawaited(_status.cancel());
    unawaited(stop());
    feed.dispose();
    if (_ownsClient) _client.close();
    super.dispose();
  }
}
