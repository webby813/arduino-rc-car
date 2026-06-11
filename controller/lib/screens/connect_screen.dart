import 'package:flutter/material.dart';

import '../models/car_connection.dart';
import '../widgets/settings_sheet.dart';

/// Shown while the car is unreachable: tells the user how to join the car's
/// hotspot. The root widget swaps to the drive screen automatically once the
/// control connection is up.
class ConnectScreen extends StatelessWidget {
  const ConnectScreen({super.key, required this.connection});

  final CarConnection connection;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('RC Car Controller'),
        actions: [
          IconButton(
            tooltip: 'Settings',
            onPressed: () => showSettingsSheet(context, connection),
            icon: const Icon(Icons.settings),
          ),
        ],
      ),
      body: Center(
        child: SingleChildScrollView(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 420),
            child: Padding(
              padding: const EdgeInsets.all(24),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Center(
                    child: Icon(
                      Icons.directions_car,
                      size: 64,
                      color: Colors.white38,
                    ),
                  ),
                  const SizedBox(height: 24),
                  const _Step(
                    number: '1',
                    text: 'Power on the car and wait a few seconds.',
                  ),
                  const _Step(
                    number: '2',
                    text:
                        'Open your phone\'s WiFi settings and join the hotspot '
                        '"${CarConnection.hotspotName}". If asked, choose to stay '
                        'connected even without internet.',
                  ),
                  const _Step(
                    number: '3',
                    text:
                        'Come back here — the app connects by itself. On iOS, '
                        'allow "Local Network" access if prompted (you can also '
                        'enable it later in Settings).',
                  ),
                  const SizedBox(height: 24),
                  ListenableBuilder(
                    listenable: connection,
                    builder: (context, _) => Row(
                      children: [
                        const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: Text(
                            'Looking for the car at ${connection.host}…',
                            style: const TextStyle(color: Colors.white54),
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _Step extends StatelessWidget {
  const _Step({required this.number, required this.text});

  final String number;
  final String text;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          CircleAvatar(
            radius: 12,
            backgroundColor: const Color(0xFF2E2E34),
            child: Text(number, style: const TextStyle(fontSize: 12)),
          ),
          const SizedBox(width: 12),
          Expanded(child: Text(text)),
        ],
      ),
    );
  }
}
