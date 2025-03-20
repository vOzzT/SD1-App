import 'package:flutter/material.dart';
import 'package:smartgrid/services/device_service.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';

class AddDeviceScreen extends StatefulWidget {
  @override
  _AddDeviceScreenState createState() => _AddDeviceScreenState();
}

class _AddDeviceScreenState extends State<AddDeviceScreen> {
  final DeviceService deviceService = DeviceService();
  final TextEditingController nameController = TextEditingController();
  final TextEditingController ipController = TextEditingController();
  final FlutterSecureStorage secureStorage = FlutterSecureStorage();

  Future<void> _createDevice() async {
    String? userId = await secureStorage.read(key: 'userID');

    if (userId == null) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text('User not found')));
      return;
    }

    try {
      await deviceService.createDevice(
        name: nameController.text,
        userId: userId,
      );
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text('Device added successfully')));
      Navigator.pop(context, true);
    } catch (e) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text('Failed to add device')));
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text('Add Device')),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            TextField(
              controller: nameController,
              decoration: InputDecoration(labelText: 'Device Name'),
            ),
            SizedBox(height: 10),
            ElevatedButton(
              onPressed: _createDevice,
              child: Text('Create Device'),
            ),
          ],
        ),
      ),
    );
  }
}
