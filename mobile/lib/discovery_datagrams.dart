import 'dart:async';
import 'dart:io';

/// Bounded ownership for the donor discovery socket workflow. Android can
/// report send failures asynchronously on the socket stream; a try/catch around
/// send alone does not catch those errors. Retire the scan through its awaited
/// result, never the root zone, and close the exact owned socket on every path.
Future<void> collectDiscoveryDatagrams({
  required Stream<RawSocketEvent> events,
  required Datagram? Function() receive,
  required void Function() send,
  required void Function() close,
  required void Function(Datagram packet) onPacket,
  required Duration window,
}) async {
  final done = Completer<void>();
  Object? failure;
  StackTrace? failureStack;
  void finish([Object? error, StackTrace? stack]) {
    if (done.isCompleted) return;
    failure = error;
    failureStack = stack;
    // Complete normally first: no error future can escape before its await is
    // installed (including a synchronous send/receive callback in a fixture).
    done.complete();
  }

  StreamSubscription<RawSocketEvent>? subscription;
  Timer? timeout;
  try {
    subscription = events.listen(
      (event) {
        if (done.isCompleted || event != RawSocketEvent.read) return;
        try {
          Datagram? packet;
          while ((packet = receive()) != null) {
            onPacket(packet!);
          }
        } on Object catch (error, stack) {
          finish(error, stack);
        }
      },
      onError: (Object error, StackTrace stack) => finish(error, stack),
      onDone: finish,
    );
    timeout = Timer(window, finish);
    send();
    await done.future;
    if (failure != null) {
      Error.throwWithStackTrace(failure!, failureStack ?? StackTrace.current);
    }
  } finally {
    timeout?.cancel();
    try {
      await subscription?.cancel();
    } finally {
      close();
    }
  }
}
