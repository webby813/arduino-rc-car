import 'dart:async';

import 'package:flutter/material.dart';

import 'models/car_connection.dart';
import 'screens/connect_screen.dart';
import 'screens/drive_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  final connection = CarConnection()..init();
  runApp(RcCarApp(connection: connection));
}

class RcCarApp extends StatefulWidget {
  const RcCarApp({super.key, required this.connection});

  final CarConnection connection;

  @override
  State<RcCarApp> createState() => _RcCarAppState();
}

class _RcCarAppState extends State<RcCarApp> with WidgetsBindingObserver {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  // Safety: never keep driving while the app is not in the foreground.
  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    switch (state) {
      case AppLifecycleState.inactive:
      case AppLifecycleState.paused:
      case AppLifecycleState.hidden:
      case AppLifecycleState.detached:
        widget.connection.suspend();
      case AppLifecycleState.resumed:
        widget.connection.resume();
    }
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'RC Car Controller',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFFE5484D),
          brightness: Brightness.dark,
        ),
        scaffoldBackgroundColor: const Color(0xFF1B1B1F),
        useMaterial3: true,
      ),
      home: HomeRouter(connection: widget.connection),
    );
  }
}

/// Shows the drive screen while connected. A short connection blip keeps the
/// drive screen up (its status dot reports the state); only a sustained loss
/// falls back to the join-the-hotspot guidance screen.
class HomeRouter extends StatefulWidget {
  const HomeRouter({super.key, required this.connection});

  final CarConnection connection;

  @override
  State<HomeRouter> createState() => _HomeRouterState();
}

class _HomeRouterState extends State<HomeRouter> {
  static const Duration _gracePeriod = Duration(seconds: 6);

  bool _inGrace = false;
  bool _wasConnected = false;
  Timer? _graceTimer;

  @override
  void initState() {
    super.initState();
    widget.connection.addListener(_onConnectionChanged);
  }

  @override
  void dispose() {
    widget.connection.removeListener(_onConnectionChanged);
    _graceTimer?.cancel();
    super.dispose();
  }

  void _onConnectionChanged() {
    final connected = widget.connection.isConnected;
    if (connected) {
      _graceTimer?.cancel();
      _graceTimer = null;
      if (_inGrace || !_wasConnected) {
        setState(() {
          _inGrace = false;
          _wasConnected = true;
        });
      }
      _wasConnected = true;
    } else if (_wasConnected && _graceTimer == null) {
      setState(() => _inGrace = true);
      _graceTimer = Timer(_gracePeriod, () {
        _graceTimer = null;
        setState(() {
          _inGrace = false;
          _wasConnected = false;
        });
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final showDrive = widget.connection.isConnected || _inGrace;
    return showDrive
        ? DriveScreen(connection: widget.connection)
        : ConnectScreen(connection: widget.connection);
  }
}
