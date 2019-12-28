# Xiaomi BTLE Hygrometer to WiFi proxy

This project implements a ESP32-based proxy device, that reports the temperatures and humidities read from Xiaomi BTLE sensors to a central Influx database over an anonymous UDP connection.

The reason for developing this project was the inability to reach all sensors from a central spot (a Linux machine). The ESP32 based proxy devices are small and can be plugged in using a USB power adapter at reasonable locations to gather the data and send over WiFi back to server. 

The Bluetooth LE communication range will vary depending on the ESP32 device used. For example, the devices having ESP32-WROOM-32D module with the antenna part overhanging the device are better than the older ESP32-WROOM-32 modules with the antenna on top of the device. Modules with external antennas may perform even better.

Data sent to Influx:

    <db>,[<extra_tags>,]id=<sensor_mac>,name=<sensor_name> temperature=22.2,humidity=33.3

## Building

This project is built using esp-idf:

    source ../esp-idf/export.sh
    idf.py build
    idf.py flash monitor
To exit from monitor, press ctrl+].

## Configuring

The "idf.py monitor" command launces a terminal that can be used to set up the wireless network.

    connect <ssid> <pass>

Once the network has been configured, the IP address is shown via the terminal:

    WIFI: ip:192.168.1.123

Connect to this address using web browser. Do a bluetooth scan and copy the MAC addresses of your devices (MJ\_HT\_V1). 
Set up the hygproxy device via the configuration page:
* **Influx server**: IP address of Influx server to connect to
* **Influx database**: Database name to write your measurements to
* **Influx extra tags**: Extra tags to quantify your results with. Separate multiple tags with commas. Ie: "proxy:dev1,location:house1". Leave empty if not needed.
* **Influx interval**: Interval between measurements. Be aware that the measurement process is single-threaded and in case of a lot of sensors and communication timeouts, this interval may not be reached.
* **Device list**: Mac address and name of the sensor. Mac addresses shall be without separators, just 12 hexadecimal digits. Name is just sent to Influx.

