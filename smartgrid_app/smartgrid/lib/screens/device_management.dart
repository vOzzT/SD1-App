import 'package:flutter/material.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:smartgrid/services/device_service.dart';
import 'package:smartgrid/screens/device_detail.dart';
import 'package:smartgrid/screens/user_detail.dart';
import 'package:smartgrid/screens/login.dart';
import 'package:smartgrid/screens/add_device.dart';

class DeviceScreen extends StatefulWidget {
  @override
  _DeviceScreenState createState() => _DeviceScreenState();
}

class _DeviceScreenState extends State<DeviceScreen> {
  final DeviceService deviceService = DeviceService();
  final FlutterSecureStorage secureStorage = FlutterSecureStorage();
  late Future<List<dynamic>> devices;

  @override
  void initState() {
    super.initState();
    devices = deviceService.fetchDevices();
  }

  Future<void> logout() async {
    await secureStorage.deleteAll();
    Navigator.pushAndRemoveUntil(
      context,
      MaterialPageRoute(builder: (context) => LoginScreen()),
      (route) => false,
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('My Devices'),
        actions: [
          PopupMenuButton<String>(
            onSelected: (value) {
              if (value == 'user_management') {
                Navigator.push(
                  context,
                  MaterialPageRoute(
                      builder: (context) => UserManagementScreen()),
                );
              } else if (value == 'logout') {
                logout();
              }
            },
            itemBuilder: (context) => [
              PopupMenuItem(
                value: 'user_management',
                child: Text('User Management'),
              ),
              PopupMenuItem(
                value: 'logout',
                child: Text('Logout'),
              ),
            ],
          ),
        ],
      ),
      body: FutureBuilder<List<dynamic>>(
        future: devices,
        builder: (context, snapshot) {
          if (snapshot.connectionState == ConnectionState.waiting) {
            return Center(child: CircularProgressIndicator());
          } else if (snapshot.hasError) {
            return Center(child: Text('Failed to load devices'));
          } else {
            return ListView.builder(
              itemCount: snapshot.data?.length ?? 0,
              itemBuilder: (context, index) {
                final device = snapshot.data![index];
                return ListTile(
                  title: Text(device['name']),
                  subtitle: Text(device['ip_addr']),
                  onTap: () {
                    Navigator.push(
                      context,
                      MaterialPageRoute(
                        builder: (context) =>
                            DeviceDetailScreen(device: device),
                      ),
                    );
                  },
                );
              },
            );
          }
        },
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () {
          Navigator.push(
            context,
            MaterialPageRoute(builder: (context) => AddDeviceScreen()),
          ).then((result) {
            if (result == true) {
              setState(() {
                devices =
                    deviceService.fetchDevices(); // Refresh the devices list
              });
            }
          });
        },
        child: Icon(Icons.add),
      ),
    );
  }
}
