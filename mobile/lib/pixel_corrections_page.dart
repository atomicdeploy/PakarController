import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';

import 'api.dart';
import 'controller_dialog.dart';
import 'models.dart';
import 'pixel_correction_address.dart';
import 'pixel_correction_preview.dart';
import 'pixel_corrections_editor.dart';
import 'pixel_diagnostic.dart';
import 'workspace_widgets.dart';

typedef PixelCorrectionsPeer = ({int connection, Object? board, Object? boot});

/// Peer-scoped drafts: transient previews never persist or change the raw scene.
class PixelCorrectionsPage extends StatefulWidget {
  const PixelCorrectionsPage({
    super.key,
    required this.api,
    required this.busyChanged,
    this.workspace,
    this.mapping,
    this.expectedPeer,
    this.incomingRule,
    this.active = true,
    this.livePreviewInitiallyEnabled,
  });
  final PakarApi api;
  final ValueChanged<bool> busyChanged;
  final PixelDiagnosticWorkspace? workspace;

  /// Caller supplies only its current peer-bound mapping, never a default map.
  final CellMap? Function()? mapping;
  final PixelCorrectionsPeer? expectedPeer;
  final PixelCorrectionRuleIntent? incomingRule;
  final bool active;
  final bool? livePreviewInitiallyEnabled;
  @override
  State<PixelCorrectionsPage> createState() => PixelCorrectionsPageState();
}

enum _CorrectionExit { cancel, save, discard }

