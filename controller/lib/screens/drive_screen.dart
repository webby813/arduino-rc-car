import 'package:flutter/material.dart';

import '../models/car_connection.dart';
import '../widgets/dpad.dart';
import '../widgets/status_bar.dart';
import '../widgets/turbo_button.dart';
import '../widgets/video_view.dart';

/// The controller screen. Portrait = Game Boy (video top half, controls
/// bottom half); landscape = PPSSPP (fullscreen video, overlay controls).
/// The same widget instances are reused across rotation (GlobalKeys) so the
/// stream and control state carry over.
class DriveScreen extends StatefulWidget {
  const DriveScreen({super.key, required this.connection});

  final CarConnection connection;

  @override
  State<DriveScreen> createState() => _DriveScreenState();
}

class _DriveScreenState extends State<DriveScreen> {
  // GlobalKeys let the video and controls move between the two layouts
  // without being rebuilt, so the MJPEG stream survives rotation.
  final GlobalKey _videoKey = GlobalKey();
  final GlobalKey _dpadKey = GlobalKey();
  final GlobalKey _turboKey = GlobalKey();

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF1B1B1F),
      body: OrientationBuilder(
        builder: (context, orientation) {
          final video = VideoView(
            key: _videoKey,
            connection: widget.connection,
            fit: orientation == Orientation.portrait
                ? BoxFit.contain
                : BoxFit.cover,
          );
          final dpad = DPad(key: _dpadKey, connection: widget.connection);
          final turbo = TurboButton(
            key: _turboKey,
            connection: widget.connection,
          );

          return orientation == Orientation.portrait
              ? _portrait(video, dpad, turbo)
              : _landscape(video, dpad, turbo);
        },
      ),
    );
  }

  /// Game Boy: video on the top half, controls on the bottom half.
  Widget _portrait(Widget video, Widget dpad, Widget turbo) {
    return SafeArea(
      child: Column(
        children: [
          Center(child: StatusBar(connection: widget.connection)),
          Expanded(
            child: ClipRRect(
              borderRadius: BorderRadius.circular(12),
              child: SizedBox.expand(child: video),
            ),
          ),
          Expanded(
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: Row(
                children: [
                  Expanded(flex: 5, child: Center(child: dpad)),
                  const SizedBox(width: 16),
                  Expanded(
                    flex: 4,
                    child: Align(
                      // Slightly raised, like a Game Boy's A/B cluster.
                      alignment: const Alignment(0, -0.3),
                      child: turbo,
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  /// PPSSPP: fullscreen video with translucent controls floating on top.
  Widget _landscape(Widget video, Widget dpad, Widget turbo) {
    return Stack(
      children: [
        Positioned.fill(child: video),
        Positioned(
          left: 24,
          bottom: 24,
          width: 180,
          child: Opacity(opacity: 0.55, child: dpad),
        ),
        Positioned(
          right: 32,
          bottom: 48,
          child: Opacity(opacity: 0.55, child: turbo),
        ),
        Positioned(
          top: 8,
          right: 16,
          child: SafeArea(
            child: StatusBar(connection: widget.connection, translucent: true),
          ),
        ),
      ],
    );
  }
}
