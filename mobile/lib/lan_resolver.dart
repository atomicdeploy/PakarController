import 'dart:async';
import 'dart:io';

/// Adapted from PCController internal/lanresolver/resolver.go: retain URL host,
/// Host header and TLS/SNI while dialing a cached successful address. Unlike
/// the donor, HTTP/auth errors never evict an address. An unreachable cached
/// socket does: re-resolve only after that route fails, never on a timer.
class LanResolver {
  LanResolver({
    Future<List<InternetAddress>> Function(String)? lookup,
    bool? nativeFirst,
    this.securityContext,
  }) : _lookup = lookup ?? InternetAddress.lookup,
       _nativeFirst = nativeFirst ?? lookup == null;
  final Future<List<InternetAddress>> Function(String) _lookup;
  final bool _nativeFirst;
  final SecurityContext? securityContext;
  final _cache = <String, InternetAddress>{};
  final _localRoutes = <String, InternetAddress>{};
  final _pending = <String, Future<List<InternetAddress>>>{};
  final _lookupFailures = <String, int>{};
  int lookupFailureRevision(String host) =>
      _lookupFailures[host.toLowerCase()] ?? 0;

  String _routeKey(Uri uri) =>
      '${uri.scheme.toLowerCase()}|${uri.host.toLowerCase()}|${uri.port}';

  /// Actual recent successful direct socket, never a preferred-interface guess.
  /// This observational cache cannot grant identity or command ownership.
  String? localAddressFor(Uri uri) => _localRoutes[_routeKey(uri)]?.address;
  String? connectedAddressFor(Uri uri) =>
      _cache[uri.host.toLowerCase()]?.address ??
      InternetAddress.tryParse(uri.host)?.address;

  /// Import only after an isolated same-board HTTP/TLS read probe succeeded.
  /// This affects TCP routing, never board/boot, credentials or capabilities.
  void rememberVerifiedAddress(Uri logical, InternetAddress address) {
    final key = logical.host.toLowerCase();
    if (_cache.length >= 64 && !_cache.containsKey(key)) {
      _cache.remove(_cache.keys.first);
    }
    _cache[key] = address;
  }

  void _rememberLocalAddress(Uri uri, Socket socket) {
    final key = _routeKey(uri);
    if (_localRoutes.length >= 64 && !_localRoutes.containsKey(key)) {
      _localRoutes.remove(_localRoutes.keys.first);
    }
    _localRoutes[key] = socket.address;
  }

  Future<List<InternetAddress>> _resolve(String host) async {
    final key = host.toLowerCase();
    if (_pending[key] case final pending?) return pending;
    final old = _cache[key];
    Future<List<InternetAddress>> work() async {
      try {
        final values = await _lookup(host).timeout(const Duration(seconds: 2));
        if (values.isEmpty) {
          throw const SocketException('Hostname lookup returned no address');
        }
        return values.take(2).toList(growable: false);
      } on SocketException {
        if (_cache[key] == old) _cache.remove(key);
        _lookupFailures[key] = lookupFailureRevision(key) + 1;
        throw HostLookupFailure(host);
      }
    }

    final pending = work();
    _pending[key] = pending;
    try {
      return await pending;
    } finally {
      _pending.remove(key);
    }
  }

  Future<T> connect<T>(
    String host,
    Future<T> Function(InternetAddress) dial, {
    bool Function()? isCancelled,
  }) async {
    final literal = InternetAddress.tryParse(host);
    if (literal != null) return dial(literal);
    final key = host.toLowerCase(), cached = _cache[host.toLowerCase()];
    if (cached != null) {
      try {
        return await dial(cached);
      } on SocketException {
        if (_cache[key] == cached) _cache.remove(key);
      } on TimeoutException {
        if (_cache[key] == cached) _cache.remove(key);
      }
    }
    if (isCancelled?.call() == true) {
      throw const SocketException('Connection cancelled');
    }
    final candidates = await _resolve(host);
    Object? last;
    for (final candidate in candidates) {
      if (isCancelled?.call() == true) {
        throw const SocketException('Connection cancelled');
      }
      try {
        final connection = await dial(candidate);
        if (!_cache.containsKey(key) && _cache.length >= 64) {
          _cache.remove(_cache.keys.first);
        }
        _cache[key] = candidate;
        return connection;
      } on Exception catch (error) {
        last = error;
      }
    }
    throw last ?? const SocketException('No reachable hostname address');
  }

