import 'dart:async';

import 'package:flutter/material.dart';

import 'icons.dart';

import 'package:shared_preferences/shared_preferences.dart';

import 'api.dart';
import 'models.dart';
import 'connection_preferences.dart';
import 'cell_search.dart';

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

class ControlShell extends StatefulWidget {
  const ControlShell({super.key, this.api});
  final PakarApi? api;
  @override
  State<ControlShell> createState() => _ControlShellState();
}

class _ControlShellState extends State<ControlShell> {
  late final api = widget.api ?? PakarApi();
  DeviceStatus? status;
  CellMap mapping = CellMap.defaults();
  StreamSubscription<DeviceStatus>? live;
  Timer? fallback;
  Timer? liveRetry;
  bool refreshing = false;
  int page = 0;
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
    liveRetry?.cancel();
    await live?.cancel();
    if (!mounted) return;
    live = api.liveStatus().listen(
      (value) => mounted ? setState(() => status = value) : null,
      onError: (Object _) {
        if (mounted) {
          setState(() {
            status = null;
            message = 'WebSocket disconnected';
          });
        }
        _retryLive();
      },
      onDone: _retryLive,
    );
  }

  void _retryLive() {
    if (!mounted) return;
    liveRetry?.cancel();
    liveRetry = Timer(const Duration(seconds: 2), _connectLive);
  }

  Future<void> _refresh({bool silent = false}) async {
    if (!mounted || refreshing) return;
    refreshing = true;
    try {
      final values = await Future.wait([api.status(), api.mapping()]);
      if (!mounted) return;
      setState(() {
        status = values[0] as DeviceStatus;
        mapping = values[1] as CellMap;
        if (!silent) message = 'Live device synchronized';
      });
    } catch (error) {
      if (mounted) {
        setState(() {
          status = null;
          message = error.toString();
        });
      }
    } finally {
      refreshing = false;
    }
  }

  Future<void> command(
    String path, [
    Map<String, String> values = const {},
  ]) async {
    try {
      await api.post(path, values);
      await _refresh();
    } catch (error) {
      if (mounted) setState(() => message = error.toString());
    }
  }

  @override
  void dispose() {
    live?.cancel();
    fallback?.cancel();
    liveRetry?.cancel();
    api.close();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final wide = MediaQuery.sizeOf(context).width >= 840;
    final destinations = [
      const NavigationDestination(
        icon: Icon(PakarIcons.gridFour),
        label: 'Table',
      ),
      const NavigationDestination(
        icon: Icon(PakarIcons.gauge),
        label: 'Front panel',
      ),
      const NavigationDestination(
        icon: Icon(PakarIcons.circuitry),
        label: 'Hardware',
      ),
      const NavigationDestination(
        icon: Icon(PakarIcons.gear),
        label: 'Settings',
      ),
    ];
    final body = switch (page) {
      0 => TablePage(status: status, mapping: mapping, command: command),
      1 => FrontPanelPage(status: status, command: command),
      2 => HardwarePage(status: status),
      _ => SettingsPage(
        api: api,
        reconnect: () async {
          await _refresh();
          _connectLive();
        },
      ),
    };
    return Scaffold(
      appBar: AppBar(
        title: const Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'PakarController',
              style: TextStyle(fontWeight: FontWeight.w700),
            ),
            Text(
              'AtomicDeploy control surface',
              style: TextStyle(fontSize: 11, color: Color(0xff909baa)),
            ),
          ],
        ),
        actions: [
          StatusPill(
            ok: status?.avrOnline == true,
            label: status == null
                ? 'State unavailable'
                : status?.avrOnline == true
                ? 'AVR online'
                : 'AVR offline',
          ),
          IconButton(
            onPressed: _refresh,
            icon: const Icon(PakarIcons.arrowClockwise),
          ),
        ],
      ),
      body: Row(
        children: [
          if (wide)
            NavigationRail(
              selectedIndex: page,
              onDestinationSelected: (value) => setState(() => page = value),
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
              selectedIndex: page,
              onDestinationSelected: (value) => setState(() => page = value),
              destinations: destinations,
            ),
    );
  }
}

class TablePage extends StatefulWidget {
  const TablePage({
    super.key,
    required this.status,
    required this.mapping,
    required this.command,
  });
  final DeviceStatus? status;
  final CellMap mapping;
  final Command command;

  @override
  State<TablePage> createState() => _TablePageState();
}

class _TablePageState extends State<TablePage> {
  DeviceStatus? get status => widget.status;
  CellMap get mapping => widget.mapping;
  Command get command => widget.command;
  Set<int> searchMatches = {};
  String? searchError;

