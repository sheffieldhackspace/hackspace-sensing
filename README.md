# Hackspace Sensing

Generic ESP8266 code for sensors that push to MQTT.

uses this MQTT library: <https://github.com/knolleary/pubsubclient>

## listen to MQTT

listen to MQTT messages with something like this:

```bash
mosquitto_sub -h mosquitto.shhm.uk -t '#' -v
```
