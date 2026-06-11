import 'package:flutter/material.dart';

import '../models/car_connection.dart';

/// Hold-to-boost button (the Game Boy "B" button). Hold sends T, release
/// sends N. A [Listener] keeps it multi-touch friendly next to the D-pad.
class TurboButton extends StatefulWidget {
  const TurboButton({super.key, required this.connection, this.size = 96});

  final CarConnection connection;
  final double size;

  @override
  State<TurboButton> createState() => _TurboButtonState();
}

class _TurboButtonState extends State<TurboButton> {
  bool _held = false;

  void _set(bool held) {
    if (_held == held) return;
    setState(() => _held = held);
    widget.connection.setTurbo(held);
  }

  @override
  Widget build(BuildContext context) {
    return Listener(
      onPointerDown: (_) => _set(true),
      onPointerUp: (_) => _set(false),
      onPointerCancel: (_) => _set(false),
      child: Container(
        width: widget.size,
        height: widget.size,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          color: _held ? const Color(0xFFE5484D) : const Color(0xFFA3282D),
          boxShadow: _held
              ? null
              : const [
                  BoxShadow(
                    color: Colors.black45,
                    offset: Offset(0, 4),
                    blurRadius: 4,
                  ),
                ],
        ),
        child: const Center(
          child: Text(
            'TURBO',
            style: TextStyle(
              color: Colors.white,
              fontWeight: FontWeight.w800,
              letterSpacing: 1.2,
              fontSize: 14,
            ),
          ),
        ),
      ),
    );
  }
}