class PixelCorrectionsPageState extends State<PixelCorrectionsPage>
    with AutomaticKeepAliveClientMixin, WidgetsBindingObserver {
  static const path = '/api/pixels/corrections';
  late final int owner;
  late final Object? board, boot;
  Map<String, dynamic>? saved, selectedRule;
  final List<Map<String, dynamic>> rules = [];
  final Set<String> invalidFields = {};
  final Map<Map<String, dynamic>, int> ruleIds = Map.identity();
  final formKey = GlobalKey<FormState>();
  final rulesScroll = ScrollController();
  int nextRuleId = 0;
  bool enabled = true, busy = false, dirty = false;
  // Default-on policy is not an active lease: opening/loading never writes.
  // An explicit edit starts the bounded RAM preview; Save alone writes flash.
  late bool previewOnEdit;
  String? error;
  StreamSubscription<dynamic>? updates;
  StreamSubscription<int>? configurationUpdates;
  int? noticedRevision;
  final Map<int, int> importedDrafts = {};
  bool foreground = true;
  CellMap? _addressMapping;
  bool _mappingPending = false;
  int _mappingAttempts = 0;
  int _mappingGeneration = 0;
  bool _allowSuppliedMap = true;
  Timer? _mappingRetry;
  String? _mappingError;
  Future<bool>? _leaving;
  PixelCorrectionRuleIntent? _consumedRule;
  bool _applyingIncomingRule = false;
  late final preview = PixelCorrectionPreview(
    api: widget.api,
    allowed: () =>
        mounted &&
        widget.active &&
        foreground &&
        current &&
        !busy &&
        !_applyingIncomingRule &&
        saved != null &&
        invalidFields.isEmpty &&
        toggleSupported,
    configuration: () => {
      'revision': saved!['revision'],
      'enabled': enabled,
      'rules': rules,
    },
    pixels: () => pixels,
  );
  bool get toggleSupported =>
      !rules.any((rule) => rule['enabled'] == false) ||
      widget.api.freshStatus?.peer.has('pixel-correction-rule-toggle') == true;
  bool get previewSupported =>
      widget.api.freshStatus?.peer.has('pixel-corrections-preview') == true;
  @override
  bool get wantKeepAlive => true;
  @override
  void initState() {
    super.initState();
    previewOnEdit =
        widget.livePreviewInitiallyEnabled ??
        widget.workspace?.correctionLivePreview ??
        true;
    final observed = widget.api.freshStatus;
    final expected =
        widget.expectedPeer ??
        (
          connection: widget.api.connectionRevision,
          board: observed?.json['board_id'],
          boot: observed?.json['boot_id'],
        );
    owner = expected.connection;
    board = expected.board;
    boot = expected.boot;
    WidgetsBinding.instance.addObserver(this);
    preview.addListener(workspaceChanged);
    widget.workspace?.addListener(workspaceChanged);
    configurationUpdates = widget.api.configurationUpdates.listen((_) {
      if (!mounted || !current) return;
      _mappingGeneration++;
      _mappingRetry?.cancel();
      _mappingAttempts = 0;
      setState(() {
        _addressMapping = null;
        _allowSuppliedMap = false;
      });
      unawaited(_hydrateAddressMapping());
    });
    updates = widget.api.statusUpdates.listen((status) {
      if (!mounted) return;
      if (preview.enabled && !preview.samePeer) unawaited(preview.stop());
      if (!current) {
        _addressMapping = null;
        _mappingRetry?.cancel();
      } else {
        unawaited(_hydrateAddressMapping());
      }
      if (busy || _leaving != null || saved == null) return;
      final revision = status.json['pixel_corrections_revision'];
      if (!current) {
        setState(() => error = 'Connection changed. Reopen this editor.');
        return;
      }
      if (revision is int &&
          revision > (saved!['revision'] as int) &&
          revision != noticedRevision) {
        noticedRevision = revision;
        unawaited(preview.stop());
        if (dirty) {
          setState(
            () => error =
                'Another client changed corrections. Reload before saving.',
          );
        } else {
          load();
        }
      } else {
        // Mapping/topology can change without changing correction rules.
        setState(() {});
      }
    });
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) {
        unawaited(_hydrateAddressMapping());
        load();
      }
    });
  }

  @override
  void didUpdateWidget(PixelCorrectionsPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!widget.active && oldWidget.active) {
      _mappingRetry?.cancel();
      unawaited(preview.stop());
    } else if (widget.active && !oldWidget.active) {
      _mappingAttempts = 0;
      unawaited(_hydrateAddressMapping());
    }
    if (oldWidget.workspace != widget.workspace) {
      oldWidget.workspace?.removeListener(workspaceChanged);
      widget.workspace?.addListener(workspaceChanged);
    }
    unawaited(_consumeIncomingRule());
  }

  void workspaceChanged() {
    if (mounted) {
      setState(() {});
      unawaited(_consumeIncomingRule());
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    foreground = state == AppLifecycleState.resumed;
    if (!foreground) {
      _mappingRetry?.cancel();
      unawaited(preview.stop());
    } else {
      _mappingAttempts = 0;
      unawaited(_hydrateAddressMapping());
      unawaited(_consumeIncomingRule());
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    preview.removeListener(workspaceChanged);
    preview.dispose();
    widget.workspace?.removeListener(workspaceChanged);
    updates?.cancel();
    configurationUpdates?.cancel();
    _mappingRetry?.cancel();
    rulesScroll.dispose();
    super.dispose();
  }

  bool get current =>
      owner == widget.api.connectionRevision &&
      board != null &&
      boot != null &&
      widget.api.freshStatus?.json['board_id'] == board &&
      widget.api.freshStatus?.json['boot_id'] == boot;
  bool get editable =>
      saved != null && !busy && !_applyingIncomingRule && current;

  /// Append one explicit output-slot intent only after verified configuration.
  /// Save remains explicit; the enabled preview policy may display this draft.
  Future<void> _consumeIncomingRule() async {
    final intent = widget.incomingRule;
    if (intent == null ||
        identical(intent, _consumedRule) ||
        _applyingIncomingRule ||
        !mounted ||
        !widget.active ||
        !foreground ||
        busy ||
        preview.pending ||
        saved == null ||
        _leaving != null) {
      return;
    }
    _consumedRule = intent;
    if (!current ||
        widget.api.freshStatus?.peer.has('pixel-corrections') != true) {
      setState(() => error = 'Correction target changed. Reopen this editor.');
      return;
    }
    if (intent.revision != null &&
        (dirty || intent.revision != saved!['revision'])) {
      setState(
        () => error =
            'Correction mapping changed. Select the diagnostic pixel again.',
      );
      return;
    }
    if (rules.length >= maxRules) {
      setState(
        () => error = 'The board allows at most $maxRules correction rules.',
      );
      return;
    }
    final next = intent.toRule();
    try {
      pixelCorrectionSources(pixels, true, [next]);
    } on FormatException {
      setState(
        () => error = 'Invalid correction for output slot ${intent.pixel + 1}.',
      );
      return;
    }
    _applyingIncomingRule = true;
    var appended = false;
    try {
      // stop() disables renewal synchronously; only our old lease may be cleared.
      if (preview.enabled) await preview.stop();
      if (!mounted ||
          !widget.active ||
          !foreground ||
          !current ||
          !identical(widget.incomingRule, intent)) {
        return;
      }
      setState(() {
        rules.add(next);
        selectedRule = next;
        dirty = true;
        error = null;
      });
      appended = true;
    } finally {
      _applyingIncomingRule = false;
      if (mounted) {
        setState(() {});
        if (!identical(widget.incomingRule, _consumedRule)) {
          unawaited(_consumeIncomingRule());
        }
      }
    }
    if (appended &&
        mounted &&
        previewOnEdit &&
        previewSupported &&
        invalidFields.isEmpty &&
        toggleSupported) {
      await preview.start();
    }
  }

  CellMap? get addressMapping {
    if (!current) return null;
    final status = widget.api.freshStatus, supplied = widget.mapping?.call();
    if (pixelCorrectionMapMatches(status, _addressMapping)) {
      return _addressMapping;
    }
    return _allowSuppliedMap && pixelCorrectionMapMatches(status, supplied)
        ? supplied
        : null;
  }

  /// Read-only label hydration is independent of the correction draft. The
  /// route's captured board/boot remains its owner; this never saves or previews.
  Future<void> _hydrateAddressMapping({bool retry = false}) async {
    if (!mounted ||
        !widget.active ||
        !foreground ||
        !current ||
        widget.api.freshStatus?.peer.table != true ||
        _mappingPending) {
      return;
    }
    if (addressMapping != null && !retry) return;
    if (retry) {
      _mappingRetry?.cancel();
      _mappingAttempts = 0;
    }
    if (_mappingRetry?.isActive == true || _mappingAttempts >= 3) return;
    _mappingPending = true;
    _mappingAttempts++;
    final generation = _mappingGeneration;
    try {
      final value = await widget.api.mapping();
      if (!mounted || !current || generation != _mappingGeneration) return;
      if (!pixelCorrectionMapMatches(widget.api.freshStatus, value)) {
        throw const FormatException(
          'Logical mapping does not match the current pixel groups',
        );
      }
      setState(() {
        _addressMapping = value;
        _mappingAttempts = 0;
        _mappingError = null;
      });
    } on Object catch (failure) {
      if (generation == _mappingGeneration) _mappingError = failure.toString();
      if (mounted &&
          current &&
          generation == _mappingGeneration &&
          widget.active &&
          foreground &&
          _mappingAttempts < 3) {
        // Only failed metadata reads retry; no steady-state/per-frame polling.
        _mappingRetry = Timer(const Duration(seconds: 2), () {
          unawaited(_hydrateAddressMapping());
        });
      }
    } finally {
      _mappingPending = false;
      if (mounted) setState(() {});
      if (mounted && generation != _mappingGeneration) {
        unawaited(_hydrateAddressMapping());
      }
    }
  }

  int get pixels => (saved?['pixels'] as int?) ?? 480;
  int get maxRules => ((saved?['max_rules'] as int?) ?? 32).clamp(0, 32);
  int idOf(Map<String, dynamic> rule) =>
      ruleIds.putIfAbsent(rule, () => nextRuleId++);
  void edit(VoidCallback change) {
    setState(() {
      change();
      dirty = true;
    });
    if (invalidFields.isEmpty &&
        toggleSupported &&
        previewOnEdit &&
        previewSupported) {
      if (preview.enabled) {
        preview.edited();
      } else {
        unawaited(preview.start());
      }
    } else {
      unawaited(preview.stop());
    }
  }

  int? get selectedDiagnosticPixel {
    final workspace = widget.workspace;
    if (workspace == null) return null;
    final index =
        (int.tryParse(workspace.position) ?? workspace.probe.first + 1) - 1;
    return index >= 0 && index < pixels ? index : null;
  }

  void addRule([int pixel = 0]) {
    if (!editable || rules.length >= maxRules) return;
    edit(() {
      selectedRule = {'kind': 'black', 'pixel': pixel, 'offset': 0};
      rules.add(selectedRule!);
    });
  }

  void importPixel(int pixel) {
    if (!editable) return;
    final index = selectedRule == null ? -1 : rules.indexOf(selectedRule!);
    final preceding = index < 0 ? rules : rules.take(index).toList();
    final List<int> matches;
    try {
      matches = pixelCorrectionOutputSlots(
        raw: pixel,
        pixels: pixels,
        sources: pixelCorrectionSources(pixels, enabled, preceding),
      );
    } on FormatException catch (failure) {
      setState(() => error = failure.message.toString());
      return;
    }
    if (matches.length != 1) {
      setState(
        () => error = matches.isEmpty
            ? 'Source ${pixel + 1} is absent before this rule. Enter its output slot explicitly.'
            : 'Source ${pixel + 1} reaches output slots ${matches.map((value) => value + 1).join(', ')}. Choose an output slot explicitly.',
      );
      return;
    }
    final wire = matches.single;
    if (selectedRule == null || !rules.contains(selectedRule)) {
      addRule(wire);
      return;
    }
    edit(() {
      selectedRule!['pixel'] = wire;
      invalidFields.remove('${idOf(selectedRule!)}-pixel');
      final id = idOf(selectedRule!);
      importedDrafts[id] = (importedDrafts[id] ?? 0) + 1;
    });
  }

  void moveRule(int index, int delta) {
    if (!editable || index + delta < 0 || index + delta >= rules.length) return;
    edit(() {
      final rule = rules.removeAt(index);
      rules.insert(index + delta, rule);
      selectedRule = rule;
    });
  }

  void setValidity(String key, bool valid) {
    setState(() {
      if (valid) {
        invalidFields.remove(key);
      } else {
        invalidFields.add(key);
        dirty = true;
      }
    });
    if (!valid) unawaited(preview.stop());
  }

  Future<bool> load({bool save = false}) async {
    if (busy || _applyingIncomingRule || preview.pending || !current) {
      return false;
    }
    if (save &&
        (saved == null ||
            formKey.currentState?.validate() != true ||
            invalidFields.isNotEmpty)) {
      return false;
    }
    if (save && !toggleSupported) {
      setState(
        () => error = 'This board needs firmware with per-rule enable support before disabled rules can be saved.',
      );
      return false;
    }
    final ownedLease = preview.lease;
    setState(() {
      busy = true;
      error = null;
    });
    widget.busyChanged(true);
    try {
      if (widget.api.isBluetooth) {
        throw StateError(
          'Pixel correction configuration requires Wi-Fi or USB for this connection.',
        );
      }
      if (save) {
        await widget.api.postJson(path, {
          'configuration': jsonEncode({
            'revision': saved!['revision'],
            'enabled': enabled,
            'rules': rules,
            'lease_id': ?ownedLease,
          }),
        }, authorize: () => mounted && current);
        if (!mounted || !current) throw const StaleConnection();
      }
      final result = await widget.api.getJson(path);
      if (!mounted || !current) throw const StaleConnection();
      if (result['rules'] is! List ||
          result['revision'] is! int ||
          (result['revision'] as int) < 0 ||
          (result['revision'] as int) > 0xffffffff ||
          result['pixels'] is! int ||
          result['enabled'] is! bool) {
        throw const FormatException('Invalid correction configuration');
      }
      final actual = (result['rules'] as List)
          .map((rule) => Map<String, dynamic>.from(rule as Map))
          .toList();
      pixelCorrectionSources(
        result['pixels'] as int,
        result['enabled'] as bool,
        actual,
      );
      final same =
          actual.length == rules.length &&
          List.generate(
            rules.length,
            (index) => ['kind', 'pixel', 'offset', 'enabled'].every(
              (key) =>
                  (actual[index][key] ??
                      (key == 'offset'
                          ? 0
                          : key == 'enabled'
                          ? true
                          : null)) ==
                  (rules[index][key] ?? (key == 'enabled' ? true : null)),
            ),
          ).every((value) => value);
      if (save &&
          (result['enabled'] != enabled ||
              !same ||
              (result['revision'] as int) < (saved!['revision'] as int))) {
        throw StateError(
          'Board correction readback differs; reload before editing again.',
        );
      }
      if (save && result['error'] != null) {
        throw StateError(result['error'].toString());
      }
      if (save) preview.committed();
      setState(() {
        saved = result;
        enabled = result['enabled'] == true;
        rules
          ..clear()
          ..addAll(
            actual.map((rule) => {...rule, 'offset': rule['offset'] ?? 0}),
          );
        ruleIds.clear();
        importedDrafts.clear();
        invalidFields.clear();
        selectedRule = rules.isEmpty ? null : rules.first;
        dirty = false;
        error = result['error']?.toString();
      });
      return true;
    } catch (failure) {
      if (mounted) setState(() => error = failure.toString());
      return false;
    } finally {
      widget.busyChanged(false);
      if (mounted) setState(() => busy = false);
      if (mounted) unawaited(_consumeIncomingRule());
    }
  }

  /// Shared by route Back/close and tab navigation. Adapted from the actual
  /// PCController EffectEditor dismiss/save guard; a board save additionally
  /// requires same-peer durable readback. Cancel never changes preview/output.
  Future<bool> requestLeave() {
    final pending = _leaving;
    if (pending != null) return pending;
    final request = _confirmLeave();
    _leaving = request;
    unawaited(request.whenComplete(() => _leaving = null));
    return request;
  }

  Future<bool> _confirmLeave() async {
    if (busy || _applyingIncomingRule || preview.pending) return false;
    if (dirty || invalidFields.isNotEmpty) {
      final choice = await showDialog<_CorrectionExit>(
        context: context,
        barrierDismissible: false,
        builder: (dialog) => ControllerDialog(
          title: 'Unsaved corrections',
          icon: Icons.edit_note_rounded,
          onClose: () => Navigator.pop(dialog, _CorrectionExit.cancel),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(dialog, _CorrectionExit.cancel),
              child: const Text('Cancel'),
            ),
            TextButton(
              onPressed: () => Navigator.pop(dialog, _CorrectionExit.discard),
              child: const Text('Discard'),
            ),
            FilledButton.icon(
              onPressed:
                  current &&
                      saved != null &&
                      invalidFields.isEmpty &&
                      toggleSupported
                  ? () => Navigator.pop(dialog, _CorrectionExit.save)
                  : null,
              icon: const Icon(Icons.save_outlined, size: 18),
              label: const Text('Save to board'),
            ),
          ],
          child: const Text(
            'Save these edits before leaving? Discard keeps the saved board configuration.',
          ),
        ),
      );
      if (!mounted ||
          choice == null ||
          choice == _CorrectionExit.cancel ||
          busy ||
          preview.pending) {
        return false;
      }
      if (choice == _CorrectionExit.save) {
        if (!await load(save: true) || !mounted || !current) return false;
      } else {
        setState(() {
          final baseline = saved;
          enabled = baseline?['enabled'] == true;
          rules
            ..clear()
            ..addAll(
              (baseline?['rules'] as List? ?? const []).map((value) {
                final rule = Map<String, dynamic>.from(value as Map);
                return {...rule, 'offset': rule['offset'] ?? 0};
              }),
            );
          selectedRule = rules.isEmpty ? null : rules.first;
          ruleIds.clear();
          importedDrafts.clear();
          invalidFields.clear();
          dirty = false;
        });
      }
    }
    // Only an approved departure retires the opt-in RAM preview. This clears
    // our exact lease; it neither writes flash nor touches another owner.
    await preview.stop();
    return mounted;
  }

  Future<void> reload() async {
    if (dirty) {
      final discard = await showDialog<bool>(
        context: context,
        builder: (context) => AlertDialog(
          title: const Text('Discard correction edits?'),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(context, true),
              child: const Text('Reload'),
            ),
          ],
        ),
      );
      if (discard != true) return;
    }
    await preview.stop();
    await load();
  }

  List<int>? get observedRgb {
    final status = widget.api.freshStatus;
    if (!current || status == null) return null;
    final frame = status.json['pixel_frame'];
    if (frame is! Map || frame['pending'] == true) return null;
    final rgb = frame['rgb'];
    if (rgb is! String ||
        rgb.length != pixels * 6 ||
        !RegExp(r'^[0-9a-fA-F]+$').hasMatch(rgb)) {
      return null;
    }
    return List.generate(
      pixels,
      (index) => int.parse(rgb.substring(index * 6, index * 6 + 6), radix: 16),
    );
  }

  Widget operationOption(String kind, Map<String, dynamic> rule, int index) {
    final focus = rule['pixel'] as int;
    final start = (focus - 2).clamp(0, (pixels - 6).clamp(0, pixels));
    final end = (start + 6).clamp(0, pixels);
    final sources = pixelCorrectionSources(pixels, true, [
      ...rules.take(index),
      {
        ...rule,
        'kind': kind,
        'enabled': true,
        'offset': kind == 'shift' ? rule['offset'] : 0,
      },
    ]);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: [
          Row(
            children: [
              Icon(correctionOperationIcon(kind), size: 18),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  correctionOperationTitle(kind),
                  style: const TextStyle(fontWeight: FontWeight.w600),
                ),
              ),
            ],
          ),
          const SizedBox(height: 5),
          Text(
            correctionOperationDescription(kind),
            style: Theme.of(context).textTheme.bodySmall,
          ),
          const SizedBox(height: 6),
          PixelCorrectionDiagram(
            compact: true,
            sources: sources.sublist(start, end),
            start: start,
            focus: focus,
            rawRgb: observedRgb,
          ),
        ],
      ),
    );
  }

  Widget ruleCard(Map<String, dynamic> rule, int index) {
    final colors = Theme.of(context).colorScheme;
    final id = idOf(rule),
        selected = identical(rule, selectedRule),
        active = rule['enabled'] != false;
    final operation = DropdownButtonFormField<String>(
      key: ValueKey('operation-$id-${rule['kind']}'),
      initialValue: rule['kind'] as String,
      isExpanded: true,
      itemHeight: null,
      menuMaxHeight: 480,
      decoration: const InputDecoration(
        labelText: 'Operation',
        prefixIcon: Icon(Icons.tune_rounded),
      ),
      selectedItemBuilder: (context) => ['black', 'remove', 'shift']
          .map(
            (kind) => Align(
              alignment: AlignmentDirectional.centerStart,
              child: Text(
                correctionOperationTitle(kind),
                overflow: TextOverflow.ellipsis,
              ),
            ),
          )
          .toList(),
      items: ['black', 'remove', 'shift']
          .map(
            (kind) => DropdownMenuItem(
              value: kind,
              child: operationOption(kind, rule, index),
            ),
          )
          .toList(),
      onChanged: !editable || !active
          ? null
          : (kind) => edit(() {
              rule['kind'] = kind!;
              rule['offset'] = 0;
              selectedRule = rule;
              invalidFields.remove('$id-offset');
            }),
    );
    final pixelField = PixelCorrectionNumberControl(
      key: ValueKey('pixel-$id-${importedDrafts[id] ?? 0}'),
      label: 'Pixel',
      value: (rule['pixel'] as int) + 1,
      minimum: 1,
      maximum: pixels,
      enabled: editable && active,
      onValidityChanged: (valid) => setValidity('$id-pixel', valid),
      onChanged: (value) => edit(() {
        rule['pixel'] = value - 1;
        selectedRule = rule;
      }),
    );
    final offsetField = PixelCorrectionNumberControl(
      key: ValueKey('offset-$id'),
      label: 'Offset',
      value: rule['offset'] as int,
      minimum: -pixels,
      maximum: pixels,
      signed: true,
      enabled: editable && active,
      onValidityChanged: (valid) => setValidity('$id-offset', valid),
      onChanged: (value) => edit(() {
        rule['offset'] = value;
        selectedRule = rule;
      }),
    );
    return Card(
      key: ObjectKey(rule),
      margin: const EdgeInsets.only(bottom: 10),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(14),
        side: BorderSide(
          color: selected
              ? colors.primary.withValues(alpha: .5)
              : colors.outlineVariant.withValues(alpha: .4),
        ),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(12, 4, 12, 12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Checkbox(
                  value: active,
                  onChanged: !editable
                      ? null
                      : (value) => edit(() {
                          rule['enabled'] = value!;
                        }),
                  semanticLabel: 'Enable rule ${index + 1}',
                ),
                Expanded(
                  child: TextButton(
                    onPressed: () => setState(() => selectedRule = rule),
                    style: TextButton.styleFrom(
                      alignment: AlignmentDirectional.centerStart,
                      padding: const EdgeInsets.symmetric(horizontal: 4),
                    ),
                    child: Text(
                      'Rule ${index + 1}',
                      style: TextStyle(
                        color: selected
                            ? colors.primary
                            : colors.onSurfaceVariant,
                      ),
                    ),
                  ),
                ),
                IconButton(
                  tooltip: 'Move up',
                  onPressed: editable && index > 0
                      ? () => moveRule(index, -1)
                      : null,
                  icon: const Icon(Icons.arrow_upward_rounded, size: 19),
                ),
                IconButton(
                  tooltip: 'Move down',
                  onPressed: editable && index + 1 < rules.length
                      ? () => moveRule(index, 1)
                      : null,
                  icon: const Icon(Icons.arrow_downward_rounded, size: 19),
                ),
                IconButton(
                  tooltip: 'Delete rule',
                  onPressed: !editable
                      ? null
                      : () => edit(() {
                          rules.removeAt(index);
                          invalidFields.removeWhere(
                            (key) => key.startsWith('$id-'),
                          );
                          ruleIds.remove(rule);
                          importedDrafts.remove(id);
                          if (selected) {
                            selectedRule = rules.isEmpty
                                ? null
                                : rules[index.clamp(0, rules.length - 1)];
                          }
                        }),
                  icon: Icon(
                    Icons.delete_outline_rounded,
                    color: editable ? colors.error : null,
                    size: 20,
                  ),
                ),
              ],
            ),
            Padding(
              padding: const EdgeInsetsDirectional.fromSTEB(4, 0, 4, 10),
              child: Text(
                pixelCorrectionAddress(
                  wire: rule['pixel'] as int,
                  pixels: pixels,
                  precedingRules: rules.take(index).toList(),
                  enabled: enabled,
                  current: current,
                  status: widget.api.freshStatus,
                  mapping: addressMapping,
                ),
                key: ValueKey('correction-address-$id'),
                style: Theme.of(context).textTheme.bodySmall
                    ?.copyWith(color: colors.onSurfaceVariant),
              ),
            ),
            Opacity(
              opacity: active ? 1 : .55,
              child: LayoutBuilder(
                builder: (context, bounds) {
                  final compact =
                      bounds.maxWidth >= 720 &&
                      MediaQuery.textScalerOf(context).scale(14) <= 20;
                  if (compact) {
                    return Row(
                      key: ValueKey('compact-rule-$id'),
                      crossAxisAlignment: CrossAxisAlignment.end,
                      children: [
                        Expanded(
                          child: Tooltip(
                            message: correctionOperationDescription(
                              rule['kind'] as String,
                            ),
                            child: operation,
                          ),
                        ),
                        const SizedBox(width: 16),
                        SizedBox(width: 190, child: pixelField),
                        if (rule['kind'] == 'shift') ...[
                          const SizedBox(width: 16),
                          SizedBox(width: 190, child: offsetField),
                        ],
                      ],
                    );
                  }
                  return Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      operation,
                      Padding(
                        padding: const EdgeInsets.symmetric(vertical: 10),
                        child: Text(
                          correctionOperationDescription(
                            rule['kind'] as String,
                          ),
                          style: Theme.of(context).textTheme.bodySmall
                              ?.copyWith(color: colors.onSurfaceVariant),
                        ),
                      ),
                      if (rule['kind'] != 'shift')
                        Align(
                          alignment: AlignmentDirectional.centerStart,
                          child: SizedBox(
                            width: bounds.maxWidth.clamp(0, 260),
                            child: pixelField,
                          ),
                        )
                      else if (bounds.maxWidth < 420)
                        Column(
                          children: [
                            pixelField,
                            const SizedBox(height: 12),
                            offsetField,
                          ],
                        )
                      else
                        Row(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Expanded(child: pixelField),
                            const SizedBox(width: 16),
                            Expanded(child: offsetField),
                          ],
                        ),
                    ],
                  );
                },
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget mappingPreview() {
    final draft = pixelCorrectionSources(pixels, enabled, rules);
    final focus =
        (selectedRule?['pixel'] as int?) ?? selectedDiagnosticPixel ?? 0;
    final start = (focus - 2).clamp(0, (pixels - 6).clamp(0, pixels)),
        end = (start + 6).clamp(0, pixels);
    final applied = saved?['sources'];
    final appliedSources =
        applied is List &&
            applied.length == pixels &&
            applied.every(
              (value) => value is int && value >= -1 && value < pixels,
            )
        ? applied.cast<int>()
        : null;
    return WorkspaceSection(
      title: 'Local mapping preview',
      icon: Icons.compare_arrows_rounded,
      trailing: WorkspaceBadge(dirty ? 'Draft' : 'Saved', active: dirty),
      children: [
        if (invalidFields.isNotEmpty)
          Text(
            'Complete the invalid number to update this preview.',
            style: TextStyle(color: Theme.of(context).colorScheme.error),
          )
        else ...[
          Wrap(
            spacing: 6,
            crossAxisAlignment: WrapCrossAlignment.center,
            children: [
              Text(
                'Wire address',
                style: Theme.of(context).textTheme.labelMedium,
              ),
              const Icon(Icons.arrow_forward_rounded, size: 14),
              Text(
                'Source pixel',
                style: Theme.of(context).textTheme.labelMedium,
              ),
            ],
          ),
          const SizedBox(height: 10),
          PixelCorrectionDiagram(
            sources: draft.sublist(start, end),
            start: start,
            focus: focus,
            rawRgb: observedRgb,
          ),
          const SizedBox(height: 8),
          Text(
            enabled
                ? 'Crossed slots send black. Numbers inside slots identify the raw scene pixel.'
                : 'Corrections disabled: the raw scene passes through unchanged.',
            style: Theme.of(context).textTheme.bodySmall,
          ),
          if (dirty && appliedSources != null) ...[
            const SizedBox(height: 14),
            Text(
              'Saved on board',
              style: Theme.of(context).textTheme.labelMedium,
            ),
            const SizedBox(height: 6),
            PixelCorrectionDiagram(
              sources: appliedSources.sublist(start, end),
              start: start,
              focus: focus,
              compact: true,
              rawRgb: observedRgb,
            ),
          ],
        ],
      ],
    );
  }

  Widget boardPreviewControl() {
    final live = preview.enabled && preview.observed != null;
    return Tooltip(
      message:
          'Preview the draft on the LEDs. Save to board makes it persistent.',
      child: FilterChip(
        key: const ValueKey('correction-live-preview'),
        showCheckmark: true,
        avatar: Icon(
          live ? Icons.visibility_rounded : Icons.visibility_outlined,
          size: 18,
        ),
        label: Text(preview.pending && !live ? 'Starting…' : 'Live preview'),
        selected: previewOnEdit,
        onSelected: previewOnEdit
            ? (_) {
                setState(() => previewOnEdit = false);
                widget.workspace?.correctionLivePreview = false;
                unawaited(preview.stop());
              }
            : editable &&
                  !preview.pending &&
                  widget.active &&
                  foreground &&
                  invalidFields.isEmpty &&
                  toggleSupported
            ? (_) {
                setState(() => previewOnEdit = true);
                widget.workspace?.correctionLivePreview = true;
                unawaited(preview.start());
              }
            : null,
      ),
    );
  }

  void showMapping() {
    showDialog<void>(
      context: context,
      builder: (context) => ControllerDialog(
        title: 'Wire mapping',
        icon: Icons.account_tree_outlined,
        maxWidth: 700,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            mappingPreview(),
            if (saved != null)
              ExpansionTile(
                title: Text(
                  'Applied wire mapping · revision ${saved!['revision']}',
                ),
                children: [
                  Padding(
                    padding: const EdgeInsets.all(12),
                    child: SelectableText(
                      (saved!['sources'] as List? ?? [])
                          .asMap()
                          .entries
                          .map(
                            (entry) =>
                                '${entry.key + 1} ← ${entry.value == -1 ? 'black' : (entry.value as int) + 1}',
                          )
                          .join('   '),
                    ),
                  ),
                ],
              ),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    super.build(context);
    final selectedPixel = selectedDiagnosticPixel,
        colors = Theme.of(context).colorScheme;
    final problem = !current
        ? 'Connection changed. Close and reopen this editor.'
        : error ??
              preview.error ??
              (!toggleSupported
                  ? 'Update board firmware to save disabled rules.'
                  : null);
    return Form(
      key: formKey,
      child: Padding(
        padding: const EdgeInsetsDirectional.fromSTEB(16, 8, 16, 0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              key: const ValueKey('correction-fixed-actions'),
              children: [
                const Icon(pixelCorrectionIcon, size: 22),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    'Corrections',
                    style: Theme.of(context).textTheme.titleMedium,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                  ),
                ),
                IconButton(
                  tooltip: 'Reload',
                  onPressed: busy || preview.pending || !current
                      ? null
                      : reload,
                  icon: const Icon(Icons.refresh_rounded, size: 20),
                ),
                IconButton(
                  tooltip: 'Wire mapping',
                  onPressed: saved == null ? null : showMapping,
                  icon: const Icon(Icons.account_tree_outlined, size: 20),
                ),
                const SizedBox(width: 4),
                FilledButton.icon(
                  onPressed: !editable || preview.pending
                      ? null
                      : () => load(save: true),
                  icon: const Icon(Icons.save_outlined, size: 18),
                  label: Text(
                    MediaQuery.sizeOf(context).width < 480
                        ? 'Save'
                        : 'Save to board',
                  ),
                  style: FilledButton.styleFrom(
                    padding: const EdgeInsets.symmetric(horizontal: 12),
                  ),
                ),
              ],
            ),
            SizedBox(
              height: 3,
              child: busy ? const LinearProgressIndicator() : null,
            ),
            Wrap(
              spacing: 8,
              runSpacing: 4,
              crossAxisAlignment: WrapCrossAlignment.center,
              children: [
                FilterChip(
                  showCheckmark: false,
                  avatar: const Icon(Icons.tune_rounded, size: 18),
                  label: const Text('Apply corrections'),
                  selected: enabled,
                  onSelected: !editable
                      ? null
                      : (value) => edit(() {
                          enabled = value;
                        }),
                ),
                if (previewSupported) boardPreviewControl(),
                if (current &&
                    widget.api.freshStatus?.peer.table == true &&
                    addressMapping == null)
                  Tooltip(
                    message:
                        _mappingError ??
                        'Read the current board’s logical cell map',
                    child: TextButton.icon(
                      key: const ValueKey('correction-map-retry'),
                      onPressed: _mappingPending
                          ? null
                          : () =>
                                unawaited(_hydrateAddressMapping(retry: true)),
                      icon: const Icon(Icons.map_outlined, size: 18),
                      label: Text(
                        _mappingPending
                            ? 'Loading cell map…'
                            : 'Retry cell map',
                      ),
                    ),
                  ),
                if (widget.workspace != null)
                  TextButton.icon(
                    key: const ValueKey('correction-use-cursor'),
                    onPressed:
                        editable &&
                            selectedPixel != null &&
                            (selectedRule != null || rules.length < maxRules)
                        ? () => importPixel(selectedPixel)
                        : null,
                    icon: const Icon(Icons.my_location_rounded, size: 18),
                    label: Text(
                      selectedPixel == null
                          ? 'Use selected cursor'
                          : 'Use selected cursor (${selectedPixel + 1})',
                    ),
                  ),
              ],
            ),
            if (problem != null)
              Padding(
                padding: const EdgeInsets.symmetric(vertical: 6),
                child: Row(
                  children: [
                    Icon(
                      Icons.warning_amber_rounded,
                      size: 18,
                      color: colors.error,
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        problem,
                        style: TextStyle(color: colors.error),
                      ),
                    ),
                  ],
                ),
              ),
            Row(
              children: [
                Expanded(
                  child: Text(
                    'Rules · ${rules.length}/$maxRules',
                    style: Theme.of(context).textTheme.labelLarge
                        ?.copyWith(color: colors.onSurfaceVariant),
                  ),
                ),
                Tooltip(
                  message: 'Rules run top to bottom. Addresses refer to the preceding rule’s output.',
                  child: Icon(
                    Icons.info_outline_rounded,
                    size: 17,
                    color: colors.onSurfaceVariant,
                  ),
                ),
                const SizedBox(width: 8),
                TextButton.icon(
                  onPressed: !editable || rules.length >= maxRules
                      ? null
                      : () => addRule(),
                  icon: const Icon(Icons.add_rounded, size: 18),
                  label: const Text('Add rule'),
                ),
              ],
            ),
            const SizedBox(height: 4),
            Expanded(
              child: Scrollbar(
                controller: rulesScroll,
                child: SingleChildScrollView(
                  key: const PageStorageKey('pixel-correction-rules'),
                  controller: rulesScroll,
                  padding: const EdgeInsets.only(bottom: 16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      for (var index = 0; index < rules.length; index++)
                        ruleCard(rules[index], index),
                      if (rules.isEmpty && saved != null)
                        const Padding(
                          padding: EdgeInsets.symmetric(vertical: 20),
                          child: Text(
                            'No corrections. Every wire address uses its original pixel.',
                          ),
                        ),
                    ],
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
