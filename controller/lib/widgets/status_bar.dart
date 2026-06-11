import 'package:flutter/material.dart';

import '../models/car_connection.dart';
import 'settings_sheet.dart';

/// Connection indicator + camera / headlight toggles + settings entry.
/// Used as the top bar in portrait and as a floating overlay in landscape.
class StatusBar extends StatelessWidget {
  const StatusBar({
    super.key,
    required this.connection,
    this.translucent = false,
  });

  final CarConnection connection;
  final bool translucent;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: connection,
      builder: (context, _) {
        final (color, label) = switch (connection.state) {
          CarConnectionState.connected => (Colors.greenAccent, 'Connected'),
          CarConnectionState.connecting => (Colors.orangeAccent, 'Connecting…'),
          CarConnectionState.disconnected => (Colors.redAccent, 'Disconnected'),
        };
        return Container(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
          decoration: BoxDecoration(
            color: translucent ? Colors.black38 : Colors.transparent,
            borderRadius: BorderRadius.circular(24),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(Icons.circle, size: 10, color: color),
              const SizedBox(width: 6),
              Text(label, style: const TextStyle(fontSize: 12)),
              const SizedBox(width: 8),
              IconButton(
                tooltip: 'Camera on/off',
                onPressed: connection.isConnected
                    ? connection.toggleCamera
                    : null,
                icon: Icon(
                  connection.cameraOn ? Icons.videocam : Icons.videocam_off,
                ),
              ),
              IconButton(
                tooltip: 'Headlight on/off',
                onPressed: connection.isConnected
                    ? connection.toggleHeadlight
                    : null,
                icon: Icon(
                  connection.headlightOn
                      ? Icons.flashlight_on
                      : Icons.flashlight_off,
                ),
              ),
              IconButton(
                tooltip: 'Settings',
                onPressed: () => showSettingsSheet(context, connection),
                icon: const Icon(Icons.settings),
              ),
            ],
          ),
        );
      },
    );
  }
}
