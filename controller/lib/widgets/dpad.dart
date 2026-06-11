import 'package:flutter/material.dart';

import '../models/car_connection.dart';

/// Game Boy style D-pad. Uses raw pointer events ([Listener]) instead of
/// gestures so press/release timing is exact and it works alongside the
/// turbo button in true multi-touch.
class DPad extends StatefulWidget {
  const DPad({super.key, required this.connection});

  final CarConnection connection;

  @override
  State<DPad> createState() => _DPadState();
}

class _DPadState extends State<DPad> {
  String? _pressed;

  void _press(String command) {
    setState(() => _pressed = command);
    widget.connection.drive(command);
  }

  void _release(String command) {
    // Only stop if this direction is still the active one (a second finger
    // may have taken over another direction in the meantime).
    if (_pressed == command) {
      setState(() => _pressed = null);
      widget.connection.stop();
    }
  }

  @override
  Widget build(BuildContext context) {
    return AspectRatio(
      aspectRatio: 1,
      child: Column(
        children: [
          Expanded(
            child: Row(
              children: [
                const Spacer(),
                _key('F', Icons.keyboard_arrow_up),
                const Spacer(),
              ],
            ),
          ),
          Expanded(
            child: Row(
              children: [
                _key('L', Icons.keyboard_arrow_left),
                const _DPadCenter(),
                _key('R', Icons.keyboard_arrow_right),
              ],
            ),
          ),
          Expanded(
            child: Row(
              children: [
                const Spacer(),
                _key('B', Icons.keyboard_arrow_down),
                const Spacer(),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _key(String command, IconData icon) {
    final bool active = _pressed == command;
    return Expanded(
      child: Listener(
        onPointerDown: (_) => _press(command),
        onPointerUp: (_) => _release(command),
        onPointerCancel: (_) => _release(command),
        child: Container(
          decoration: BoxDecoration(
            color: active ? const Color(0xFF4A4A52) : const Color(0xFF2E2E34),
            borderRadius: BorderRadius.circular(10),
            boxShadow: active
                ? null
                : const [
                    BoxShadow(
                      color: Colors.black45,
                      offset: Offset(0, 3),
                      blurRadius: 3,
                    ),
                  ],
          ),
          child: Icon(icon, size: 36, color: Colors.white70),
        ),
      ),
    );
  }
}

class _DPadCenter extends StatelessWidget {
  const _DPadCenter();

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Container(
        color: const Color(0xFF2E2E34),
        child: Center(
          child: Container(
            width: 22,
            height: 22,
            decoration: const BoxDecoration(
              color: Color(0xFF1C1C20),
              shape: BoxShape.circle,
            ),
          ),
        ),
      ),
    );
  }
}
