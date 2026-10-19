// Show actionable public protocol reasons, never arbitrary server bodies,
// credentials, requested text, remote URLs or uploaded payloads in an error.
String commandFailureMessage({Object? error, int? httpStatus}) {
  const reasons = <String, String>{
    'capability_not_acknowledged':
        'This board has not acknowledged this feature',
    'unsupported_command': 'The board does not support this command',
    'command_rejected':
        'The board rejected this command; check its mode and maintenance state',
    'peer_changed': 'The selected board or its boot changed before dispatch',
    'command_expired': 'The command expired before the board could apply it',
    'request_id_conflict':
        'The board rejected conflicting content for this request ID',
    'command_capacity': 'The board command queue is full',
    'invalid_request_id': 'The board rejected the request identity',
    'invalid_command': 'The board rejected the command shape or values',
    'red_cells_locked': 'Red-cell lock prevents this change to white; unlock red cells before applying it',
    'invalid_cell_assignments': 'Cell assignments must contain unique cell IDs 1–80 and white, red or off states',
    'unsupported_board_role':
        'This operation is not part of the connected board’s baked-in role',
    'esp_update_in_progress': 'An ESP firmware update currently owns the board',
    'passive_uart_capture_active': 'UART diagnostics currently own the board',
    'pixel_diagnostic_busy':
        'The LED driver is busy or cannot publish the diagnostic frame',
    'audio_unavailable': 'The AVR buzzer is unavailable or maintenance owns it',
    'invalid_audio_cue': 'The AVR has not advertised this melody',
    'invalid_buzzer_values':
        'The buzzer frequency, duration or Stop selector is invalid',
    'avr_transport_unavailable':
        'The AVR UART transport did not accept the command',
  };
  final reason = error is String ? reasons[error] : null;
  final detail =
      reason ?? 'The device rejected the command without a recognized reason';
  return '${httpStatus == null ? '' : 'HTTP $httpStatus · '}$detail. Nothing was retried.';
}

class DeviceCommandRejected extends FormatException {
  DeviceCommandRejected({Object? error, int? httpStatus})
    : super(commandFailureMessage(error: error, httpStatus: httpStatus));
}
