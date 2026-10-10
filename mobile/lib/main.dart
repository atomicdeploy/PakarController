import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';

import 'icons.dart';

import 'package:shared_preferences/shared_preferences.dart';

import 'api.dart';
import 'models.dart';
import 'connection_preferences.dart';
import 'cell_search.dart';
import 'peer_context.dart';
import 'application_artwork.dart';

final bundledApplicationArtwork = base64Decode(applicationArtworkBase64);

void main() => runApp(const PakarControllerApp());

const studioDark = ColorScheme.dark(
  primary: Color(0xff68a7ff),
  secondary: Color(0xff38d27a),
  error: Color(0xffff5c68),
  surface: Color(0xff11161e),
  onSurface: Color(0xffeef2f7),
  outline: Color(0xff29313d),
);

class PakarControllerApp extends StatelessWidget {
  const PakarControllerApp({super.key, this.api});
  final PakarApi? api;

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'PakarController',
    debugShowCheckedModeBanner: false,
    themeMode: ThemeMode.dark,
    darkTheme: ThemeData(
      fontFamily: 'Vazirmatn',
      colorScheme: studioDark,
      scaffoldBackgroundColor: const Color(0xff080a0e),
      cardTheme: const CardThemeData(
        color: Color(0xff11161e),
        elevation: 0,
        shape: RoundedRectangleBorder(
          side: BorderSide(color: Color(0xff29313d)),
          borderRadius: BorderRadius.all(Radius.circular(14)),
        ),
      ),
      inputDecorationTheme: const InputDecorationTheme(
        border: OutlineInputBorder(),
      ),
      useMaterial3: true,
    ),
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

class _ControlShellState extends State<ControlShell> {
  late final api = widget.api ?? PakarApi();
  DeviceStatus? status;
  CellMap mapping = CellMap(physical: const [], names: const []);
  StreamSubscription<DeviceStatus>? live;
  Timer? fallback;
  Timer? liveRetry;
  Timer? expiry;
  int? refreshingRevision;
  int observation = 0;
  String page = 'overview';
  String message = 'Connecting to ledboard.local';

  @override
  void initState() {
    super.initState();
    _restore();
  }

  Future<void> _restore() async {
    final prefs = await SharedPreferences.getInstance();
    try {
      await ConnectionPreferences(prefs).restore(api);
    } on Exception catch (error) {
      if (mounted) {
        setState(() => message = 'Preferences could not be restored: $error');
      }
      return;
    }
    if (!mounted) return;
    await _refresh();
    _connectLive();
    fallback = Timer.periodic(
      const Duration(seconds: 3),
      (_) => _refresh(silent: true),
    );
  }

  Future<void> _connectLive() async {
    if (!mounted) return;
    final owner = api.connectionRevision;
    liveRetry?.cancel();
    await live?.cancel();
    if (!mounted || owner != api.connectionRevision) return;
    live = api.liveStatus().listen(
      (value) {
        if (mounted && owner == api.connectionRevision) _observe(value);
      },
      onError: (Object _) {
        if (!mounted || owner != api.connectionRevision) return;
        _revoke('WebSocket disconnected');
        _retryLive();
      },
      onDone: () {
        if (owner == api.connectionRevision) {
          _revoke('WebSocket disconnected');
          _retryLive();
        }
      },
    );
  }

  void _retryLive() {
    if (!mounted) return;
    liveRetry?.cancel();
    liveRetry = Timer(const Duration(seconds: 2), _connectLive);
  }

  Future<void> _refresh({bool silent = false}) async {
    final owner = api.connectionRevision, started = observation;
    if (!mounted || refreshingRevision == owner) return;
    refreshingRevision = owner;
    try {
      final value = await api.status();
      if (!mounted ||
          owner != api.connectionRevision ||
          started != observation) {
        return;
      }
      _observe(value);
      final accepted = observation;
      if (value.peer.table) {
        try {
          final next = await api.mapping();
          if (mounted &&
              owner == api.connectionRevision &&
              accepted == observation) {
            setState(() => mapping = next);
          }
        } on Exception {
          /* Fresh status remains usable; no fabricated map. */
        }
      }
      if (!mounted || owner != api.connectionRevision) return;
      setState(() {
        if (!silent) message = 'Live device synchronized';
      });
    } catch (error) {
      if (mounted &&
          owner == api.connectionRevision &&
          started == observation) {
        _revoke(error.toString());
      }
    } finally {
      if (refreshingRevision == owner) refreshingRevision = null;
    }
  }

