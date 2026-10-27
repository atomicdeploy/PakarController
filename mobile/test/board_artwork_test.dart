import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/peer_context.dart';
import 'package:pakarcontroller/board_artwork.dart';
import 'package:pakarcontroller/application_artwork.dart';
import 'dart:convert';

void main() {
  final app = {'id':'atomicdeploy.pakarcontroller','name':'Board','title':'Board',
    'icon_png_base64':applicationArtworkBase64};
  test('Acknowledged roles choose distinct cached artwork, not hostnames', () {
    final table=PeerApplication.parse(app,role:'table-selection')!;
    final keys=PeerApplication.parse(app,role:'first-wins')!;
    expect(identical(table.icon,boardArtwork['table-selection']),isTrue);
    expect(identical(keys.icon,boardArtwork['first-wins']),isTrue);
    expect(table.icon, isNot(orderedEquals(keys.icon!)));
    expect(PeerApplication.parse(app,role:'ledcontroller')!.icon,base64Decode(applicationArtworkBase64));
  });
  test('Board supplied valid custom artwork remains authoritative', () {
    final custom={...app,'icon_png_base64':base64Encode(boardArtwork['first-wins']!)};
    expect(PeerApplication.parse(custom,role:'table-selection')!.icon,boardArtwork['first-wins']);
  });
}
