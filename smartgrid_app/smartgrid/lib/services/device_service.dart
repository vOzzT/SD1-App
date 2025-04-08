import 'dart:convert';
import 'package:http/http.dart' as http;
import 'auth_service.dart';

class DeviceService {
  // final String baseUrl = 'http://10.0.2.2:8080';
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

  Future<List<dynamic>> fetchFrequencyData(int deviceId, {DateTime? start, DateTime? end}) async {
    final token = await authService.getToken();
    String url = '$baseUrl/fetchFrequencyData/$deviceId';

    if (start != null && end != null) {
      url += '?start=${start.toIso8601String()}&end=${end.toIso8601String()}';
    }

    final response = await http.get(
      Uri.parse(url),
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer $token',
      },
    );

    if (response.statusCode == 200) {
      return jsonDecode(response.body);
    } else {
      throw Exception('Failed to load frequency data');
    }
  }


  Future<List<dynamic>> fetchBreakers(int deviceId) async {
    final token = await authService.getToken();
    final response = await http.get(
      Uri.parse('$baseUrl/fetchBreakers/$deviceId'),
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer $token',
      },
    );

    if (response.statusCode == 200) {
      return jsonDecode(response.body);
    } else {
      throw Exception('Failed to load breakers');
    }
  }

  Future<bool> sendCommand(int deviceId, String command, {int? breakerId, int? breakerNum, bool? breakerState}) async {
    final Map<String, dynamic> body = {
      'deviceId': deviceId,
      'command': command,
    };

    // Include breakerId only if it's provided
    if (breakerId != null && breakerState != null && breakerNum != null) {
      body['breakerId'] = breakerId;
      body['breakerNumber'] = breakerNum;
      body['breakerState'] = !breakerState;
    }

    final response = await http.post(
      Uri.parse('$baseUrl/sendPacket/$deviceId'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode(body),
    );

    return response.statusCode == 200;
  }

  Future<void> createDevice({required String name, required String userId }) async {
    final response = await http.post(
      Uri.parse('$baseUrl/createDevice'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'name': name,
        'user_id': int.parse(userId)
      }),
    );

    if (response.statusCode != 200) {
      throw Exception('Failed to add device');
    }
  }

  Future<void> updateDevice({required int deviceId, required String name}) async {
    final response = await http.put(
      Uri.parse('$baseUrl/updateDevice/$deviceId'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'name': name,
      }),
    );

    if (response.statusCode != 200) {
      throw Exception('Failed to update device');
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

  Future<void> createBreaker({required String name, required int deviceId, required String breakerNumber}) async {
    final response = await http.post(
      Uri.parse('$baseUrl/createBreaker'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'name': name,
        'device_id': deviceId,
        'breaker_number': breakerNumber,
        'status': true
      }),
    );

    if (response.statusCode != 200) {
      throw Exception('Failed to add device');
    }
  }

  Future<void> updateBreaker({required int breakerId, required String name, required String breakerNumber}) async {
    final response = await http.put(
      Uri.parse('$baseUrl/updateBreaker/$breakerId'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'name': name,
        'breaker_number': breakerNumber,
      }),
    );

    if (response.statusCode != 200) {
      throw Exception('Failed to update breaker');
    }
  }

  Future<void> deleteBreaker(int breakerId) async {
    final response = await http.delete(
      Uri.parse('$baseUrl/deleteBreaker/$breakerId'),
      headers: {'Content-Type': 'application/json'},
    );

    if (response.statusCode != 200) {
      throw Exception('Failed to delete device');
    }
  }
}