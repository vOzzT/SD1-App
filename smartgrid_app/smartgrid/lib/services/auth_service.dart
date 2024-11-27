import 'dart:convert';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:http/http.dart' as http;

class AuthService {
  // final String baseUrl = 'http://192.168.1.139:8080';
  final String baseUrl = 'http://smartgrid-app.xyz:8080';
  final storage = FlutterSecureStorage();

  Future<String?> login(String username, String password) async {
    final response = await http.post(
      Uri.parse('$baseUrl/login'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({'login': username, 'pass': password}),
    );

    if (response.statusCode == 200) {
      final data = jsonDecode(response.body);
      final token = data['token'];
      final userID = data['userID'];

      await storage.write(key: 'jwt', value: token);
      await storage.write(key: 'userID', value: userID.toString());
      
      return token;
    } else {
      throw Exception('Login failed');
    }
  }

  Future<void> logout() async {
    await storage.delete(key: 'jwt');
  }

  Future<String?> getToken() async {
    return await storage.read(key: 'jwt');
  }

  Future<bool> signup(String name, String email, String username, String password) async {
    final response = await http.post(
      Uri.parse('$baseUrl/createUser'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'name': name,
        'email': email,
        'login': username,
        'pass': password,
        'isverified': false,
      }),
    );
    return response.statusCode == 200;
  }
  
}
