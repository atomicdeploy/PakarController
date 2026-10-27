import 'package:flutter/material.dart';

const pixelCorrectionIcon = Icons.tune;
const pixelDiagnosticIcon = Icons.center_focus_strong_outlined;

/// One icon gutter and one touch row, without nested ListTile padding.
PopupMenuItem<String> workspaceMenuItem(
  String value,
  String label,
  IconData icon, {
  bool enabled = true,
}) => PopupMenuItem<String>(
  value: value,
  enabled: enabled,
  height: 48,
  padding: const EdgeInsets.symmetric(horizontal: 16),
  child: Row(
    children: [
      Icon(icon, size: 20),
      const SizedBox(width: 12),
      Flexible(child: Text(label)),
    ],
  ),
);

class ConnectionChoice extends StatelessWidget {
  const ConnectionChoice({
    super.key,
    required this.title,
    required this.detail,
    required this.icon,
  });
  final String title, detail;
  final IconData icon;
  @override
  Widget build(BuildContext context) => Row(
    children: [
      Icon(icon, size: 22, color: Theme.of(context).colorScheme.primary),
      const SizedBox(width: 12),
      Expanded(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, maxLines: 1, overflow: TextOverflow.ellipsis),
            Text(
              detail,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: Theme.of(context).colorScheme.onSurfaceVariant,
              ),
            ),
          ],
        ),
      ),
    ],
  );
}

/// Shared Studio layout roles: Pealayer PreferencesTab's page header,
/// icon-labelled groups and bounded detail surface, adapted to Flutter.
/// All client pages/captures use these components rather than local palettes.
class WorkspaceHeader extends StatelessWidget {
  const WorkspaceHeader({
    super.key,
    required this.title,
    required this.icon,
    this.subtitle,
    this.actions = const [],
  });
  final String title;
  final String? subtitle;
  final IconData icon;
  final List<Widget> actions;
  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, bounds) {
      final titleBlock = Row(
        children: [
          Container(
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              color: Theme.of(context).colorScheme.surfaceContainerHigh,
              borderRadius: BorderRadius.circular(12),
            ),
            child: Icon(
              icon,
              color: Theme.of(context).colorScheme.primary,
              size: 24,
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: Theme.of(context).textTheme.headlineSmall
                      ?.copyWith(fontWeight: FontWeight.w600),
                ),
                if (subtitle?.isNotEmpty == true)
                  Text(
                    subtitle!,
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: Theme.of(context).colorScheme.onSurfaceVariant,
                    ),
                  ),
              ],
            ),
          ),
        ],
      );
      return Padding(
        padding: const EdgeInsets.only(bottom: 20),
        child:
            bounds.maxWidth < 480 ||
                MediaQuery.textScalerOf(context).scale(14) > 20
            ? Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  titleBlock,
                  if (actions.isNotEmpty) ...[
                    const SizedBox(height: 14),
                    Align(
                      alignment: AlignmentDirectional.centerEnd,
                      child: Wrap(
                        alignment: WrapAlignment.end,
                        crossAxisAlignment: WrapCrossAlignment.center,
                        spacing: 8,
                        runSpacing: 8,
                        children: actions,
                      ),
                    ),
                  ],
                ],
              )
            : Row(
                children: [
                  Expanded(child: titleBlock),
                  const SizedBox(width: 16),
                  if (actions.isNotEmpty)
                    Flexible(
                      fit: FlexFit.tight,
                      child: Wrap(
                        alignment: WrapAlignment.end,
                        crossAxisAlignment: WrapCrossAlignment.center,
                        spacing: 8,
                        runSpacing: 8,
                        children: actions,
                      ),
                    ),
                ],
              ),
      );
    },
  );
}

class WorkspaceSection extends StatelessWidget {
  const WorkspaceSection({
    super.key,
    required this.title,
    required this.children,
    this.icon,
    this.trailing,
  });
  final String title;
  final List<Widget> children;
  final IconData? icon;
  final Widget? trailing;
  @override
  Widget build(BuildContext context) => Card(
    margin: const EdgeInsets.only(bottom: 16),
    child: Padding(
      padding: const EdgeInsets.all(20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              if (icon != null) ...[
                Icon(
                  icon,
                  size: 20,
                  color: Theme.of(context).colorScheme.onSurfaceVariant,
                ),
                const SizedBox(width: 10),
              ],
              Expanded(
                child: Text(
                  title,
                  style: Theme.of(context).textTheme.titleSmall
                      ?.copyWith(fontWeight: FontWeight.w600),
                ),
              ),
              ?trailing,
            ],
          ),
          const Padding(
            padding: EdgeInsets.symmetric(vertical: 14),
            child: Divider(height: 1),
          ),
          ...children,
        ],
      ),
    ),
  );
}

class WorkspaceBody extends StatelessWidget {
  const WorkspaceBody({super.key, required this.children, this.controller});
  final List<Widget> children;
  final ScrollController? controller;
  @override
  Widget build(BuildContext context) => Align(
    alignment: Alignment.topCenter,
    child: ConstrainedBox(
      constraints: const BoxConstraints(maxWidth: 1120),
      child: ListView(
        controller: controller,
        padding: const EdgeInsets.all(20),
        children: children,
      ),
    ),
  );
}

