import 'package:flutter/material.dart';
import 'package:smartgrid/services/device_service.dart';
import 'package:smartgrid/screens/device_graphing.dart';

class DeviceDetailScreen extends StatefulWidget {
  final dynamic device;

  const DeviceDetailScreen({required this.device});

  @override
  _DeviceDetailScreenState createState() => _DeviceDetailScreenState();
}

class _DeviceDetailScreenState extends State<DeviceDetailScreen> {
  final DeviceService deviceService = DeviceService();
  List<dynamic> breakers = [];

  // Fetch Breakers from the server and update the breakers list
  Future<void> fetchBreakers() async {
    try {
      List<dynamic> fetchedBreakers = await deviceService.fetchBreakers(widget.device['id']);
      setState(() {
        breakers = fetchedBreakers;
      });
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to load breakers: $e')),
      );
    }
  }

  @override
  void initState() {
    super.initState();
    fetchBreakers();  // Fetch breakers when the screen is first loaded
  }

  // Navigate to FrequencyChart Screen
  void navigateToFrequencyChart() {
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (context) => FrequencyChart(deviceId: widget.device['id']),
      ),
    );
  }


  Future<void> flashLed() async {
    try {
      await deviceService.sendCommand(widget.device['id'], "flashLED");
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Flashed LED successfully')),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to toggle LED: $e')),
      );
    }
  }

  Future<void> pingDevice() async {
    try {
      await deviceService.sendCommand(widget.device['id'], 'pingDevice');
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Device pinged successfully')),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to ping device: $e')),
      );
    }
  }

  Future<void> toggleBreaker(int breakerId) async {
    try {
      await deviceService.sendCommand(widget.device['id'], "ToggleBreaker", breakerId: breakerId);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Breaker toggled successfully')),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to toggle breaker: $e')),
      );
    }
  }

  Future<void> addBreaker() async {
    TextEditingController nameController = TextEditingController();
    TextEditingController numberController = TextEditingController();

    await showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Text('Add Breaker'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: nameController,
              decoration: InputDecoration(labelText: 'Breaker Name'),
            ),
            TextField(
              controller: numberController,
              decoration: InputDecoration(labelText: 'Breaker Number'),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: Text('Cancel'),
          ),
          TextButton(
            onPressed: () async {
              String name = nameController.text;
              String number = numberController.text;
              if (name.isNotEmpty && number.isNotEmpty) {
                try {
                  await deviceService.createBreaker(
                      name: name,
                      deviceId: widget.device['id'],
                      breaker_number: number
                  );

                  // Optionally, fetch the updated list of breakers
                  fetchBreakers();

                  Navigator.pop(context);
                } catch (e) {
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(content: Text('Failed to add breaker: $e')),
                  );
                }
              }
            },
            child: Text('Add'),
          ),
        ],
      ),
    );
  }

  // Edit Breaker Method
  Future<void> editBreaker(int breakerId, String currentName, String currentNumber) async {
    TextEditingController nameController = TextEditingController();
    TextEditingController numberController = TextEditingController();

    nameController.text = currentName;
    numberController.text = currentNumber;

    await showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Text('Edit Breaker'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: nameController,
              decoration: InputDecoration(labelText: 'Breaker Name'),
            ),
            TextField(
              controller: numberController,
              decoration: InputDecoration(labelText: 'Breaker Number'),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: Text('Cancel'),
          ),
          TextButton(
            onPressed: () async {
              String name = nameController.text;
              String number = numberController.text;
              if (name.isNotEmpty && number.isNotEmpty) {
                try {
                  await deviceService.updateBreaker(
                      breakerId: breakerId,
                      name: name,
                      breakerNumber: number
                  );
                  fetchBreakers();
                  Navigator.pop(context);
                } catch (e) {
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(content: Text('Failed to update breaker: $e')),
                  );
                }
              }
            },
            child: Text('Save'),
          ),
        ],
      ),
    );
  }

  // Delete Breaker Method
  Future<void> deleteBreaker(int breakerId) async {
    try {
      await deviceService.deleteBreaker(breakerId);
      fetchBreakers();  // Refresh the breaker list
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Breaker deleted successfully')),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to delete breaker: $e')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Device Details'),
      ),
      body: SingleChildScrollView(  // Wrap the content in a SingleChildScrollView to handle overflow
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: <Widget>[
            Text('Device Name: ${widget.device['name']}', style: TextStyle(fontSize: 18)),
            Text('MAC Address: ${widget.device['mac_addr']}', style: TextStyle(fontSize: 18)),
            SizedBox(height: 20),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: [
                IconButton(
                  icon: Icon(Icons.cable, size: 30),  // Graph icon
                  onPressed: pingDevice,  // Navigate to Frequency Chart screen
                ),
                IconButton(
                  icon: Icon(Icons.insert_chart, size: 30),  // Graph icon
                  onPressed: navigateToFrequencyChart,  // Navigate to Frequency Chart screen
                ),
              ],
            ),
            SizedBox(height: 20),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text('Breakers:', style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                IconButton(
                  icon: Icon(Icons.add_box, size: 30),
                  onPressed: addBreaker,
                ),
              ],
            ),
            SizedBox(height: 10),
            breakers.isNotEmpty
                ? Column(
              children: breakers.map<Widget>((breaker) {
                return ListTile(
                  leading: Icon(
                    breaker['status'] == true ? Icons.lightbulb : Icons.lightbulb_outline,
                    color: breaker['status'] == true ? Colors.yellow : Colors.grey,
                  ),
                  title: Text('${breaker['name']} - ${breaker['breaker_number']}'),
                  trailing: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      IconButton(
                        icon: Icon(Icons.edit),
                        onPressed: () {
                          editBreaker(breaker['id'], breaker['name'], breaker['breaker_number']);
                        },
                      ),
                      IconButton(
                        icon: Icon(Icons.delete),
                        onPressed: () {
                          deleteBreaker(breaker['id']);
                        },
                      ),
                    ],
                  ),
                  onTap: () {
                    toggleBreaker(breaker['id']);
                    setState(() {
                      breaker['status'] = breaker['status'] == true ? false : true;  // Toggle the status
                    });
                  },
                );
              }).toList(),
            )
                : Text('No breakers registered for this device.'),
          ],
        ),
      ),
    );
  }
}