  @override
  Widget build(BuildContext context) => ListView(
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
                  style: TextStyle(fontSize: 24, fontWeight: FontWeight.w700),
                ),
                Text(
                  'Logical row-major identity · odd columns offset by half a cell',
                ),
              ],
            ),
          ),
          FilledButton.icon(
            onPressed: status == null
                ? null
                : () => command('/api/v1/game/reset'),
            icon: const Icon(PakarIcons.arrowCounterClockwise),
            label: const Text('Reset'),
          ),
        ],
      ),
      const SizedBox(height: 12),
      TextField(
        decoration: InputDecoration(
          labelText: 'Find logical cell',
          hintText: '12, 3–8',
          errorText: searchError,
        ),
        onChanged: (text) => setState(() {
          try {
            searchMatches = parseCellSearch(text);
            searchError = null;
          } on FormatException catch (error) {
            searchMatches = {};
            searchError = error.message;
          }
        }),
      ),
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
                    Positioned.fill(
                      child: Row(
                        children: [
                          for (var column = 0; column < 10; ++column)
                            Expanded(
                              child: ColoredBox(
                                color: column.isEven
                                    ? const Color(0xff0d1016)
                                    : const Color(0xff181e28),
                              ),
                            ),
                        ],
                      ),
                    ),
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
                          id: id,
                          highlighted: searchMatches.contains(id),
                          name: mapping.names.elementAtOrNull(id - 1) ?? '$id',
                          physical:
                              mapping.physical.elementAtOrNull(id - 1) ?? id,
                          state:
                              status?.cells.elementAtOrNull(id - 1) ??
                              'unknown',
                          onPressed:
                              status?.cells.elementAtOrNull(id - 1) == null
                              ? null
                              : () async {
                                  final current =
                                      status?.cells.elementAtOrNull(id - 1) ??
                                      'off';
                                  final next = current == 'white'
                                      ? 'red'
                                      : current == 'red'
                                      ? 'off'
                                      : 'white';
                                  await command('/api/v1/cell', {
                                    'id': '$id',
                                    'state': next,
                                    'effect': 'fade',
                                  });
                                },
                          onLongPress: status == null
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

class CellButton extends StatelessWidget {
  const CellButton({
    super.key,
    required this.id,
    required this.name,
    required this.physical,
    required this.state,
    required this.onPressed,
    required this.onLongPress,
    this.highlighted = false,
  });
  final int id;
  final int physical;
  final String name;
  final String state;
  final VoidCallback? onPressed;
  final VoidCallback? onLongPress;
  final bool highlighted;

  @override
  Widget build(BuildContext context) {
    final background = state == 'white'
        ? const Color(0xffeef2f7)
        : state == 'red'
        ? const Color(0xffff5c68)
        : const Color(0xff0d1016);
    final foreground = state == 'white'
        ? const Color(0xff17202b)
        : const Color(0xffeef2f7);
    return Tooltip(
      message: 'Logical $id · physical $physical · $state',
      child: Material(
        color: background,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(9),
          side: BorderSide(
            color: highlighted ? const Color(0xff68a7ff) : Colors.transparent,
            width: 2,
          ),
        ),
        child: InkWell(
          onTap: onPressed,
          onLongPress: onLongPress,
          borderRadius: BorderRadius.circular(9),
          child: Center(
            child: FittedBox(
              child: Text(
                name,
                style: TextStyle(
                  color: foreground,
                  fontWeight: FontWeight.w700,
                ),
              ),
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
      Card(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            children: [
              if (widget.status == null || widget.status!.segments.length != 4)
                const Text('Display state unavailable')
              else
                SevenSegmentPreview(
                  segments: widget.status?.segments ?? const [],
                  brightness: widget.status?.segmentBrightness ?? 0,
                  active: widget.status?.tmReady == true,
                ),
              const SizedBox(height: 16),
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
                onChanged: (value) => setState(() => repeat = value ?? 'once'),
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
      Wrap(
        spacing: 8,
        runSpacing: 8,
        children: [
          for (final mode in ['idle', 'cell', 'first-wins', 'snake'])
            ChoiceChip(
              label: Text(mode),
              selected: widget.status?.mode == mode,
              onSelected: widget.status == null
                  ? null
                  : (_) => widget.command('/api/v1/mode', {'value': mode}),
            ),
        ],
      ),
      const SizedBox(height: 12),
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
      ('AVR', fact('avr_online', 'Online', 'Offline'), PakarIcons.cpu),
      ('TM1637', fact('tm1637_ack', 'Ready', 'No ACK'), PakarIcons.gauge),
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
      ('Inputs', mask('active_inputs'), PakarIcons.keyboard),
      ('Outputs', mask('active_outputs'), PakarIcons.lightning),
    ];
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
          status?.tmReady == true ? PakarIcons.check : PakarIcons.warning,
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
