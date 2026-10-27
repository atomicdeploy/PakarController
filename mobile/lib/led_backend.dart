/// Shared interpretation of the board's LED transmitter state. Animation mode
/// and pixel mapping remain independent from the physical transmitter.
const selectableLedBackends = ['gpio-static', 'rmt-dma'];

class LedBackendState {
  const LedBackendState._(
    this.active,
    this.requested,
    this.available,
    this.runtimeSwitch,
    this.pending,
    this.ready,
    this.error,
    this.saved,
    this.settingsError,
    this.handoffKnown,
  );

  final String active, requested;
  final List<String> available;
  final bool runtimeSwitch, pending, ready;
  final int error;
  final bool? saved;
  final String? settingsError;
  final bool handoffKnown;

  static LedBackendState? fromStatus(Map<String, dynamic> status) {
    final capabilities = status['capabilities'];
    final capability = capabilities is Map ? capabilities['led_output'] : null;
    final output = status['led_output'];
    final driver = output is Map ? output['driver'] : null;
    if (driver is! Map ||
        capability is! Map ||
        capability['runtime_switch'] is! bool ||
        capability['available_backends'] is! List ||
        driver['available_backends'] is! List ||
        !const [
          'none',
          'fastled',
          ...selectableLedBackends,
        ].contains(driver['backend'])) {
      return null;
    }
    final declared = capability['available_backends'];
    final compiled = driver['available_backends'];
    final available = declared is List
        ? selectableLedBackends
              .where(
                (backend) =>
                    declared.contains(backend) &&
                    (compiled is! List || compiled.contains(backend)),
              )
              .toList()
        : <String>[];
    final requested = driver['requested_backend'];
    final switchError = driver['switch_error'];
    final driverError = driver['error_code'];
    return LedBackendState._(
      driver['backend'] as String,
      selectableLedBackends.contains(requested) ? requested as String : '',
      List.unmodifiable(available),
      capability['runtime_switch'] == true && available.isNotEmpty,
      driver['switch_pending'] == true,
      driver['ready'] == true,
      switchError is int && switchError != 0
          ? switchError
          : driverError is int
          ? driverError
          : 0,
      output['backend_saved'] is bool ? output['backend_saved'] as bool : null,
      output['backend_settings_error'] == null ||
              output['backend_settings_error'] == false ||
              output['backend_settings_error'] == 0 ||
              output['backend_settings_error'] == ''
          ? null
          : '${output['backend_settings_error']}',
      driver['switch_pending'] is bool && driver['switch_error'] is int,
    );
  }

  bool canSelect(String backend) =>
      runtimeSwitch && handoffKnown && !pending && available.contains(backend);

  bool get failed =>
      !pending && (error != 0 || saved == false || settingsError != null);

  String get failureDescription =>
      'LED transmitter switch failed (${settingsError ?? (error != 0 ? error : 'not persisted')}); active: $active. No command was replayed.';

  bool matches(String backend) =>
      handoffKnown &&
      available.contains(backend) &&
      active == backend &&
      requested == backend &&
      !pending &&
      ready &&
      error == 0 &&
      saved == true &&
      settingsError == null;
}
