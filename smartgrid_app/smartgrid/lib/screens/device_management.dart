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

  // Method to delete the device
  Future<void> deleteDevice(int deviceId) async {
    try {
      await deviceService.deleteDevice(deviceId);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Device deleted successfully')),
      );
      setState(() {
        devices = deviceService.fetchDevices(); // Refresh the devices list
      });
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to delete device: $e')),
      );
    }
  }

  // Method to handle device name update
  Future<void> updateDeviceName(int deviceId, String currentName) async {
    TextEditingController nameController = TextEditingController();
    nameController.text = currentName;

    await showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Text('Update Device Name'),
        content: TextField(
          controller: nameController,
          decoration: InputDecoration(labelText: 'Device Name'),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: Text('Cancel'),
          ),
          TextButton(
            onPressed: () async {
              String newName = nameController.text;
              if (newName.isNotEmpty) {
                try {
                  await deviceService.updateDevice(deviceId: deviceId, name: newName);
                  setState(() {
                    devices = deviceService.fetchDevices(); // Refresh the devices list
                  });
                  Navigator.pop(context);
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(content: Text('Device name updated successfully')),
                  );
                } catch (e) {
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(content: Text('Failed to update device name: $e')),
                  );
                }
              }
            },
            child: Text('Update'),
          ),
        ],
      ),
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
                  subtitle: Text(device['mac_addr']),
                  onTap: () {
                    Navigator.push(
                      context,
                      MaterialPageRoute(
                        builder: (context) =>
                            DeviceDetailScreen(device: device),
                      ),
                    );
                  },
                  trailing: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      // Pencil icon for editing device name
                      IconButton(
                        icon: Icon(Icons.edit),
                        onPressed: () {
                          updateDeviceName(device['id'], device['name']);
                        },
                      ),
                      // Trash can icon for deleting device
                      IconButton(
                        icon: Icon(Icons.delete),
                        onPressed: () {
                          deleteDevice(device['id']);
                        },
                      ),
                    ],
                  ),
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
