# Hackspace Sensing

Generic ESP8266 code for sensors that push to MQTT.

uses this MQTT library: <https://github.com/knolleary/pubsubclient>

See sensor information: https://wiki.sheffieldhackspace.org.uk/members/projects/monitoring

## Build using PlatformIO

First, edit `secrets.h` to make sure you're using the correct MQTT topic etc.

Then, something like:

```bash
# test flashing
pio run -e testblink -t upload
# test WiFi and MQTT 
pio run -e testmqttpub -t upload

# upload specific firmware (e.g., SCD40)
pio run -e scd40-main -t upload

# connect to serial port
pio device monitor
```

## listen to MQTT

listen to MQTT messages with something like this:

```bash
mosquitto_sub -h mosquitto.shhm.uk -t '#' -v
```
