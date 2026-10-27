import 'package:flutter/material.dart';

import 'icons.dart';
import 'led_backend.dart';

/// Adapt Pealayer PreferencesTab's contract-supplied select options and
/// field-scoped saving state. The selected value is observed hardware state,
/// never an optimistic copy of the requested driver.
class LedBackendControl extends StatelessWidget {
  const LedBackendControl({
    super.key,
    required this.state,
    required this.enabled,
    required this.onChanged,
  });
  final LedBackendState state;
  final bool enabled;
  final ValueChanged<String> onChanged;

  @override
  Widget build(BuildContext context) {
    final fa = Localizations.maybeLocaleOf(context)?.languageCode == 'fa';
    String text(String en, String faText) => fa ? faText : en;
    String label(String value) => switch (value) {
      'gpio-static' => text('GPIO · simple', 'GPIO · ساده'),
      'rmt-dma' => 'RMT + DMA',
      'none' => text('No active transmitter', 'فرستنده فعال نیست'),
      'fastled' => 'FastLED',
      _ => text('Unknown', 'نامشخص'),
    };
    final theme = Theme.of(context);
    final failed = state.failed;
    final description = state.pending
        ? text(
            'Switching to ${label(state.requested)}…',
            'در حال تغییر به ${label(state.requested)}…',
          )
        : state.settingsError != null
        ? text(
            'Settings were not saved: ${state.settingsError}',
            'تنظیمات ذخیره نشد: ${state.settingsError}',
          )
        : state.error != 0
        ? text(
            'Transmitter error ${state.error}',
            'خطای فرستنده ${state.error}',
          )
        : state.saved == false
        ? text('Transmitter setting was not saved', 'تنظیم فرستنده ذخیره نشد')
        : !state.ready
        ? text('Transmitter not ready', 'فرستنده آماده نیست')
        : text(
            'Active: ${label(state.active)}',
            'فعال: ${label(state.active)}',
          );
    return Padding(
      padding: const EdgeInsets.only(top: 8, bottom: 12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          InputDecorator(
            decoration: InputDecoration(
              labelText: text('LED transmitter', 'فرستنده LED'),
              prefixIcon: const Icon(PakarIcons.circuitry, size: 20),
              isDense: true,
              enabled:
                  enabled &&
                  !state.pending &&
                  state.runtimeSwitch &&
                  state.handoffKnown,
              contentPadding: const EdgeInsets.symmetric(
                horizontal: 12,
                vertical: 8,
              ),
            ),
            child: DropdownButtonHideUnderline(
              child: DropdownButton<String>(
                key: const ValueKey('led-backend-selector'),
                value: state.available.contains(state.active)
                    ? state.active
                    : null,
                hint: Text(label(state.active)),
                isExpanded: true,
                isDense: true,
                items: [
                  for (final backend in state.available)
                    DropdownMenuItem(
                      value: backend,
                      child: Text(label(backend)),
                    ),
                ],
                onChanged:
                    enabled &&
                        !state.pending &&
                        state.runtimeSwitch &&
                        state.handoffKnown
                    ? (value) {
                        if (value != null &&
                            state.canSelect(value) &&
                            (value != state.active || failed || !state.ready)) {
                          onChanged(value);
                        }
                      }
                    : null,
              ),
            ),
          ),
          const SizedBox(height: 6),
          Semantics(
            liveRegion: true,
            child: Row(
              children: [
                if (state.pending)
                  const SizedBox.square(
                    dimension: 14,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                else
                  Icon(
                    failed ? Icons.error_outline : Icons.memory_outlined,
                    size: 16,
                    color: failed
                        ? theme.colorScheme.error
                        : theme.colorScheme.onSurfaceVariant,
                  ),
                const SizedBox(width: 6),
                Expanded(
                  child: Text(
                    description,
                    key: const ValueKey('led-backend-state'),
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: failed
                          ? theme.colorScheme.error
                          : theme.colorScheme.onSurfaceVariant,
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
