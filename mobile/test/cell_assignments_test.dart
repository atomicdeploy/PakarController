import 'dart:convert';
import 'dart:typed_data';

import 'package:archive/archive.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/cell_assignments.dart';
import 'package:pakarcontroller/command_readback.dart';
import 'package:pakarcontroller/command_wire.dart';
import 'package:pakarcontroller/models.dart';

import 'peer_fixture.dart';

CellAssignments sample() => CellAssignments.fromRows([
  {'cell': 80, 'state': 'off'},
  {'cell': 1, 'state': 'red'},
  {'cell': 15, 'state': 'white'},
]);

Uint8List changePart(Uint8List source, String path, String replacement) {
  final archive = ZipDecoder().decodeBytes(source);
  archive.addFile(ArchiveFile.string(path, replacement));
  return Uint8List.fromList(ZipEncoder().encode(archive)!);
}

DeviceStatus rolePeer(String role) {
  final json = peerFixture().json;
  json['board_id'] = 'test-board';
  json['boot_id'] = 'test-boot';
  json['command_clock_ms'] = 100;
  json['red_lock'] = false;
  json['capabilities']['role'] = role;
  json['capabilities']['acknowledged_features'] = [
    ...json['capabilities']['acknowledged_features'],
    'cell-lock',
    'cell-assignments',
    'command-ack',
  ];
  return DeviceStatus(json);
}

