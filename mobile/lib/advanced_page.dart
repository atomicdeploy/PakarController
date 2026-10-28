import 'package:flutter/material.dart';

import 'api.dart';
import 'bridge_page.dart';
import 'models.dart';
import 'network_tools_page.dart';
import 'workspace_widgets.dart';

/// Advanced destinations stay out of the main game controls. The app bridge
/// and the board's own outbound connections are distinct owners.
class AdvancedPage extends StatelessWidget {
  const AdvancedPage({
    super.key,
    required this.api,
    required this.observed,
    required this.openSettings,
  });
  final PakarApi api;
  final DeviceStatus? Function() observed;
  final void Function(String section) openSettings;

  @override
  Widget build(BuildContext context) {
    void open(String title, Widget child) => Navigator.of(context).push<void>(
      MaterialPageRoute(
        builder: (_) => Scaffold(
          appBar: AppBar(title: Text(title)),
          body: child,
        ),
      ),
    );
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        const WorkspaceHeader(title: 'Advanced', icon: Icons.tune),
        const SizedBox(height: 16),
        WorkspaceSection(
          title: 'Board',
          icon: Icons.developer_board,
          children: [
            ListTile(
              leading: const Icon(Icons.schedule),
              title: const Text('Clock & network tools'),
              subtitle: const Text('NTP, HTTP time, public IP, DNS and ping'),
              trailing: const Icon(Icons.chevron_right),
              onTap: observed()?.peer.has('network-tools') == true
                  ? () => open(
                      'Clock & network tools',
                      NetworkToolsPage(api: api),
                    )
                  : null,
            ),
            ListTile(
              leading: const Icon(Icons.wifi),
              title: const Text('Wi-Fi & Ethernet'),
              subtitle: const Text(
                'Scan networks, station profiles and addressing',
              ),
              trailing: const Icon(Icons.chevron_right),
              onTap: () => openSettings('wifi'),
            ),
            ListTile(
              leading: const Icon(Icons.cloud_outlined),
              title: const Text('Remote connections'),
              subtitle: const Text('Board MQTT and WebSocket configuration'),
              trailing: const Icon(Icons.chevron_right),
              onTap: () => openSettings('mqtt'),
            ),
          ],
        ),
        const SizedBox(height: 16),
        WorkspaceSection(
          title: 'This application',
          icon: Icons.phone_android,
          children: [
            ListTile(
              leading: const Icon(Icons.hub_outlined),
              title: const Text('App bridge & repeater'),
              subtitle: const Text('Local applications and remote endpoints'),
              trailing: const Icon(Icons.chevron_right),
              onTap: () => open(
                'App bridge & repeater',
                BridgePage(api: api, observed: observed),
              ),
            ),
          ],
        ),
      ],
    );
  }
}
