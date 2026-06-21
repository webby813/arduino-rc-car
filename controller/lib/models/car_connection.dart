import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

enum CarConnectionState { disconnected, connecting, connected }

/// Owns the WebSocket control link to the car: connect/retry, the 250 ms
/// heartbeat the firmware's failsafe expects, and send helpers for every
/// control message (drive, turbo, camera, headlight).
class CarConnection extends ChangeNotifier {
  static const String defaultHost = '192.168.4.1';
  static const String hotspotName = 'RC-CAR';
  static const Duration _heartbeatPeriod = Duration(milliseconds: 150);
  static const Duration _retryDelay = Duration(milliseconds: 700);
  // A locked-out server can accept the TCP socket but never finish the
  // WebSocket upgrade; cap how long we wait so a stalled connect retries
  // instead of hanging forever on "Looking for the car".
  static const Duration _connectTimeout = Duration(seconds: 3);
  static const String _hostPrefKey = 'car_host';

  String _host = defaultHost;
  CarConnectionState _state = CarConnectionState.disconnected;
  WebSocketChannel? _channel;
  StreamSubscription<dynamic>? _subscription;
  Timer? _heartbeat;
  Timer? _retry;
  bool _suspended = false;
  bool _disposed = false;
  // Bumped on every teardown so a connect attempt that resolves late (e.g. a
  // stalled handshake that finally times out) can tell it has been superseded
  // and quietly bow out instead of clobbering a newer connection.
  int _generation = 0;

  String activeDrive = 'S';
  bool turboOn = false;
  bool cameraOn = true;
  bool headlightOn = false;

  String get host => _host;
  CarConnectionState get state => _state;
  bool get isConnected => _state == CarConnectionState.connected;
  String get streamUrl => 'http://$_host:81/stream';

  Future<void> init() async {
    final prefs = await SharedPreferences.getInstance();
    _host = prefs.getString(_hostPrefKey) ?? defaultHost;
    connect();
  }

  /// Override the car address (persisted), e.g. if the firmware is reverted
  /// to station mode. Reconnects immediately.
  Future<void> setHost(String value) async {
    final v = value.trim();
    if (v.isEmpty || v == _host) return;
    _host = v;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_hostPrefKey, v);
    _teardown();
    _setState(CarConnectionState.disconnected);
    connect();
  }

  void connect() {
    if (_suspended || _disposed || _state != CarConnectionState.disconnected) {
      return;
    }
    _setState(CarConnectionState.connecting);
    _open();
  }

  /// Force an immediate reconnect, e.g. from a manual "Retry" button. Works
  /// even while a previous attempt is mid-handshake: teardown bumps the
  /// generation so the stalled attempt bows out, then we start fresh.
  void reconnectNow() {
    if (_suspended || _disposed) return;
    _teardown();
    _setState(CarConnectionState.disconnected);
    connect();
  }

  Future<void> _open() async {
    final gen = ++_generation;
    WebSocketChannel? channel;
    try {
      channel = WebSocketChannel.connect(Uri.parse('ws://$_host/ws'));
      await channel.ready.timeout(_connectTimeout);
      if (_suspended || _disposed || gen != _generation) {
        channel.sink.close();
        return;
      }
      _channel = channel;
      _subscription = channel.stream.listen(
        (_) {},
        onDone: _onLost,
        onError: (Object _) => _onLost(),
        cancelOnError: true,
      );
      _heartbeat = Timer.periodic(_heartbeatPeriod, (_) => _send('P'));
      // Re-assert toggle states so the car matches the UI after a reconnect.
      _send(cameraOn ? 'C1' : 'C0');
      _send(headlightOn ? 'H1' : 'H0');
      if (turboOn) _send('T');
      _setState(CarConnectionState.connected);
    } catch (_) {
      channel?.sink.close();
      if (gen == _generation) _onLost(); // ignore a superseded attempt
    }
  }

  void _onLost() {
    _teardown();
    if (_suspended || _disposed) return;
    _setState(CarConnectionState.disconnected);
    _retry = Timer(_retryDelay, connect);
  }

  // ---- control message helpers -------------------------------------------

  /// Drive command: F/B/L/R while held, S on release.
  void drive(String command) {
    activeDrive = command;
    _send(command);
    notifyListeners();
  }

  void stop() => drive('S');

  void setTurbo(bool on) {
    turboOn = on;
    _send(on ? 'T' : 'N');
    notifyListeners();
  }

  void toggleCamera() {
    cameraOn = !cameraOn;
    _send(cameraOn ? 'C1' : 'C0');
    notifyListeners();
  }

  void toggleHeadlight() {
    headlightOn = !headlightOn;
    _send(headlightOn ? 'H1' : 'H0');
    notifyListeners();
  }

  // ---- app lifecycle ------------------------------------------------------

  /// App going to background: stop the car and release the socket rather
  /// than relying on the firmware watchdogs.
  void suspend() {
    if (_suspended) return;
    _suspended = true;
    _send('S');
    _send('N');
    activeDrive = 'S';
    turboOn = false;
    _teardown();
    _setState(CarConnectionState.disconnected);
  }

  void resume() {
    if (!_suspended) return;
    _suspended = false;
    connect();
  }

  // ---- internals -----------------------------------------------------------

  void _send(String message) {
    final channel = _channel;
    if (channel == null) return;
    try {
      channel.sink.add(message);
    } catch (_) {
      _onLost();
    }
  }

  void _teardown() {
    _generation++; // invalidate any in-flight/stalled connect attempt
    _heartbeat?.cancel();
    _heartbeat = null;
    _retry?.cancel();
    _retry = null;
    _subscription?.cancel();
    _subscription = null;
    _channel?.sink.close();
    _channel = null;
  }

  void _setState(CarConnectionState next) {
    if (_state == next) return;
    _state = next;
    notifyListeners();
  }

  @override
  void dispose() {
    _disposed = true;
    _teardown();
    super.dispose();
  }
}