void main() {
  group('Semantic cell assignment documents', () {
    for (final format in ['json', 'csv', 'xlsx']) {
      test('$format roundtrip is sorted, partial and lossless', () {
        final cells = sample();
        final encoded = cells.encode(format);
        expect(encoded.length, lessThan(CellAssignments.maximumFileBytes));
        if (format == 'xlsx') expect(encoded.take(4), [80, 75, 3, 4]);
        final decoded = CellAssignments.decode('Cells.$format', encoded);
        expect(decoded.values, {1: 'lost', 15: 'idle', 80: 'off'});
        expect(decoded.values.keys.toList(), [1, 15, 80]);
        expect(() => decoded.values[3] = 'red', throwsUnsupportedError);
      });
    }
    test('Complete observed export never invents unavailable cell states', () {
      final cells = List.filled(80, 'white');
      expect(CellAssignments.fromObservedCells(cells).values.length, 80);
      for (final invalid in [
        null,
        cells.take(79).toList(),
        [...cells, 'red'],
        [...cells.take(79), 'unknown'],
      ]) {
        expect(
          () => CellAssignments.fromObservedCells(invalid),
          throwsFormatException,
        );
      }
    });
    test('Canonical file colors and JSON board role are validated', () {
      final document = CellAssignments.decode(
        'colors.JSON',
        Uint8List.fromList(
          utf8.encode(
            jsonEncode({
              'role': 'table-selection',
              'assignments': [
                {'cell': 1, 'state': '#FF0000'},
                {'cell': 2, 'state': ' WHITE '},
                {'cell': 3, 'state': '#000000'},
              ],
            }),
          ),
        ),
      );
      expect(document.values, {1: 'lost', 2: 'idle', 3: 'off'});
      expect(
        () => CellAssignments.decode(
          'colors.json',
          Uint8List.fromList(
            utf8.encode(
              '{"role":"first-wins","assignments":[{"cell":1,"state":"red"}]}',
            ),
          ),
        ),
        throwsFormatException,
      );
    });
    test(
      'Invalid IDs, duplicated IDs and arbitrary colors reject whole import',
      () {
        for (final bad in [
          [],
          [
            {'cell': 0, 'state': 'red'},
          ],
          [
            {'cell': 81, 'state': 'red'},
          ],
          [
            {'cell': 1.0, 'state': 'red'},
          ],
          [
            {'cell': '1', 'state': 'red'},
          ],
          [
            {'cell': 1, 'state': '#00ff00'},
          ],
          [
            {'cell': 1, 'state': 'red'},
            {'cell': 1, 'state': 'off'},
          ],
          [
            for (var i = 1; i <= 81; i++) {'cell': i, 'state': 'red'},
          ],
        ]) {
          expect(() => CellAssignments.fromRows(bad), throwsFormatException);
        }
      },
    );
    test('CSV quoting/BOM/CRLF normalize without running formulas', () {
      final cells = CellAssignments.decode(
        'cells.csv',
        Uint8List.fromList(
          utf8.encode('\ufeff"cell","state"\r\n"01","#ff0000"\r\n80,off\r\n'),
        ),
      );
      expect(cells.values, {1: 'lost', 80: 'off'});
      for (final text in [
        'cell,state\n1,red,off',
        'cell,state\n1,"red',
        'cell,state\n1,"red"junk',
        'cell,state\n=1,red',
        'cell,state\n1,=HYPERLINK("bad")',
        'id,state\n1,red',
        'cell,state\n1,${'a' * 65}',
      ]) {
        expect(
          () => CellAssignments.decode(
            'cells.csv',
            Uint8List.fromList(utf8.encode(text)),
          ),
          throwsFormatException,
        );
      }
    });
    test(
      'File size, empty files and unsupported suffixes fail before parsing',
      () {
        for (final bytes in [
          Uint8List(0),
          Uint8List(CellAssignments.maximumFileBytes + 1),
        ]) {
          expect(
            () => CellAssignments.decode('cells.xlsx', bytes),
            throwsFormatException,
          );
        }
        expect(
          () => CellAssignments.decode('cells.xls', Uint8List.fromList([1])),
          throwsFormatException,
        );
      },
    );
    test('Real XLSX shared strings and numeric IDs import', () {
      var bytes = sample().encode('xlsx');
      bytes = changePart(
        bytes,
        'xl/sharedStrings.xml',
        '<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><si><t>cell</t></si><si><t>state</t></si><si><t>#FF0000</t></si></sst>',
      );
      bytes = changePart(
        bytes,
        'xl/worksheets/sheet1.xml',
        '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData><row r="1"><c r="A1" t="s"><v>0</v></c><c r="B1" t="s"><v>1</v></c></row><row r="2"><c r="A2" t="n"><v>80</v></c><c r="B2" t="s"><v>2</v></c></row></sheetData></worksheet>',
      );
      expect(CellAssignments.decode('colors.xlsx', bytes).values, {80: 'lost'});
    });
    test('Workbook relationship resolves a custom sheet path', () {
      var bytes = sample().encode('xlsx');
      bytes = changePart(
        bytes,
        'xl/workbook.xml',
        '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="Colors" sheetId="1" r:id="colors"/></sheets></workbook>',
      );
      bytes = changePart(
        bytes,
        'xl/_rels/workbook.xml.rels',
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="colors" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="/xl/worksheets/sheet1.xml"/></Relationships>',
      );
      expect(
        CellAssignments.decode('colors.xlsx', bytes).values,
        sample().values,
      );
    });
    test(
      'XLSX formulas, entities, external refs and duplicate cells are rejected',
      () {
        final workbook = sample().encode('xlsx');
        for (final xml in [
          '<worksheet><c r="A2"><f>1+1</f><v>2</v></c></worksheet>',
          '<!DOCTYPE worksheet [<!ENTITY test "red">]><worksheet/>',
          '<worksheet><c r="A1"><v>cell</v></c><c r="A1"><v>cell</v></c></worksheet>',
          '<worksheet><c r="C1"><v>unsupported</v></c></worksheet>',
          '<worksheet><c r="A82"><v>81</v></c></worksheet>',
        ]) {
          expect(
            () => CellAssignments.decode(
              'cells.xlsx',
              changePart(workbook, 'xl/worksheets/sheet1.xml', xml),
            ),
            throwsFormatException,
          );
        }
        expect(
          () => CellAssignments.decode(
            'cells.xlsx',
            changePart(
              workbook,
              'xl/_rels/workbook.xml.rels',
              '<Relationships><Relationship TargetMode="External" Target="https://example.invalid"/></Relationships>',
            ),
          ),
          throwsFormatException,
        );
      },
    );
    test('ZIP CRC, encryption flag and expansion size fail closed', () {
      final source = sample().encode('xlsx');
      final eocd = source.length - 22;
      final offset = ByteData.sublistView(source)
          .getUint32(eocd + 16, Endian.little);
      for (final bad in [
        Uint8List.fromList(source)..[6] |= 1,
        Uint8List.fromList(source)..[offset + 8] |= 1,
        Uint8List.fromList(source)..[offset + 16] ^= 1,
        Uint8List.fromList(source)..[offset + 26] = 0xff,
      ]) {
        expect(
          () => CellAssignments.decode('cells.xlsx', bad),
          throwsFormatException,
        );
      }
    });
  });
  group('Role, lock and atomic command admission', () {
    test('Baked role filters modes and non-table controls', () {
      final table = rolePeer('table-selection').peer;
      expect(table.modes.map((e) => e.id), ['cell']);
      expect(table.allows('/api/game/reset', {}), false);
      expect(table.allows('/api/cells/lock', {'enabled': 'true'}), true);
      final game = rolePeer('first-wins').peer;
      expect(game.modes.map((e) => e.id), ['first-wins']);
      expect(game.has('cell-lock'), false);
      expect(game.has('cell-assignments'), false);
      expect(game.allows('/api/cells/assignments', {}), false);
      expect(game.allows('/api/cells', {'ids': '1', 'state': 'red'}), false);
      expect(game.allows('/api/game/reset', {}), true);
      final unknown = rolePeer('unsupported-role').peer;
      expect(unknown.modes, isEmpty);
      expect(unknown.allows('/api/cell', {'id': '1', 'state': 'red'}), false);
      expect(unknown.allows('/api/cells', {'ids': '1', 'state': 'red'}), false);
    });
    test(
      'Full assignment command is one typed array, same board/boot receipt',
      () {
        final status = rolePeer('table-selection');
        final values = {'assignments': sample().commandValue};
        final packet = commandPacket(status, '/api/cells/assignments', values);
        final raw = jsonDecode(packet);
        expect(raw['command'], 'cell-assignments');
        expect(raw['assignments'], isA<List>());
        final reply = <String, dynamic>{
          'type': 'command-result',
          'accepted': true,
          'request_id': raw['request_id'],
          'board_id': raw['board_id'],
          'boot_id': raw['boot_id'],
          'count': 3,
          'status': status.json,
        };
        expect(
          verifyCommandReply(
            reply,
            packet,
            '/api/cells/assignments',
            values,
          ).hostname,
          'ledboard',
        );
        reply['count'] = 2;
        expect(
          () => verifyCommandReply(
            reply,
            packet,
            '/api/cells/assignments',
            values,
          ),
          throwsFormatException,
        );
      },
    );
    test('Only observed cell colors and lock value verify a command', () {
      final status = rolePeer('table-selection');
      final values = {'assignments': sample().commandValue};
      expect(
        commandReadbackMatches(status, '/api/cells/assignments', values),
        false,
      );
      for (final entry in sample().values.entries) {
        status.json['cells'][entry.key - 1] = entry.value;
      }
      expect(
        commandReadbackMatches(status, '/api/cells/assignments', values),
        true,
      );
      expect(
        commandReadbackMatches(status, '/api/cells/lock', {'enabled': 'true'}),
        false,
      );
      status.json['red_lock'] = true;
      expect(
        commandReadbackMatches(status, '/api/cells/lock', {'enabled': 'true'}),
        true,
      );
      expect(
        jsonDecode(
          commandPacket(status, '/api/cells/lock', {'enabled': 'true'}),
        )['enabled'],
        true,
      );
    });
  });
}
