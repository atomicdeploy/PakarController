import 'package:flutter/material.dart';

import 'icons.dart';

enum HeaderAction {
  connection('Connection', Icons.link),
  refresh('Refresh stale state', PakarIcons.arrowClockwise),
  devices('Devices', PakarIcons.broadcast),
  settings('Board settings', PakarIcons.gear);

  const HeaderAction(this.label, this.icon);
  final String label;
  final IconData icon;
}

/// Local chrome preferences only: no transport or board state is stored here.
class HeaderLayout {
  HeaderLayout({List<HeaderAction>? order, Set<HeaderAction>? enabled})
    : order = List.unmodifiable(order ?? HeaderAction.values),
      enabled = Set.unmodifiable(
        enabled ?? {HeaderAction.connection, HeaderAction.refresh},
      );

  factory HeaderLayout.restore(Object? value) {
    if (value is! Map) return HeaderLayout();
    final byName = {
      for (final action in HeaderAction.values) action.name: action,
    };
    final order = <HeaderAction>[];
    if (value['order'] case final List saved) {
      for (final name in saved.take(32)) {
        final action = byName[name];
        if (action != null && !order.contains(action)) order.add(action);
      }
    }
    order.addAll(
      HeaderAction.values.where((action) => !order.contains(action)),
    );
    return HeaderLayout(
      order: order,
      enabled: value['enabled'] is List
          ? {
              for (final name in (value['enabled'] as List).take(32))
                ?byName[name],
            }
          : null,
    );
  }

  final List<HeaderAction> order;
  final Set<HeaderAction> enabled;
  Map<String, dynamic> snapshot() => {
    'order': order.map((action) => action.name).toList(),
    'enabled': order
        .where(enabled.contains)
        .map((action) => action.name)
        .toList(),
  };
  HeaderLayout toggle(HeaderAction action, bool visible) => HeaderLayout(
    order: order,
    enabled: {...enabled}
      ..remove(action)
      ..addAll(visible ? [action] : []),
  );
  HeaderLayout reorder(int oldIndex, int newIndex) {
    final next = [...order];
    if (newIndex > oldIndex) newIndex--;
    next.insert(newIndex, next.removeAt(oldIndex));
    return HeaderLayout(order: next, enabled: enabled);
  }

  List<HeaderAction> available({
    required bool wanted,
    required bool fresh,
    required bool settingsAvailable,
  }) => order
      .where(
        (action) =>
            enabled.contains(action) &&
            switch (action) {
              HeaderAction.refresh => wanted && !fresh,
              HeaderAction.settings => settingsAvailable,
              _ => true,
            },
      )
      .toList();
}

String connectionActionLabel({required bool wanted, required bool fresh}) =>
    !wanted
    ? 'Connect'
    : fresh
    ? 'Disconnect'
    : 'Cancel connection';
IconData connectionActionIcon({required bool wanted, required bool fresh}) =>
    !wanted
    ? Icons.link
    : fresh
    ? Icons.link_off
    : Icons.close;

/// Adapts donor workspace Material/InkWell semantics and existing OutputPanel
/// reorder handles. The overflow anchor is deliberately never removable.
class HeaderLayoutEditor extends StatefulWidget {
  const HeaderLayoutEditor({super.key, required this.initial});
  final HeaderLayout initial;
  @override
  State<HeaderLayoutEditor> createState() => _HeaderLayoutEditorState();
}

class _HeaderLayoutEditorState extends State<HeaderLayoutEditor> {
  late HeaderLayout draft = widget.initial;
  @override
  Widget build(BuildContext context) => AlertDialog(
    title: const Text('Configure header'),
    content: SizedBox(
      width: 460,
      height: (MediaQuery.sizeOf(context).height * .5).clamp(160.0, 360.0),
      child: ReorderableListView.builder(
        buildDefaultDragHandles: false,
        itemCount: draft.order.length,
        onReorderItem: (oldIndex, newIndex) => setState(
          () => draft = draft.reorder(
            oldIndex,
            newIndex > oldIndex ? newIndex + 1 : newIndex,
          ),
        ),
        itemBuilder: (context, index) {
          final action = draft.order[index];
          final choice = CheckboxListTile(
            contentPadding: EdgeInsets.zero,
            controlAffinity: ListTileControlAffinity.leading,
            title: Text(action.label),
            value: draft.enabled.contains(action),
            onChanged: (value) =>
                setState(() => draft = draft.toggle(action, value == true)),
          );
          final handles = Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              PopupMenuButton<int>(
                tooltip: 'Move ${action.label}',
                icon: const Icon(Icons.swap_vert),
                onSelected: (direction) => setState(
                  () => draft = draft.reorder(
                    index,
                    direction < 0 ? index - 1 : index + 2,
                  ),
                ),
                itemBuilder: (_) => [
                  if (index > 0)
                    const PopupMenuItem(value: -1, child: Text('Move up')),
                  if (index < draft.order.length - 1)
                    const PopupMenuItem(value: 1, child: Text('Move down')),
                ],
              ),
              ReorderableDragStartListener(
                index: index,
                child: const SizedBox(
                  width: 48,
                  height: 48,
                  child: Tooltip(
                    message: 'Drag to reorder',
                    child: Icon(Icons.drag_indicator),
                  ),
                ),
              ),
            ],
          );
          return LayoutBuilder(
            key: ValueKey(action.name),
            builder: (context, constraints) => Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                if (constraints.maxWidth >= 340)
                  Row(
                    children: [
                      Expanded(child: choice),
                      handles,
                    ],
                  )
                else ...[
                  choice,
                  Align(
                    alignment: AlignmentDirectional.centerEnd,
                    child: handles,
                  ),
                ],
                const Divider(height: 1),
              ],
            ),
          );
        },
      ),
    ),
    actions: [
      TextButton(
        onPressed: () => setState(() => draft = HeaderLayout()),
        child: const Text('Defaults'),
      ),
      TextButton(
        onPressed: () => Navigator.pop(context),
        child: const Text('Cancel'),
      ),
      FilledButton(
        onPressed: () => Navigator.pop(context, draft),
        child: const Text('Save'),
      ),
    ],
  );
}
