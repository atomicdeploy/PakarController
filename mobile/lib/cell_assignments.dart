import 'dart:convert';
import 'dart:io' as io;
import 'dart:typed_data';

import 'package:archive/archive.dart';
import 'package:xml/xml.dart';

/// Semantic cell assignments, not streamed RGB or an implicit mapping change.
/// A whole document validates before a single board-side atomic command exists.
class CellAssignments {
  CellAssignments._(Map<int, String> values)
    : values = Map.unmodifiable(
        Map.fromEntries(
          values.entries.toList()..sort((a, b) => a.key.compareTo(b.key)),
        ),
      );

  static const maximumFileBytes = 512 * 1024;
  static const states = ['white', 'red', 'off'];
  final Map<int, String> values;
  List<Map<String, Object>> get assignments => [
    for (final entry in values.entries)
      {'cell': entry.key, 'state': entry.value},
  ];
  String get commandValue => jsonEncode(assignments);

  static String normalizeState(Object? value) {
    if (value is! String) throw const FormatException('Missing cell state');
    return switch (value.trim().toLowerCase()) {
      'white' || '#ffffff' => 'white',
      'red' || '#ff0000' => 'red',
      'off' || '#000000' => 'off',
      _ => throw const FormatException('Cell colors must be white, red or off'),
    };
  }

  static CellAssignments fromRows(Object? rows) {
    if (rows is! List || rows.isEmpty || rows.length > 80) {
      throw const FormatException('Provide between 1 and 80 cell assignments');
    }
    final values = <int, String>{};
    for (final row in rows) {
      if (row is! Map) throw const FormatException('Invalid assignment row');
      final id = row['cell'];
      if (id is! int || id < 1 || id > 80 || values.containsKey(id)) {
        throw const FormatException(
          'Cell IDs must be unique integers from 1 to 80',
        );
      }
      values[id] = normalizeState(row['state']);
    }
    return CellAssignments._(values);
  }

  /// Unknown or partial observations are never exported as invented white/off.
  static CellAssignments fromObservedCells(Object? cells) {
    if (cells is! List || cells.length != 80) {
      throw const FormatException(
        'A complete current 80-cell observation is required',
      );
    }
    return fromRows([
      for (var i = 0; i < cells.length; i++) {'cell': i + 1, 'state': cells[i]},
    ]);
  }

  static CellAssignments decode(String name, Uint8List bytes) {
    if (bytes.isEmpty || bytes.length > maximumFileBytes) {
      throw const FormatException('Cell document is empty or exceeds 512 KiB');
    }
    if (name.toLowerCase().endsWith('.xlsx')) {
      try {
        return _readXlsx(bytes);
      } on FormatException {
        rethrow;
      } on Object {
        throw const FormatException('Invalid or unsupported XLSX workbook');
      }
    }
    final text = utf8.decode(bytes).replaceFirst('\ufeff', '');
    if (name.toLowerCase().endsWith('.json')) {
      final document = jsonDecode(text);
      if (document is! Map ||
          (document.containsKey('role') &&
              document['role'] != 'table-selection')) {
        throw const FormatException(
          'Select a table-selection assignment document',
        );
      }
      return fromRows(document['assignments']);
    }
    if (name.toLowerCase().endsWith('.csv')) return _fromTable(_readCsv(text));
    throw const FormatException('Select a .json, .csv or .xlsx document');
  }

  Uint8List encode(String extension) => switch (extension) {
    'json' => Uint8List.fromList(
      utf8.encode(
        const JsonEncoder.withIndent('  ')
            .convert({'role': 'table-selection', 'assignments': assignments}),
      ),
    ),
    'csv' => Uint8List.fromList(
      utf8.encode(
        'cell,state\r\n${values.entries.map((e) => '${e.key},${e.value}').join('\r\n')}\r\n',
      ),
    ),
    'xlsx' => _writeXlsx(),
    _ => throw const FormatException('Unsupported cell export format'),
  };

  static CellAssignments _fromTable(List<List<String>> rows) {
    rows = rows
        .where((row) => row.any((value) => value.trim().isNotEmpty))
        .toList();
    if (rows.isEmpty ||
        rows.first.length != 2 ||
        rows.first[0].trim().toLowerCase() != 'cell' ||
        rows.first[1].trim().toLowerCase() != 'state') {
      throw const FormatException('The first row must contain cell,state');
    }
    return fromRows([
      for (final row in rows.skip(1))
        if (row.length == 2)
          {'cell': int.tryParse(row[0].trim()), 'state': row[1]}
        else
          throw const FormatException(
            'Every cell row must have exactly two columns',
          ),
    ]);
  }