  void _observe(DeviceStatus value) {
    final changed = status?.hostname != value.hostname;
    expiry?.cancel();
    ++observation;
    setState(() {
      status = value;
      if (changed || !value.peer.table) {
        mapping = CellMap(physical: const [], names: const []);
      }
    });
    expiry = Timer(const Duration(seconds: 6), () {
      if (mounted) _revoke('Device state expired');
    });
  }

  void _revoke(String reason) {
    expiry?.cancel();
    ++observation;
    if (!mounted) return;
    setState(() {
      status = null;
      mapping = CellMap(physical: const [], names: const []);
      message = reason;
    });
  }

  Future<void> _reconnect() async {
    _revoke('Connecting to ${api.baseUrl}');
    liveRetry?.cancel();
    await live?.cancel();
    await _refresh();
    if (mounted) await _connectLive();
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
    try {
      if (owner != api.connectionRevision ||
          peer != status?.hostname ||
          status?.peer.allows(path, values) != true) {
        throw const FormatException(
          'This peer has not freshly acknowledged this capability',
        );
      }
      await api.post(path, values);
      if (!mounted || owner != api.connectionRevision) return false;
      await _refresh();
      return true;
    } catch (error) {
      if (mounted && owner == api.connectionRevision) {
        setState(() => message = error.toString());
      }
      return false;
    }
  }

  @override
  void dispose() {
    live?.cancel();
    fallback?.cancel();
    liveRetry?.cancel();
    expiry?.cancel();
    api.close();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final peer = status?.peer ?? PeerContext.unknown;
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
      ('connection', 'Connection', PakarIcons.gear),
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
              key: ValueKey('table:$owner:${hostname ?? ''}'),
              status: status,
              mapping: mapping,
              command: scoped,
              verifiedCommand: (path, values) =>
                  _command(owner, hostname, path, values),
            ),
            'front-panel' => FrontPanelPage(
              key: ValueKey('front:$owner:${hostname ?? ''}'),
              status: status,
              command: scoped,
            ),
            'hardware' => HardwarePage(status: status),
            'connection' => SettingsPage(
              key: const ValueKey('connection-preferences'),
              api: api,
              reconnect: _reconnect,
            ),
            _ => BoardOverview(status: status),
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
              onPressed: _refresh,
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
  });
  final DeviceStatus? status;
  final CellMap mapping;
  final Command command;
  final VerifiedCommand? verifiedCommand;

  @override
  State<TablePage> createState() => _TablePageState();
}

class _TablePageState extends State<TablePage> {
  DeviceStatus? get status => widget.status;
  CellMap get mapping => widget.mapping;
  Command get command => widget.command;
  final query = TextEditingController();
  Set<int> selection = {};
  String? searchError;
  bool multiple = true, showIndicators = false, applying = false;
  String? batchMessage;

  @override
  void dispose() {
    query.dispose();
    super.dispose();
  }

  void select(int id) => setState(() {
    if (multiple) {
      if (!selection.remove(id)) selection.add(id);
    } else {
      selection = {id};
    }
    query.text = formatCellSelection(selection);
    searchError = null;
  });

  Future<void> applySelection(String state) async {
    final send = widget.verifiedCommand;
    if (send == null || applying || selection.isEmpty) return;
    final ids = selection.toList()..sort();
    setState(() {
      applying = true;
      batchMessage = null;
    });
    var completed = 0;
    for (final id in ids) {
      if (!mounted ||
          !await send('/api/v1/cell', {
            'id': '$id',
            'state': state,
            'effect': 'fade',
          })) {
        break;
      }
      ++completed;
    }
    if (mounted) {
      setState(() {
        applying = false;
        batchMessage = completed == ids.length
            ? '$completed commands acknowledged; inspect live cells for output.'
            : 'Stopped after $completed/${ids.length}. Failed or uncertain commands were not retried.';
      });
    }
  }

