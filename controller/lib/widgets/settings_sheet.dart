import 'package:flutter/material.dart';

import '../models/car_connection.dart';

/// Bottom sheet for overriding the car address (default 192.168.4.1).
Future<void> showSettingsSheet(BuildContext context, CarConnection connection) {
  final controller = TextEditingController(text: connection.host);
  return showModalBottomSheet<void>(
    context: context,
    isScrollControlled: true,
    builder: (context) => Padding(
      padding: EdgeInsets.only(
        left: 24,
        right: 24,
        top: 24,
        bottom: MediaQuery.of(context).viewInsets.bottom + 24,
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text('Settings', style: Theme.of(context).textTheme.titleLarge),
          const SizedBox(height: 16),
          TextField(
            controller: controller,
            keyboardType: TextInputType.url,
            decoration: const InputDecoration(
              labelText: 'Car address',
              helperText:
                  'Default ${CarConnection.defaultHost} (the car\'s hotspot). '
                  'Only change this if you modified the firmware.',
              border: OutlineInputBorder(),
            ),
          ),
          const SizedBox(height: 16),
          Row(
            mainAxisAlignment: MainAxisAlignment.end,
            children: [
              TextButton(
                onPressed: () {
                  controller.text = CarConnection.defaultHost;
                },
                child: const Text('Reset to default'),
              ),
              const SizedBox(width: 8),
              FilledButton(
                onPressed: () {
                  connection.setHost(controller.text);
                  Navigator.of(context).pop();
                },
                child: const Text('Save'),
              ),
            ],
          ),
        ],
      ),
    ),
  );
}