  static List<List<String>> _readCsv(String text) {
    final rows = <List<String>>[], row = <String>[];
    var field = StringBuffer(), quoted = false, afterQuote = false;
    void finishField() {
      if (field.length > 64) {
        throw const FormatException('Cell field is too long');
      }
      row.add(field.toString());
      field = StringBuffer();
      afterQuote = false;
      if (row.length > 2) {
        throw const FormatException('Expected only cell,state columns');
      }
    }

    void finishRow() {
      finishField();
      rows.add(List.of(row));
      row.clear();
      if (rows.length > 82) throw const FormatException('Too many cell rows');
    }

    for (var i = 0; i < text.length; i++) {
      final c = text[i];
      if (quoted) {
        if (c == '"') {
          if (i + 1 < text.length && text[i + 1] == '"') {
            field.write('"');
            i++;
          } else {
            quoted = false;
            afterQuote = true;
          }
        } else {
          field.write(c);
        }
      } else if (c == ',') {
        finishField();
      } else if (c == '\r' || c == '\n') {
        if (c == '\r' && i + 1 < text.length && text[i + 1] == '\n') i++;
        finishRow();
      } else if (c == '"' && field.isEmpty && !afterQuote) {
        quoted = true;
      } else if (afterQuote || c == '"') {
        throw const FormatException('Invalid CSV quoting');
      } else {
        field.write(c);
      }
      if (field.length > 64) {
        throw const FormatException('Cell field is too long');
      }
    }
    if (quoted) throw const FormatException('Unclosed CSV quote');
    if (field.isNotEmpty || row.isNotEmpty || afterQuote) finishRow();
    return rows;
  }

  static const _sheetNs =
      'http://schemas.openxmlformats.org/spreadsheetml/2006/main';
  static const _relNs =
      'http://schemas.openxmlformats.org/package/2006/relationships';
  static const _officeRelNs =
      'http://schemas.openxmlformats.org/officeDocument/2006/relationships';

  Uint8List _writeXlsx() {
    final archive = Archive();
    void part(String path, String xml) =>
        archive.addFile(ArchiveFile.string(path, xml));
    part(
      '[Content_Types].xml',
      '<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/></Types>',
    );
    part(
      '_rels/.rels',
      '<Relationships xmlns="$_relNs"><Relationship Id="rId1" Type="$_officeRelNs/officeDocument" Target="xl/workbook.xml"/></Relationships>',
    );
    part(
      'xl/workbook.xml',
      '<workbook xmlns="$_sheetNs" xmlns:r="$_officeRelNs"><sheets><sheet name="Cells" sheetId="1" r:id="rId1"/></sheets></workbook>',
    );
    part(
      'xl/_rels/workbook.xml.rels',
      '<Relationships xmlns="$_relNs"><Relationship Id="rId1" Type="$_officeRelNs/worksheet" Target="worksheets/sheet1.xml"/></Relationships>',
    );
    final sheet = StringBuffer(
      '<worksheet xmlns="$_sheetNs"><cols><col min="1" max="1" width="12" customWidth="1"/><col min="2" max="2" width="16" customWidth="1"/></cols><sheetData><row r="1"><c r="A1" t="inlineStr"><is><t>cell</t></is></c><c r="B1" t="inlineStr"><is><t>state</t></is></c></row>',
    );
    var row = 2;
    for (final entry in values.entries) {
      sheet.write(
        '<row r="$row"><c r="A$row"><v>${entry.key}</v></c><c r="B$row" t="inlineStr"><is><t>${entry.value}</t></is></c></row>',
      );
      row++;
    }
    sheet.write('</sheetData><autoFilter ref="A1:B${row - 1}"/></worksheet>');
    part('xl/worksheets/sheet1.xml', sheet.toString());
    return Uint8List.fromList(ZipEncoder().encode(archive)!);
  }

