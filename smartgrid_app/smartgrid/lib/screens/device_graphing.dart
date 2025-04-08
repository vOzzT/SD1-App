import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import 'package:smartgrid/services/device_service.dart';
import 'package:timezone/data/latest.dart' as tz;
import 'package:timezone/timezone.dart' as tz;
import 'package:intl/intl.dart';  // For formatting date and time

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

  DateTime? start;
  DateTime? end;
  String selectedRange = 'Last 1 hour';

  final Map<String, Duration> presets = {
    'Last 1 hour': Duration(hours: 1),
    'Last 6 hours': Duration(hours: 6),
    'Last 24 hours': Duration(hours: 24),
    'Custom': Duration.zero,
  };

  tz.TZDateTime? localStartTime;
  tz.TZDateTime? localEndTime;

  @override
  void initState() {
    super.initState();
    tz.initializeTimeZones();
    applyPreset('Last 1 hour');
  }

  Future<void> applyPreset(String range) async {
    DateTime now = DateTime.now();
    tz.TZDateTime localNow = tz.TZDateTime.now(tz.local);

    if (range == 'Custom') {
      final picked = await showDateRangePicker(
        context: context,
        firstDate: DateTime.now().subtract(Duration(days: 30)),
        lastDate: DateTime.now(),
      );

      if (picked != null) {
        setState(() {
          start = picked.start;
          end = picked.end;
          selectedRange = 'Custom';
        });
        await fetchFrequencyData(); // Fetch data after custom range selection
      }
    } else {
      setState(() {
        localEndTime = localNow;
        localStartTime = localNow.subtract(presets[range]!);
        selectedRange = range;
      });
      await fetchFrequencyData(); // Fetch data after preset range selection
    }
  }

  Future<void> fetchFrequencyData() async {
    try {
      print("Fetching data with start: $start, end: $end");

      // Fetch the data based on the selected range
      List<dynamic> data = await deviceService.fetchFrequencyData(
        widget.deviceId,
        start: start,
        end: end,
      );

      // Check if data is empty
      if (data.isEmpty) {
        print("No data received.");
      } else {
        print("Data received: $data");

        // Print first and last data point for debugging
        if (data.isNotEmpty) {
          var firstDataPoint = data.first;  // First data point
          var lastDataPoint = data.last;    // Last data point
          print("First data point: $firstDataPoint");
          print("Last data point: $lastDataPoint");
        }

        List<FlSpot> spots = data.map((entry) {
          DateTime timestamp = DateTime.parse(entry['timestamp']);
          tz.TZDateTime localTime = tz.TZDateTime.from(timestamp, tz.local);
          double frequency = double.parse(entry['frequency'].toStringAsFixed(3));  // Cap the frequency to 3 decimal places
          return FlSpot(localTime.millisecondsSinceEpoch / 1000, frequency);
        }).toList();

        // Filter data points to match selected time range
        List<FlSpot> trimmedDataPoints = spots.where((spot) {
          return spot.x >= (localStartTime?.millisecondsSinceEpoch ?? 0) / 1000 &&
              spot.x <= (localEndTime?.millisecondsSinceEpoch ?? 0) / 1000;
        }).toList();

        setState(() {
          _dataPoints = trimmedDataPoints;
        });
      }
    } catch (e) {
      print('Error fetching frequency data: $e');
    }
  }


  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("Frequency Graph")),
      body: Column(
        children: [
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
            child: Row(
              children: [
                Text("Select Time Range: ", style: TextStyle(fontSize: 14)),
                SizedBox(width: 10),
                DropdownButton<String>(
                  value: selectedRange,
                  items: presets.keys.map((range) {
                    return DropdownMenuItem<String>(
                      value: range,
                      child: Text(range),
                    );
                  }).toList(),
                  onChanged: (value) {
                    if (value != null) applyPreset(value);
                  },
                ),
              ],
            ),
          ),
          Expanded(
            child: _dataPoints.isEmpty
                ? Center(child: CircularProgressIndicator())
                : Padding(
              padding: const EdgeInsets.only(left: 10.0, right: 20.0, bottom: 20.0, top: 10),
              child: LineChart(
                LineChartData(
                  clipData: FlClipData.all(),
                  minX: localStartTime != null
                      ? localStartTime!.millisecondsSinceEpoch / 1000
                      : _dataPoints.first.x,
                  maxX: localEndTime != null
                      ? localEndTime!.millisecondsSinceEpoch / 1000
                      : _dataPoints.last.x,
                  minY: _dataPoints.map((e) => e.y).reduce((a, b) => a < b ? a : b) - 5,
                  maxY: _dataPoints.map((e) => e.y).reduce((a, b) => a > b ? a : b) + 5,
                  gridData: FlGridData(show: true),
                  borderData: FlBorderData(
                    show: true,
                    border: Border.all(color: const Color(0xff37434d), width: 1),
                  ),
                  titlesData: FlTitlesData(
                    bottomTitles: AxisTitles(
                      axisNameWidget: Padding(
                        padding: const EdgeInsets.only(top: 8.0),
                        child: Text('Time (hh:mm)', style: TextStyle(fontWeight: FontWeight.bold)),
                      ),
                      axisNameSize: 30,
                      sideTitles: SideTitles(
                        showTitles: true,
                        reservedSize: 40,
                        interval: (_dataPoints.last.x - _dataPoints.first.x) / 4,
                        getTitlesWidget: (value, meta) {
                          // Adjust intervals or show titles based on the range of time
                          double range = _dataPoints.last.x - _dataPoints.first.x;

                          if (range > 3600) {
                            // For larger ranges (e.g., 24 hours), hide the titles
                            return SideTitleWidget(
                              axisSide: meta.axisSide,
                              space: 6,
                              child: Container(), // Empty container to hide the label
                            );
                          } else {
                            // For smaller ranges, show the time labels
                            final date = DateTime.fromMillisecondsSinceEpoch((value * 1000).toInt());
                            final formatted = DateFormat('HH:mm').format(date);
                            return SideTitleWidget(
                              axisSide: meta.axisSide,
                              space: 6,
                              child: Text(formatted, style: TextStyle(fontSize: 10)),
                            );
                          }
                        },
                      ),
                    ),
                    leftTitles: AxisTitles(
                      axisNameWidget: Padding(
                        padding: const EdgeInsets.only(right: 8.0),
                        child: Text('Frequency (Hz)', style: TextStyle(fontWeight: FontWeight.bold)),
                      ),
                      axisNameSize: 30,
                      sideTitles: SideTitles(
                        showTitles: true,
                        reservedSize: 40,
                        interval: 10,
                        getTitlesWidget: (value, meta) {
                          return SideTitleWidget(
                            axisSide: meta.axisSide,
                            space: 4,
                            child: Text('${value.toStringAsFixed(3)}', style: TextStyle(fontSize: 10)), // Cap to 3 decimal places here
                          );
                        },
                      ),
                    ),
                    topTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                    rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                  ),
                  lineBarsData: [
                    LineChartBarData(
                      spots: _dataPoints,
                      isCurved: true,
                      belowBarData: BarAreaData(show: false),
                      isStrokeCapRound: true,
                      dotData: FlDotData(show: false),
                    ),
                  ],
                  lineTouchData: LineTouchData(
                    handleBuiltInTouches: true,
                    touchCallback: (FlTouchEvent event, LineTouchResponse? response) {
                      setState(() {
                        // Ensure the touch events are within bounds
                        if (response != null && response.lineBarSpots != null && response.lineBarSpots!.isNotEmpty) {
                          double x = response.lineBarSpots![0].x;
                          if (x >= localStartTime!.millisecondsSinceEpoch / 1000 &&
                              x <= localEndTime!.millisecondsSinceEpoch / 1000) {
                            touchedIndex = response.lineBarSpots![0].spotIndex;
                          } else {
                            touchedIndex = null; // Don't show touch events outside the range
                          }
                        } else {
                          touchedIndex = null;
                        }
                      });
                    },
                    touchTooltipData: LineTouchTooltipData(
                      tooltipBgColor: Colors.blueAccent,
                    ),
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
