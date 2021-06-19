import influxdb_client
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

influx_bucket = "ataraboi-bucket"
influx_org = "labiot-org"
influx_token = "Rzwt8QckFYOq0VH9NlHpIEZichC5MHPoKswYLvuiPK1JRpjxD5QHb0tXDG1RouJx1DfLbWR-Ddt4T_vlZ1yqCA=="
influx_url = "149.132.178.180:8086"
influx_point = "terzo-assignment"


class DeviceCounter:
    count = 0
    macList = set()

    def increment(self, MAC):
        if(not MAC in self.macList):
            self.count += 1
            self.macList.add(MAC)
            return True
        return False

    def decrement(self, MAC):
        if(MAC in self.macList and self.count > 0):
            self.count -= 1
            self.macList.discard(MAC)
            return True
        return False

    def getCount(self):
        return self.count


class WindowAlertHandler:

    windowCounter = DeviceCounter()
    deviceCounter = DeviceCounter()

    actualAlert = False
    mac = "Server"

    client = influxdb_client.InfluxDBClient(
        url=influx_url, token=influx_token, org=influx_org)

    TOPIC_WINDOW_ALERT = "rt/alert/windowOpen"

    def __init__(self, mqttClient):
        self.mqttClient = mqttClient

    def windowOpened(self, MAC):
        if(self.windowCounter.increment(MAC)):
            print(self.windowCounter.count)
            self.uploadWindowsCountInflux()
            self.checkAlert()

    def windowClosed(self, MAC):
        if(self.windowCounter.decrement(MAC)):
            self.uploadWindowsCountInflux()
            self.checkAlert()

    def deviceOn(self, MAC):
        if(self.deviceCounter.increment(MAC)):
            self.checkAlert()

    def deviceOff(self, MAC):
        if(self.deviceCounter.decrement(MAC)):
            self.checkAlert()

    def calculateActualAlertStatus(self):
        return self.windowCounter.count > 0 and self.deviceCounter.count > 0

    def checkAlert(self):
        if(self.actualAlert != self.calculateActualAlertStatus()):
            print("Window alert status change")
            self.actualAlert = self.calculateActualAlertStatus()
            self.comunicateAlertMQTT()
            self.uploadAlertInflux()

    def comunicateAlertMQTT(self):
        print("MQTT")
        self.mqttClient.publish("rt/alert/windowOpen",
                                "on" if self.actualAlert else "off")

    def uploadAlertInflux(self):
        write_api = self.client.write_api(write_options=SYNCHRONOUS)
        p = influxdb_client.Point(influx_point).tag("device", self.mac).field(
            "window_alert", 1 if self.actualAlert else 0)
        write_api.write(bucket=influx_bucket, org=influx_org, record=p)

    def uploadWindowsCountInflux(self):
        write_api = self.client.write_api(write_options=SYNCHRONOUS)
        p = influxdb_client.Point(influx_point).tag("device", self.mac).field(
            "open_windows_count", self.windowCounter.count)
        write_api.write(bucket=influx_bucket, org=influx_org, record=p)
