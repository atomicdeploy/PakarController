import 'package:flutter/material.dart';

import 'controller_dialog.dart';
import 'pixel_corrections_editor.dart';

/// Reuses the imported PCController phone/tablet dialog. Choosing an operation
/// creates an editor intent only; persistence and live preview remain explicit.
Future<PixelCorrectionRuleIntent?> showPixelCorrectionMenu(
  BuildContext context,
  int pixel,
) => showDialog<PixelCorrectionRuleIntent>(
  context: context,
  builder: (dialog) => ControllerDialog(
    title: 'Correct output slot ${pixel + 1}',
    icon: Icons.tune_rounded,
    maxWidth: 520,
    child: Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        for (final option in [
          (
            'black',
            0,
            Icons.lightbulb_outline,
            'Black out',
            'Keep this address; send black.',
          ),
          (
            'remove',
            0,
            Icons.hide_source_outlined,
            'Remove address',
            'Skip this value; move following values earlier.',
          ),
          (
            'shift',
            1,
            Icons.arrow_forward_rounded,
            'Shift forward',
            'Move this and following values one slot later.',
          ),
          (
            'shift',
            -1,
            Icons.arrow_back_rounded,
            'Shift backward',
            'Move this and following values one slot earlier.',
          ),
        ])
          ListTile(
            key: ValueKey('correction-menu-${option.$1}-${option.$2}'),
            contentPadding: const EdgeInsets.symmetric(horizontal: 4),
            leading: Icon(
              option.$3,
              color: Theme.of(dialog).colorScheme.primary,
            ),
            title: Text(option.$4),
            subtitle: Text(option.$5),
            trailing: const Icon(Icons.chevron_right, size: 20),
            onTap: () => Navigator.pop(
              dialog,
              PixelCorrectionRuleIntent(
                kind: option.$1,
                pixel: pixel,
                offset: option.$2,
              ),
            ),
          ),
      ],
    ),
  ),
);
