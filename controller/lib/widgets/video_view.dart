import 'package:flutter/material.dart';
import 'package:flutter_mjpeg/flutter_mjpeg.dart';

import '../models/car_connection.dart';

/// Live MJPEG view with placeholder states for "camera off" and
/// "not connected".
class VideoView extends StatelessWidget {
  const VideoView({
    super.key,
    required this.connection,
    this.fit = BoxFit.contain,
  });

  final CarConnection connection;
  final BoxFit fit;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: connection,
      builder: (context, _) {
        if (!connection.isConnected) {
          return const _Placeholder(
            icon: Icons.wifi_off,
            label: 'Not connected',
          );
        }
        if (!connection.cameraOn) {
          return const _Placeholder(
            icon: Icons.videocam_off,
            label: 'Camera off',
          );
        }
        return ColoredBox(
          color: Colors.black,
          child: Mjpeg(
            // New widget per host/camera-on cycle so the stream re-attaches
            // cleanly after a toggle or address change.
            key: ValueKey('${connection.streamUrl}-${connection.cameraOn}'),
            isLive: true,
            fit: fit,
            stream: connection.streamUrl,
            timeout: const Duration(seconds: 6),
            loading: (context) =>
                const Center(child: CircularProgressIndicator()),
            error: (context, error, stack) => const _Placeholder(
              icon: Icons.videocam_off,
              label: 'No video signal',
            ),
          ),
        );
      },
    );
  }
}

class _Placeholder extends StatelessWidget {
  const _Placeholder({required this.icon, required this.label});

  final IconData icon;
  final String label;

  @override
  Widget build(BuildContext context) {
    return Container(
      color: Colors.black,
      child: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 48, color: Colors.white24),
            const SizedBox(height: 8),
            Text(label, style: const TextStyle(color: Colors.white38)),
          ],
        ),
      ),
    );
  }
}