class WorkspaceBadge extends StatelessWidget {
  const WorkspaceBadge(this.label, {super.key, this.active = false, this.icon});
  final String label;
  final bool active;
  final IconData? icon;
  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    final color = active ? colors.secondary : colors.onSurfaceVariant;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: BoxDecoration(
        color: colors.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(6),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (icon != null) ...[
            Icon(icon, size: 14, color: color),
            const SizedBox(width: 5),
          ],
          Flexible(
            child: Text(
              label,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: Theme.of(context).textTheme.labelSmall
                  ?.copyWith(color: color),
            ),
          ),
        ],
      ),
    );
  }
}

/// Content-sized tiles, not aspect-ratio cards: labels may wrap at large text
/// scales without clipping, while every row retains aligned column widths.
class WorkspaceGrid extends StatelessWidget {
  const WorkspaceGrid({
    super.key,
    required this.children,
    this.minimumWidth = 300,
  });
  final List<Widget> children;
  final double minimumWidth;
  @override
  Widget build(BuildContext context) => LayoutBuilder(
    builder: (context, bounds) {
      const gap = 12.0;
      final scale = MediaQuery.textScalerOf(context).scale(14) / 14;
      final minimum = minimumWidth * scale.clamp(1, 1.6);
      final columns = ((bounds.maxWidth + gap) / (minimum + gap)).floor().clamp(
        1,
        3,
      );
      final width = (bounds.maxWidth - gap * (columns - 1)) / columns;
      return Wrap(
        spacing: gap,
        runSpacing: gap,
        children: [
          for (final child in children) SizedBox(width: width, child: child),
        ],
      );
    },
  );
}

/// Adapts PCController ActivityStrip's real Material/InkWell composition,
/// restrained semantic icon surface, reserved border and directional affordance.
/// Navigation never issues a board command; an absent callback has no chevron.
class WorkspaceActionCard extends StatelessWidget {
  const WorkspaceActionCard({
    super.key,
    required this.title,
    required this.icon,
    this.subtitle,
    this.value,
    this.tone,
    this.onTap,
  });
  final String title;
  final String? subtitle, value;
  final IconData icon;
  final Color? tone;
  final VoidCallback? onTap;
  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    final ink = tone ?? colors.primary;
    final shape = RoundedRectangleBorder(
      borderRadius: BorderRadius.circular(14),
      side: BorderSide(color: colors.outlineVariant.withValues(alpha: .36)),
    );
    return Material(
      color: colors.surfaceContainerLow,
      shape: shape,
      clipBehavior: Clip.antiAlias,
      child: InkWell(
        customBorder: shape,
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Container(
                padding: const EdgeInsets.all(9),
                decoration: BoxDecoration(
                  color: ink.withValues(alpha: .10),
                  borderRadius: BorderRadius.circular(10),
                ),
                child: Icon(icon, color: ink, size: 22),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Text(
                      title,
                      style: Theme.of(context).textTheme.titleSmall
                          ?.copyWith(fontWeight: FontWeight.w600),
                    ),
                    if (value != null) ...[
                      const SizedBox(height: 6),
                      Text(
                        value!,
                        style: Theme.of(context).textTheme.titleMedium
                            ?.copyWith(color: ink, fontWeight: FontWeight.w600),
                      ),
                    ],
                    if (subtitle?.isNotEmpty == true) ...[
                      const SizedBox(height: 4),
                      Text(
                        subtitle!,
                        style: Theme.of(context).textTheme.bodySmall
                            ?.copyWith(color: colors.onSurfaceVariant),
                      ),
                    ],
                  ],
                ),
              ),
              if (onTap != null) ...[
                const SizedBox(width: 8),
                Icon(
                  Directionality.of(context) == TextDirection.rtl
                      ? Icons.chevron_left
                      : Icons.chevron_right,
                  color: colors.onSurfaceVariant,
                  size: 20,
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

class WorkspaceValueRow extends StatelessWidget {
  const WorkspaceValueRow({
    super.key,
    required this.label,
    required this.value,
  });
  final String label, value;
  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(vertical: 7),
    child: LayoutBuilder(
      builder: (context, bounds) {
        final labelWidget = Text(
          label,
          style: Theme.of(context).textTheme.bodySmall
              ?.copyWith(color: Theme.of(context).colorScheme.onSurfaceVariant),
        );
        final valueWidget = SelectableText(
          value,
          style: Theme.of(context).textTheme.bodyMedium,
        );
        return bounds.maxWidth < 320 ||
                MediaQuery.textScalerOf(context).scale(14) > 21
            ? Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [labelWidget, const SizedBox(height: 3), valueWidget],
              )
            : Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Expanded(child: labelWidget),
                  const SizedBox(width: 16),
                  Expanded(flex: 2, child: valueWidget),
                ],
              );
      },
    ),
  );
}