  static CellAssignments _readXlsx(Uint8List bytes) {
    // ZIP parsing stays lazy; never ask archive to inflate untrusted content.
    // Check central-directory identity/count/size before even parsing entries.
    final data = ByteData.sublistView(bytes);
    var end = -1;
    for (var i = bytes.length - 22; i >= 0 && i >= bytes.length - 65557; i--) {
      if (data.getUint32(i, Endian.little) == 0x06054b50) {
        end = i;
        break;
      }
    }
    if (end < 0 ||
        data.getUint16(end + 4, Endian.little) != 0 ||
        data.getUint16(end + 6, Endian.little) != 0 ||
        data.getUint16(end + 8, Endian.little) !=
            data.getUint16(end + 10, Endian.little) ||
        data.getUint16(end + 10, Endian.little) > 96 ||
        end + 22 + data.getUint16(end + 20, Endian.little) != bytes.length) {
      throw const FormatException(
        'Select a bounded, unencrypted XLSX workbook',
      );
    }
    final count = data.getUint16(end + 10, Endian.little);
    var cursor = data.getUint32(end + 16, Endian.little), total = 0;
    final centralEnd = cursor + data.getUint32(end + 12, Endian.little);
    final paths = <String>{};
    final expectedCrcs = <String, int>{};
    for (var i = 0; i < count; i++) {
      if (cursor + 46 > end ||
          data.getUint32(cursor, Endian.little) != 0x02014b50) {
        throw const FormatException('Malformed XLSX directory');
      }
      final flags = data.getUint16(cursor + 8, Endian.little);
      final method = data.getUint16(cursor + 10, Endian.little);
      final size = data.getUint32(cursor + 24, Endian.little);
      final nameLength = data.getUint16(cursor + 28, Endian.little);
      final length =
          46 +
          nameLength +
          data.getUint16(cursor + 30, Endian.little) +
          data.getUint16(cursor + 32, Endian.little);
      final unixMode = data.getUint32(cursor + 38, Endian.little) >> 16;
      if ((flags & 1) != 0 ||
          ![0, 8].contains(method) ||
          size > 512 * 1024 ||
          (unixMode & 0xf000) == 0xa000 ||
          cursor + length > end ||
          data.getUint16(cursor + 34, Endian.little) != 0) {
        throw const FormatException(
          'Encrypted, oversized or unsupported XLSX entry',
        );
      }
      final path = utf8.decode(
        bytes.sublist(cursor + 46, cursor + 46 + nameLength),
      );
      // A contradictory local header must not bypass the checked central
      // directory (including encryption and alternate compression methods).
      final local = data.getUint32(cursor + 42, Endian.little);
      final compressed = data.getUint32(cursor + 20, Endian.little);
      if (local + 30 > end ||
          data.getUint32(local, Endian.little) != 0x04034b50 ||
          data.getUint16(local + 6, Endian.little) != flags ||
          data.getUint16(local + 8, Endian.little) != method ||
          local +
                  30 +
                  data.getUint16(local + 26, Endian.little) +
                  data.getUint16(local + 28, Endian.little) +
                  compressed >
              data.getUint32(end + 16, Endian.little)) {
        throw const FormatException('Contradictory XLSX file headers');
      }
      if (path.isEmpty ||
          path.contains('..') ||
          path.startsWith('/') ||
          path.contains('\\') ||
          !paths.add(path) ||
          path.toLowerCase().endsWith('.bin')) {
        throw const FormatException(
          'Unsafe, duplicated or macro workbook entry',
        );
      }
      total += size;
      expectedCrcs[path] = data.getUint32(cursor + 16, Endian.little);
      if (total > 2 * 1024 * 1024) {
        throw const FormatException('Expanded workbook exceeds 2 MiB');
      }
      cursor += length;
    }
    if (cursor != centralEnd || centralEnd > end) {
      throw const FormatException('Malformed XLSX directory bounds');
    }
    final archive = ZipDecoder().decodeBytes(bytes);
    final parts = <String, ArchiveFile>{
      for (final file in archive.files) file.name: file,
    };
    XmlDocument xml(String path) {
      final file = parts[path];
      if (file == null || !file.isFile || file.isSymbolicLink) {
        throw FormatException('Missing XLSX part: $path');
      }
      final raw = file.rawContent?.toUint8List();
      if (raw == null) throw const FormatException('Unreadable XLSX entry');
      final sink = _BoundedBytes(file.size);
      if (file.compressionType == ArchiveFile.DEFLATE) {
        final decoder = io.ZLibDecoder(raw: true).startChunkedConversion(sink);
        try {
          decoder.add(raw);
          decoder.close();
        } on Object {
          throw const FormatException(
            'Invalid or oversized compressed workbook entry',
          );
        }
      } else {
        sink.add(raw);
        sink.close();
      }
      final output = sink.bytes;
      if (output.length != file.size ||
          getCrc32(output) != file.crc32 ||
          getCrc32(output) != expectedCrcs[path]) {
        throw const FormatException('XLSX integrity check failed');
      }
      final source = utf8.decode(output);
      if (RegExp(
        r'<!DOCTYPE|<!ENTITY',
        caseSensitive: false,
      ).hasMatch(source)) {
        throw const FormatException(
          'Document type declarations are not allowed',
        );
      }
      return XmlDocument.parse(source);
    }

    final contentTypes = xml('[Content_Types].xml');
    if (!contentTypes
        .findAllElements('Override', namespace: '*')
        .any(
          (entry) =>
              entry.getAttribute('PartName') == '/xl/workbook.xml' &&
              entry.getAttribute('ContentType') == 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml',
        )) {
      throw const FormatException(
        'Select an ordinary .xlsx workbook without macros',
      );
    }
    final relationships = xml('xl/_rels/workbook.xml.rels');
    final targets = <String, String>{};
    for (final relation in relationships.findAllElements(
      'Relationship',
      namespace: '*',
    )) {
      if (relation.getAttribute('TargetMode') == 'External') {
        throw const FormatException(
          'External workbook relationships are not allowed',
        );
      }
      if (relation.getAttribute('Type') == '$_officeRelNs/worksheet') {
        final target = relation.getAttribute('Target') ?? '';
        final path = target.startsWith('/xl/')
            ? target.substring(1)
            : 'xl/$target';
        if (path.contains('..') || path.contains(':') || path.contains('\\')) {
          throw const FormatException('Invalid worksheet relationship');
        }
        targets[relation.getAttribute('Id') ?? ''] = path;
      }
    }
    final sheets = xml('xl/workbook.xml')
        .findAllElements('sheet', namespace: '*')
        .toList();
    if (sheets.isEmpty || sheets.length > 16) {
      throw const FormatException('Workbook has no supported sheet');
    }
    final chosen =
        sheets
            .where((s) => s.getAttribute('name')?.toLowerCase() == 'cells')
            .firstOrNull ??
        sheets.first;
    final sheetPath =
        targets[chosen.getAttribute('id', namespace: _officeRelNs)];
    if (sheetPath == null) {
      throw const FormatException('Worksheet relationship is missing');
    }
    final shared = <String>[];
    if (parts.containsKey('xl/sharedStrings.xml')) {
      for (final entry in xml(
        'xl/sharedStrings.xml',
      ).findAllElements('si', namespace: '*')) {
        final text = entry
            .findAllElements('t', namespace: '*')
            .map((node) => node.innerText)
            .join();
        if (shared.length >= 512 || text.length > 64) {
          throw const FormatException('Workbook text exceeds cell bounds');
        }
        shared.add(text);
      }
    }
    final rows = <int, Map<int, String>>{};
    final sheet = xml(sheetPath);
    if (sheet.findAllElements('f', namespace: '*').isNotEmpty) {
      throw const FormatException('Cell formulas are not allowed');
    }
    for (final cell in sheet.findAllElements('c', namespace: '*')) {
      final reference = RegExp(r'^([AB])([1-9][0-9]?)$')
          .firstMatch(cell.getAttribute('r') ?? '');
      if (reference == null) {
        if (cell.innerText.trim().isNotEmpty) {
          throw const FormatException(
            'Only columns A/B and 81 rows are supported',
          );
        }
        continue;
      }
      final row = int.parse(reference[2]!),
          column = reference[1] == 'A' ? 0 : 1;
      if (row > 81) throw const FormatException('Too many cell rows');
      final type = cell.getAttribute('t');
      final raw = cell.getElement('v', namespace: '*')?.innerText ?? '';
      final text = switch (type) {
        'inlineStr' =>
          cell
              .findAllElements('t', namespace: '*')
              .map((e) => e.innerText)
              .join(),
        's' =>
          shared.elementAtOrNull(int.tryParse(raw) ?? -1) ??
              (throw const FormatException('Invalid shared workbook string')),
        null || 'n' || 'str' => raw,
        _ => throw const FormatException('Unsupported workbook cell value'),
      };
      if (text.length > 64 || (rows[row] ??= {}).containsKey(column)) {
        throw const FormatException('Invalid or duplicated workbook cell');
      }
      rows[row]![column] = text;
    }
    final indices = rows.keys.toList()..sort();
    return _fromTable([
      for (final row in indices) [rows[row]![0] ?? '', rows[row]![1] ?? ''],
    ]);
  }
}

/// Fail while inflating, not after allocating an adversarial expansion.
class _BoundedBytes implements Sink<List<int>> {
  _BoundedBytes(this.maximum);
  final int maximum;
  final _builder = BytesBuilder();
  Uint8List get bytes => _builder.takeBytes();
  @override
  void add(List<int> data) {
    if (_builder.length + data.length > maximum) {
      throw const FormatException('Workbook expansion exceeds declared bound');
    }
    _builder.add(data);
  }

  @override
  void close() {}
}
