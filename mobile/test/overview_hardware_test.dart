import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pakarcontroller/main.dart';
import 'package:pakarcontroller/workspace_widgets.dart';
import 'peer_fixture.dart';

void main() {
  testWidgets('Overview actions navigate without sending board commands', (tester) async {
    String? destination;
    await tester.pumpWidget(MaterialApp(home:Scaffold(body:BoardOverview(
      status:peerFixture(),connected:true,navigate:(value)=>destination=value))));
    await tester.tap(find.text('Table controls'));
    expect(destination,'table');
    await tester.tap(find.text('Hardware'));
    expect(destination,'hardware');
  });
  testWidgets('Hardware and status remain bounded at phone width and large text', (tester) async {
    tester.view.physicalSize=const Size(360,800);tester.view.devicePixelRatio=1;
    addTearDown(tester.view.resetPhysicalSize);addTearDown(tester.view.resetDevicePixelRatio);
    await tester.pumpWidget(MaterialApp(builder:(context,child)=>MediaQuery(
      data:MediaQuery.of(context).copyWith(textScaler:const TextScaler.linear(1.8)),child:child!),
      home:Scaffold(body:Column(children:[Expanded(child:HardwarePage(status:peerFixture())),
        StatusBar(status:peerFixture(),message:'Command accepted and device state confirmed',onConnection:(){})]))));
    await tester.pumpAndSettle();expect(tester.takeException(),isNull);
    expect(find.text('Configure hardware'),findsOneWidget);
    expect(find.byTooltip('Connection settings'),findsOneWidget);
  });
  testWidgets('Connection choice uses icon and separate subtitle', (tester) async {
    await tester.pumpWidget(const MaterialApp(home:Scaffold(body:SizedBox(width:300,height:80,
      child:ConnectionChoice(title:'Network',detail:'Wi-Fi or Ethernet',icon:Icons.wifi)))));
    expect(find.text('Network'),findsOneWidget);expect(find.text('Wi-Fi or Ethernet'),findsOneWidget);
    expect(find.byIcon(Icons.wifi),findsOneWidget);expect(tester.takeException(),isNull);
  });
}
