import 'package:flutter/material.dart';
import 'package:smartgrid/screens/login.dart';
import 'package:smartgrid/screens/device_management.dart';
import 'package:smartgrid/services/auth_service.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  final AuthService authService = AuthService();

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Device Control App',
      theme: ThemeData(
        primarySwatch: Colors.red,
      ),
      home: FutureBuilder(
        future: authService.getToken(),
        builder: (context, snapshot) {
          // Check if there's an existing JWT token and show the correct initial screen
          if (snapshot.connectionState == ConnectionState.waiting) {
            return Center(child: CircularProgressIndicator());
          } else if (snapshot.hasData && snapshot.data != null) {
            return DeviceScreen(); // Go to devices if JWT token exists
          } else {
            return LoginScreen(); // Otherwise, show login screen
          }
        },
      ),
    );
  }
}