  HttpClient createClient() => HttpClient(context: securityContext)
    ..connectionTimeout = const Duration(seconds: 6)
    ..findProxy = controllerProxyFor
    ..connectionFactory = (uri, proxyHost, proxyPort) {
      if (proxyHost != null) _localRoutes.remove(_routeKey(uri));
      final secure =
          proxyHost == null && (uri.scheme == 'https' || uri.scheme == 'wss');
      final port = proxyPort ?? (uri.hasPort ? uri.port : (secure ? 443 : 80));
      ConnectionTask<Socket>? active;
      Socket? connected;
      var cancelled = false;
      ConnectionTask<Socket> observedTask(
        Future<Socket> socket,
        void Function() cancel,
      ) {
        // HttpClient obtains the task asynchronously. Observe an immediate DNS
        // failure now so it cannot escape as an unhandled zone error before
        // HttpClient attaches. The ORIGINAL future still delivers that exact
        // failure to HttpClient; this is not fallback or error suppression.
        unawaited(
          socket.then<void>((_) {}, onError: (Object _, StackTrace _) {}),
        );
        return ConnectionTask.fromSocket(socket, cancel);
      }

      Future<Socket> dial(Object address) async {
        if (cancelled) throw const SocketException('Connection cancelled');
        // Dial the sticky numeric TCP route, then upgrade with the ORIGINAL
        // hostname. This preserves SNI/certificate checks without another DNS
        // lookup on every HTTPS/WSS connection. Never accept bad certificates.
        final task = await Socket.startConnect(address, port);
        active = task;
        if (cancelled) task.cancel();
        var socket = await task.socket.timeout(
          Duration(seconds: secure ? 5 : 2),
          onTimeout: () {
            task.cancel();
            throw TimeoutException('Connection deadline');
          },
        );
        if (cancelled) {
          socket.destroy();
          throw const SocketException('Connection cancelled');
        }
        connected = socket;
        if (secure) {
          try {
            socket = await SecureSocket.secure(
              socket,
              host: uri.host,
              context: securityContext,
            ).timeout(const Duration(seconds: 5));
            connected = socket;
          } on Object {
            socket.destroy();
            rethrow;
          }
          if (cancelled) {
            socket.destroy();
            throw const SocketException('Connection cancelled');
          }
        }
        if (proxyHost == null) _rememberLocalAddress(uri, socket);
        return socket;
      }

      final host = proxyHost ?? uri.host,
          key = (proxyHost ?? uri.host).toLowerCase();
      // Preserve the SDK's ConnectionTask/DNS timing and Happy Eyeballs path.
      // Do not put a two-second manual lookup ahead of native mDNS connection.
      if (_nativeFirst &&
          !_cache.containsKey(key) &&
          InternetAddress.tryParse(host) == null) {
        final native = secure
            ? SecureSocket.startConnect(host, port, context: securityContext)
            : Socket.startConnect(host, port);
        return native
            .then(
              (task) => observedTask(
                task.socket.then((socket) {
                  if (_cache.length >= 64 && !_cache.containsKey(key)) {
                    _cache.remove(_cache.keys.first);
                  }
                  _cache[key] = socket.remoteAddress;
                  if (proxyHost == null) _rememberLocalAddress(uri, socket);
                  return socket;
                }),
                task.cancel,
              ),
            )
            .onError<SocketException>((error, stack) {
              // Classify a genuine lookup failure for SSDP read-only recovery.
              // The request has not been sent while acquiring its socket.
              return observedTask(
                connect(host, dial, isCancelled: () => cancelled),
                () {
                  cancelled = true;
                  active?.cancel();
                  connected?.destroy();
                },
              );
            });
      }
      final result = connect(host, dial, isCancelled: () => cancelled);
      return Future.value(
        observedTask(result, () {
          cancelled = true;
          active?.cancel();
          connected?.destroy();
        }),
      );
    };
}

/// Actual PCController netpolicy/proxy.go LAN bypass ranges/suffixes, adapted
/// without altering process environment or public endpoint proxy preferences.
String controllerProxyFor(Uri uri, {Map<String, String>? environment}) =>
    localControllerHost(uri.host)
    ? 'DIRECT'
    : HttpClient.findProxyFromEnvironment(uri, environment: environment);

bool localControllerHost(String host) {
  final name = host.toLowerCase().replaceFirst(RegExp(r'\.$'), '');
  if (name == 'localhost' ||
      !name.contains('.') && !name.contains(':') ||
      const [
        '.home.arpa',
        '.internal',
        '.lan',
        '.local',
        '.localdomain',
        '.localhost',
      ].any(name.endsWith)) {
    return true;
  }
  final address = InternetAddress.tryParse(name);
  if (address == null) return false;
  if (address.isLoopback || address.isLinkLocal) return true;
  var bytes = address.rawAddress;
  if (bytes.length == 16 &&
      bytes.take(10).every((value) => value == 0) &&
      bytes[10] == 0xff &&
      bytes[11] == 0xff) {
    bytes = bytes.sublist(12);
  }
  if (bytes.length == 16) {
    return (bytes[0] & 0xfe) == 0xfc ||
        bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0xc0;
  }
  return bytes[0] == 10 ||
      bytes[0] == 127 ||
      bytes[0] == 169 && bytes[1] == 254 ||
      bytes[0] == 100 && bytes[1] >= 64 && bytes[1] <= 127 ||
      bytes[0] == 172 && bytes[1] >= 16 && bytes[1] <= 31 ||
      bytes[0] == 192 && bytes[1] == 168;
}

class HostLookupFailure extends SocketException {
  HostLookupFailure(String host) : super('Hostname lookup failed for $host');
}
