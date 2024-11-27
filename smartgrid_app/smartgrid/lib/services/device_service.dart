import 'dart:convert';
import 'package:http/http.dart' as http;
import 'auth_service.dart';

class DeviceService {
  // final String baseUrl = 'http://192.168.1.139:8080';
  final String baseUrl = 'http://smartgrid-app.xyz:8080';
  final AuthService authService = AuthService();

  Future<List<dynamic>> fetchDevices() async {
    final token = await authService.getToken();
    final response = await http.get(
      Uri.parse('$baseUrl/fetchDevices'),
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer $token',
      },
    );

    if (response.statusCode == 200) {
      return jsonDecode(response.body);
    } else {
      throw Exception('Failed to load devices');
    }
  }

  Future<bool> sendCommand(int deviceId, String command) async {
    final response = await http.post(
      Uri.parse('$baseUrl/sendPacket/$deviceId'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({'command': command}),
    );
    return response.statusCode == 200;
  }

  Future<void> createDevice({required String name, required String ipAddress, required String userId }) async {
    final response = await http.post(
      Uri.parse('$baseUrl/createDevice'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'name': name,
        'ip_addr': ipAddress,
        'user_id': int.parse(userId)
      }),
    );

    if (response.statusCode != 200) {
      throw Exception('Failed to add device');
    }
  }

  Future<void> deleteDevice(int deviceId) async {
    final response = await http.delete(
      Uri.parse('$baseUrl/deleteDevice/$deviceId'),
      headers: {'Content-Type': 'application/json'},
    );

    if (response.statusCode != 200) {
      throw Exception('Failed to delete device');
    }
  }

}