import 'dart:math' as math;

import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

/// Client-only view state: telemetry and reconnects never reset the viewport.
class TableViewportController extends ChangeNotifier {
  double zoom = 1;
  Offset pan = Offset.zero;
  void fit() {
    zoom = 1;
    pan = Offset.zero;
    notifyListeners();
  }

  void change(double value, Offset offset) {
    if (!value.isFinite || !offset.dx.isFinite || !offset.dy.isFinite) return;
    final next = value.clamp(.5, 8).toDouble();
    if (zoom == next && pan == offset) return;
    zoom = next;
    pan = offset;
    notifyListeners();
  }
}

class TableViewport extends StatefulWidget {
  const TableViewport({
    super.key,
    required this.controller,
    required this.child,
    this.contentSize = const Size(960, 228),
  });
  final TableViewportController controller;
  final Widget child;
  final Size contentSize;
  @override
  State<TableViewport> createState() => _TableViewportState();
}

class _TableViewportState extends State<TableViewport> {
  double previousScale = 1;
  Offset previous = Offset.zero;
  bool gestureActive = false;
  void cancelGesture() => gestureActive = false;
  @override
  Widget build(BuildContext context) => ListenableBuilder(
    listenable: widget.controller,
    builder: (context, _) => LayoutBuilder(
      builder: (context, bounds) {
        final size = Size(
          bounds.maxWidth,
          (MediaQuery.sizeOf(context).height * .38).clamp(180, 420),
        );
        final fit = math.min(
          size.width / widget.contentSize.width,
          size.height / widget.contentSize.height,
        );
        final view = widget.controller;
        final touchPlatform = switch (Theme.of(context).platform) {
          TargetPlatform.android || TargetPlatform.iOS => true,
          _ => false,
        };
        final tablet =
            touchPlatform && MediaQuery.sizeOf(context).shortestSide >= 600;
        Offset origin(double zoom) => Offset(
          (size.width - widget.contentSize.width * fit * zoom) / 2,
          (size.height - widget.contentSize.height * fit * zoom) / 2,
        );
        Offset constrain(Offset pan, double zoom) {
          final limit = Offset(
            math.max(
              0,
              (widget.contentSize.width * fit * zoom + size.width) / 2 - 24,
            ),
            math.max(
              0,
              (widget.contentSize.height * fit * zoom + size.height) / 2 - 24,
            ),
          );
          return Offset(
            pan.dx.clamp(-limit.dx, limit.dx),
            pan.dy.clamp(-limit.dy, limit.dy),
          );
        }

        final visiblePan = constrain(view.pan, view.zoom);
        void zoomAt(double next, Offset from, Offset to) {
          next = next.clamp(.5, 8);
          // Pealayer timelineZoomAtPointer, extended to both axes and centered fit.
          final scene =
              (from - origin(view.zoom) - constrain(view.pan, view.zoom)) /
              (fit * view.zoom);
          final pan = to - scene * (fit * next) - origin(next);
          view.change(next, constrain(pan, next));
        }

        return Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            if (!tablet)
              Row(
                children: [
                  TextButton.icon(
                    onPressed: view.fit,
                    icon: const Icon(Icons.fit_screen),
                    label: const Text('Fit'),
                  ),
                  IconButton(
                    tooltip: 'Zoom out',
                    icon: const Icon(Icons.remove),
                    onPressed: () => zoomAt(
                      view.zoom / 1.25,
                      size.center(Offset.zero),
                      size.center(Offset.zero),
                    ),
                  ),
                  Text('${(view.zoom * 100).round()}%'),
                  IconButton(
                    tooltip: 'Zoom in',
                    icon: const Icon(Icons.add),
                    onPressed: () => zoomAt(
                      view.zoom * 1.25,
                      size.center(Offset.zero),
                      size.center(Offset.zero),
                    ),
                  ),
                ],
              ),
            SizedBox(
              width: size.width,
              height: size.height,
              child: ClipRect(
                child: Listener(
                  // PCController ActivitySwipe retires a cancelled pointer
                  // before recognizer completion can act on stale gesture state.
                  onPointerCancel: (_) => cancelGesture(),
                  onPointerSignal: (event) {
                    if (event is PointerScrollEvent) {
                      final zooming =
                          HardwareKeyboard.instance.isControlPressed ||
                          HardwareKeyboard.instance.isMetaPressed;
                      if (!zooming && view.zoom <= 1) return;
                      GestureBinding.instance.pointerSignalResolver.register(
                        event,
                        (_) {
                          if (zooming) {
                            zoomAt(
                              view.zoom *
                                  math.exp(-event.scrollDelta.dy * .002),
                              event.localPosition,
                              event.localPosition,
                            );
                          } else {
                            final delta =
                                HardwareKeyboard.instance.isShiftPressed
                                ? Offset(
                                    event.scrollDelta.dy,
                                    event.scrollDelta.dx,
                                  )
                                : event.scrollDelta;
                            view.change(
                              view.zoom,
                              constrain(
                                constrain(view.pan, view.zoom) - delta,
                                view.zoom,
                              ),
                            );
                          }
                        },
                      );
                    }
                  },
                  child: RawGestureDetector(
                    key: const Key('table-viewport-gestures'),
                    behavior: HitTestBehavior.opaque,
                    gestures: {
                      ScaleGestureRecognizer:
                          GestureRecognizerFactoryWithHandlers<
                            ScaleGestureRecognizer
                          >(() => ScaleGestureRecognizer(debugOwner: this), (
                            recognizer,
                          ) {
                            recognizer
                              // A zoomed canvas must claim panning before the
                              // enclosing page's vertical drag. At fit, retain
                              // normal page scrolling and child tap ownership.
                              ..gestureSettings = view.zoom > 1
                                  ? const DeviceGestureSettings(touchSlop: 4)
                                  : MediaQuery.maybeGestureSettingsOf(context)
                              ..onStart = (details) {
                                gestureActive = true;
                                previousScale = 1;
                                previous = details.localFocalPoint;
                              }
                              ..onUpdate = (details) {
                                if (!gestureActive ||
                                    !details.scale.isFinite ||
                                    details.scale <= 0) {
                                  return;
                                }
                                zoomAt(
                                  view.zoom * details.scale / previousScale,
                                  previous,
                                  details.localFocalPoint,
                                );
                                previousScale = details.scale;
                                previous = details.localFocalPoint;
                              }
                              ..onEnd = (_) => cancelGesture();
                          }),
                    },
                    child: Stack(
                      clipBehavior: Clip.none,
                      children: [
                        Positioned(
                          left: 0,
                          top: 0,
                          width: widget.contentSize.width,
                          height: widget.contentSize.height,
                          child: Transform(
                            alignment: Alignment.topLeft,
                            transform: Matrix4.identity()
                              ..translateByDouble(
                                origin(view.zoom).dx + visiblePan.dx,
                                origin(view.zoom).dy + visiblePan.dy,
                                0,
                                1,
                              )
                              ..scaleByDouble(
                                fit * view.zoom,
                                fit * view.zoom,
                                1,
                                1,
                              ),
                            child: widget.child,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ),
          ],
        );
      },
    ),
  );
}
