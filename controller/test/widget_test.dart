import 'package:flutter_test/flutter_test.dart';

import 'package:rc_car_controller/main.dart';
import 'package:rc_car_controller/models/car_connection.dart';
import 'package:rc_car_controller/screens/connect_screen.dart';

void main() {
  testWidgets('shows the connect guidance screen while disconnected', (
    WidgetTester tester,
  ) async {
    final connection = CarConnection(); // not init'ed: stays disconnected
    await tester.pumpWidget(RcCarApp(connection: connection));

    expect(find.byType(ConnectScreen), findsOneWidget);
    expect(find.textContaining(CarConnection.hotspotName), findsWidgets);
  });
}
