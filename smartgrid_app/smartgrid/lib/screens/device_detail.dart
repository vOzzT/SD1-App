import 'package:flutter/material.dart';
import 'package:smartgrid/services/device_service.dart'; // Assuming DeviceService has methods for device actions

class DeviceDetailScreen extends StatefulWidget {
  final dynamic device;

  const DeviceDetailScreen({required this.device});

  @override
  _DeviceDetailScreenState createState() => _DeviceDetailScreenState();
}

class _DeviceDetailScreenState extends State<DeviceDetailScreen> {
  final DeviceService deviceService = DeviceService();
  bool isLedOn = false;

  Future<void> toggleLed() async {
    try {
      await deviceService.sendCommand(widget.device['id'], "ToggleLed");
      setState(() {
        isLedOn = !isLedOn;
      });
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to toggle LED: $e')),
      );
    }
  }

  Future<void> pingDevice() async {
    try {
      await deviceService.sendCommand(widget.device['id'], 'Ping');
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Device pinged successfully')),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to ping device: $e')),
      );
    }
  }

  Future<void> confirmAndDeleteDevice() async {
    bool? confirmDelete = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text('Delete Device'),
        content: Text('Are you sure you want to delete this device?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, true),
            child: Text('Delete'),
          ),
        ],
      ),
    );

    if (confirmDelete == true) {
      try {
        await deviceService.deleteDevice(widget.device['id']);
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Device deleted successfully')),
        );
        Navigator.pop(context, true);  // Return to previous screen after deletion
      } catch (e) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to delete device: $e')),
        );
      }
    }
  }

  Future<void> updateDevice() async {
    // Here you would navigate to another screen or open a dialog to update the device.
    // For simplicity, we’ll show a placeholder SnackBar.
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text('Update device feature coming soon')),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Device Details'),
        actions: [
          PopupMenuButton<String>(
            onSelected: (value) {
              if (value == 'update') {
                updateDevice();
              } else if (value == 'delete') {
                confirmAndDeleteDevice();
              }
            },
            itemBuilder: (context) => [
              PopupMenuItem(
                value: 'update',
                child: Text('Update Device'),
              ),
              PopupMenuItem(
                value: 'delete',
                child: Text('Delete Device'),
              ),
            ],
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Text('Device Name: ${widget.device['name']}', style: TextStyle(fontSize: 18)),
            Text('Device IP: ${widget.device['ip_addr']}', style: TextStyle(fontSize: 18)),
            SizedBox(height: 20),
            ElevatedButton(
              onPressed: pingDevice,
              child: Text('Ping Device'),
            ),
            SizedBox(height: 20),
            SwitchListTile(
              title: Text('Toggle LED'),
              value: isLedOn,
              onChanged: (bool value) {
                toggleLed();
              },
            ),
          ],
        ),
      ),
    );
  }
}
