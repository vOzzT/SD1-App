import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';  // For the chart widget
import 'package:smartgrid/services/device_service.dart';  // For the DeviceService

class FrequencyChart extends StatefulWidget {
  final int deviceId;
  FrequencyChart({required this.deviceId});

  @override
  _FrequencyChartState createState() => _FrequencyChartState();
}

class _FrequencyChartState extends State<FrequencyChart> {
  final DeviceService deviceService = DeviceService();
  List<FlSpot> _dataPoints = [];
  int? touchedIndex;

  @override
  void initState() {
    super.initState();
    fetchFrequencyData();
  }

  Future<void> fetchFrequencyData() async {
    try {
      List<dynamic> data = await deviceService.fetchFrequencyData(widget.deviceId);
      // Process the data and update chart
      List<FlSpot> spots = [];
      for (var entry in data) {
        DateTime timestamp = DateTime.parse(entry['timestamp']);
        double frequency = entry['frequency'].toDouble();
        spots.add(FlSpot(timestamp.millisecondsSinceEpoch / 1000, frequency));
      }

      setState(() {
        _dataPoints = spots;
      });
    } catch (e) {
      print('Error fetching frequency data: $e');
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("Frequency Graph")),
      body: Padding(
        padding: EdgeInsets.all(10),
        child: _dataPoints.isNotEmpty
            ? LineChart(
          LineChartData(
            titlesData: FlTitlesData(show: false),
            gridData: FlGridData(show: false),
            borderData: FlBorderData(
              show: true,
              border: Border.all(
                color: const Color(0xff37434d),
                width: 1,
              ),
            ),
            lineBarsData: [
              LineChartBarData(
                spots: _dataPoints,
                isCurved: true,  // Make the line smooth
                belowBarData: BarAreaData(show: false),
                isStrokeCapRound: true,  // Round the line ends
                dotData: FlDotData(show: false),  // Disable the dots
              ),
            ],
            lineTouchData: LineTouchData(
              touchTooltipData: LineTouchTooltipData(
                tooltipBgColor: Colors.blueAccent,  // Tooltip background color
              ),
              touchCallback: (FlTouchEvent touchEvent, LineTouchResponse? lineTouchResponse) {
                setState(() {
                  touchedIndex = lineTouchResponse != null && lineTouchResponse.lineBarSpots != null && lineTouchResponse.lineBarSpots!.isNotEmpty
                      ? lineTouchResponse.lineBarSpots![0].spotIndex
                      : null;
                });
              },
              handleBuiltInTouches: true,  // Enables touch interaction
            ),
            // Padding to adjust the graph bounds
            minX: _dataPoints.isNotEmpty ? _dataPoints.first.x - 1 : 0, // Add some padding to the x-axis
            maxX: _dataPoints.isNotEmpty ? _dataPoints.last.x + 1 : 1,  // Add some padding to the x-axis
            minY: _dataPoints.isNotEmpty ? _dataPoints.map((e) => e.y).reduce((a, b) => a < b ? a : b) - 10 : 0,  // Add some padding to the y-axis (bottom)
            maxY: _dataPoints.isNotEmpty ? _dataPoints.map((e) => e.y).reduce((a, b) => a > b ? a : b) + 10 : 10,  // Add some padding to the y-axis (top)
          ),
        )
            : Center(child: CircularProgressIndicator()),
      ),
    );
  }
}