  @override
  Widget build(BuildContext context) => status?.peer.table != true
      ? const Center(child: Text('No acknowledged table'))
      : ListView(
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
                        'Logical row-major identity · odd columns offset by half a cell',
                      ),
                    ],
                  ),
                ),
                FilledButton.icon(
                  onPressed:
                      status?.peer.modes.any(
                            (mode) => mode.id == 'first-wins',
                          ) !=
                          true
                      ? null
                      : () => command('/api/v1/game/reset'),
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
                    onSelected: (_) =>
                        command('/api/v1/mode', {'value': mode.id}),
                  ),
              ],
            ),
            const SizedBox(height: 12),
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
                  onSelected: (value) => setState(() => showIndicators = value),
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
                        selection.isEmpty ||
                            applying ||
                            widget.verifiedCommand == null
                        ? null
                        : () => applySelection(value),
                    child: Text('Set $value'),
                  ),
              ],
            ),
            if (batchMessage != null) Text(batchMessage!),
            const SizedBox(height: 12),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: AspectRatio(
                  aspectRatio: 10 / 8.6,
                  child: LayoutBuilder(
                    builder: (context, bounds) {
                      final pitchX = bounds.maxWidth / 10;
                      final pitchY = bounds.maxHeight / 8.5;
                      return Stack(
                        children: [
                          for (var id = 1; id <= 80; id++)
                            Positioned(
                              left: ((id - 1) % 10) * pitchX + 2,
                              top:
                                  ((id - 1) ~/ 10) * pitchY +
                                  (((id - 1) % 10).isEven ? pitchY * .5 : 0) +
                                  2,
                              width: pitchX - 4,
                              height: pitchY - 4,
                              child: CellButton(
                                key: ValueKey('cell-$id'),
                                id: id,
                                highlighted: selection.contains(id),
                                showIndicator: showIndicators,
                                name:
                                    mapping.names.elementAtOrNull(id - 1) ??
                                    '$id',
                                physical:
                                    mapping.physical.elementAtOrNull(id - 1) ??
                                    0,
                                state:
                                    status?.cells.elementAtOrNull(id - 1) ??
                                    'unknown',
                                onPressed: applying ? null : () => select(id),
                                onLongPress: !mapping.valid
                                    ? null
                                    : () => _editMapping(context, id),
                              ),
                            ),
                        ],
                      );
                    },
                  ),
                ),
              ),
            ),
          ],
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
      await command('/api/v1/cells/mapping', {
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
        : ((widget.id - 1) % 10).isEven
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
  });
  final DeviceStatus? status;
  final Command command;
  @override
  State<FrontPanelPage> createState() => _FrontPanelPageState();
}

class _FrontPanelPageState extends State<FrontPanelPage> {
  final text = TextEditingController(text: 'PAKAR CONTROLLER');
  double speed = 260;
  String repeat = 'once';
  bool forceScroll = false;

  @override
  void dispose() {
    text.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => ListView(
    padding: const EdgeInsets.all(16),
    children: [
      const Text(
        'Front panel',
        style: TextStyle(fontSize: 24, fontWeight: FontWeight.w700),
      ),
      const SizedBox(height: 12),
      if (widget.status?.peer.has('display') == true)
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: widget.status!.segments.length == 4
                ? SevenSegmentPreview(
                    segments: widget.status!.segments,
                    brightness: widget.status!.segmentBrightness,
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
                          : () => widget.command('/api/v1/display', {
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
      if (widget.status?.peer.has('buzzer') == true)
        Wrap(
          spacing: 8,
          children: [
            for (final cue in widget.status?.audioCues ?? const <String>[])
              ActionChip(
                avatar: const Icon(PakarIcons.musicNote),
                label: Text(cue),
                onPressed: widget.status == null
                    ? null
                    : () => widget.command('/api/v1/audio', {'cue': cue}),
              ),
          ],
        ),
    ],
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
  const SettingsPage({super.key, required this.api, required this.reconnect});
  final PakarApi api;
  final Future<void> Function() reconnect;
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
  Widget build(BuildContext context) => ListView(
    padding: const EdgeInsets.all(16),
    children: [
      const Text(
        'Preferences',
        style: TextStyle(fontSize: 24, fontWeight: FontWeight.w700),
      ),
      const SizedBox(height: 12),
      Card(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
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
                decoration: const InputDecoration(labelText: 'Password'),
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
        ),
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
