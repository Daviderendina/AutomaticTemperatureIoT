from flask import Flask, render_template, request, url_for
from flask_mqtt import Mqtt
from flask_socketio import SocketIO
from flask_mysqldb import MySQL
import time
import json
import eventlet

eventlet.monkey_patch()
influxdb_url = "http://149.132.178.180:8086"
influxdb_token = "Rzwt8QckFYOq0VH9NlHpIEZichC5MHPoKswYLvuiPK1JRpjxD5QHb0tXDG1RouJx1DfLbWR-Ddt4T_vlZ1yqCA=="
influxdb_org = "labiot-org"
influxdb_bucket = "ataraboi-bucket"
influxdb_pointDevice = "secondo-assignment"

mqtt_username = "ataraboi"
mqtt_password = "iot829904"
mqtt_broker_ip = "149.132.178.180"
# provo inizialmente a vedere se si possono collegare
mqtt_topic = ["rt/discovery"]
Mac_cancelled = ""
lwill = False
app = Flask(__name__)
# serve per evitare doppie scritture
app.config['MQTT_CLIENT_ID'] = 'gianni2'
app.config['MQTT_BROKER_URL'] = mqtt_broker_ip
app.config['MQTT_BROKER_PORT'] = 1883
app.config['SERVER_NAME'] = '127.0.0.1:8181'
app.config['MQTT_USERNAME'] = mqtt_username
app.config['MQTT_PASSWORD'] = mqtt_password
app.config['MQTT_KEEPALIVE'] = 5
app.config['MQTT_TLS_ENABLED'] = False
app.config['TEMPLATES_AUTO_RELOAD'] = True
# connessione a MYSQL
app.config['MYSQL_HOST'] = '149.132.178.180'
app.config['MYSQL_USER'] = 'ataraboi'
app.config['MYSQL_PASSWORD'] = 'iot829904'
app.config['MYSQL_DB'] = 'ataraboi'

mqtt = Mqtt(app)
socketio = SocketIO(app)
mysql = MySQL(app)
mqtt_array = []
# The callback for when the client receives a CONNACK response from the server.
data_for_response = {}
far_alert_values = {}
for_value_status = {}
for_value_update = {}


@mqtt.on_connect()
def handle_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    for topic in mqtt_topic:
        mqtt.subscribe(topic)
    for topic in for_value_update:
        mqtt.subscribe(topic)

# The callback for when a PUBLISH message is received from the server.


@mqtt.on_message()
def handle_messages(client, userdata, msg):
    with app.app_context():
        my_json = msg.payload.decode('utf8').replace("'", '"')
        if msg.topic == "rt/discovery":
            present = 0
            try:
                payload = json.loads(my_json)
                global Mac_cancelled
                Mac_cancelled = ""
                global lwill
                lwill = False
                for element in mqtt_array:
                    if payload['mac'] == element['mac']:
                        present = 1
                        print("Present value")
            except ValueError as e:
                present = 1
                # last will del singolo dispositivo
                last_will_dispositivo(my_json)
            if present == 0:
                cur = mysql.connection.cursor()
                cur.execute("INSERT INTO device (MAC, name, description,type) VALUES (%s,%s,%s,%s) ON DUPLICATE KEY UPDATE MAC=%s,name=%s, description=%s,type=%s", (
                    (payload['mac'], payload['name'], payload['description'], payload['type'], payload['mac'], payload['name'], payload['description'], payload['type'])))

                mysql.connection.commit()
                mqtt_array.append(payload)
                element = mqtt_array[len(mqtt_array)-1]
                data_for_response = {"mac": payload['mac'],
                                     "status": "rt/devices/"+payload['type']+"/"+payload['mac']+"/status",
                                     "threshold": {},
                                     "sensors": {},
                                     "observes": {},
                                     "database": {"influxdb_url": influxdb_url,
                                                  "influxdb_token": influxdb_token,
                                                  "influxdb_org": influxdb_org,
                                                  "influxdb_bucket": influxdb_bucket,
                                                  "influxdb_pointDevice": influxdb_pointDevice},
                                     "alert": {}
                                     }
                if 'temperaturehumidity' in payload:
                    if payload['temperaturehumidity'] == "true":
                        data_for_response['temperaturehumidity'] = "rt/measures/temperaturehumidity"
                if 'threshold' in payload:
                    for i, treash in enumerate(element['threshold']):
                        data_for_response['threshold'][treash['id']] = "rt/devices/" + \
                            payload['type']+"/threshold/"+payload['mac'] + \
                            "/"+payload['threshold'][i]['id']
                        element['threshold'][i]['valueread'] = data_for_response['threshold'][treash['id']]
                        cur.execute(
                            "INSERT INTO treshold(MAC, name, type, topic) VALUES (%s, %s,%s,%s)", (payload['mac'], payload['threshold'][i]['id'], payload['threshold'][i]['type'], data_for_response['threshold'][treash['id']]))
                        mysql.connection.commit()
                if 'sensors' in payload:
                    for i, sensori in enumerate(element['sensors']):
                        data_for_response['sensors'][sensori['id']] = {"status": "rt/measures/" + sensori['measure']+"/status", "update": "rt/measures/" +
                                                                       sensori['measure']+"/update"}
                        # quando lo aggiungo è acceso, e lo leggo su ../update per le misure dei device
                        global for_value_update
                        for_value_update["rt/measures/" +
                                         sensori['measure']+"/update"] = {'name': sensori['name'], 'value': '0'}
                        # su questo dict mi metto a fare solamente la scrittura su /status
                        global for_value_status
                        for_value_status["rt/measures/" +
                                         sensori['measure']+"/status"] = {'name': sensori['name'], 'value': 'off'}
                        cur.execute(
                            "INSERT INTO sensor(MAC, name, measure, unitofmeasure) VALUES (%s, %s,%s,%s)", (payload['mac'], sensori['name'], sensori['measure'], sensori['unit of measure']))
                        mysql.connection.commit()
                if 'alert' in payload:
                    for i, singoloAlert in enumerate(element['alert']):
                        data_for_response['alert'][singoloAlert] = "rt/alert/" + \
                            singoloAlert
                        global far_alert_values
                        far_alert_values["rt/alert/" + singoloAlert] = {
                            'name': singoloAlert, 'value': "off"}
                if 'observes' in payload:
                    for i, sensori in enumerate(element['observes']):
                        if sensori == "temperaturehumidity":
                            data_for_response['observes'][sensori] = "rt/measures/temperaturehumidity/update"
                        else:
                            if sensori == "deviceStatus":
                                data_for_response['observes'][sensori] = "rt/devices/+/+/status"
                            else:
                                data_for_response['observes'][sensori] = "rt/measures/" + \
                                    sensori + \
                                    "/update"
                        cur.execute(
                            "INSERT INTO observe(MAC,  measure) VALUES (%s, %s)", (payload['mac'], sensori))
                        mysql.connection.commit()
                y = json.dumps(data_for_response)
                mqtt_array[len(mqtt_array)-1] = element
                time.sleep(1)
                mqtt.publish("rt/discovery/response", y)
                for topic in for_value_update:
                    mqtt.subscribe(topic)
                for topic in far_alert_values:
                    mqtt.subscribe(topic)
                cur.execute(
                    "INSERT INTO log(MAC, event) VALUES (%s, %s)", (payload['mac'], 'in'))
                mysql.connection.commit()
                cur.close()
                return render_template("index.html", mqt=mqtt_array, mac_add=Mac_cancelled, lwill=lwill)
        else:
            if msg.topic in for_value_update:  # gestione delle update / value
                for_value_update[msg.topic]['value'] = my_json
            if msg.topic in far_alert_values:  # gestione delle update / value
                print("entro")
                far_alert_values[msg.topic]['value'] = my_json
            return url_for("do_discovery")


@ mqtt.on_publish()
def handle_publish(client, userdata, mid):
    print("mid: "+str(mid))


@ app.route('/')
def do_discovery():
    global Mac_cancelled
    global lwill
    global far_alert_values
    alert = far_alert_values
    a = Mac_cancelled
    b = lwill
    Mac_cancelled = ""
    lwill = False
    return render_template("index.html", mqt=mqtt_array, mac_add=a, l_will=b, status=for_value_status, update=for_value_update, alert=alert)


@ app.route('/', methods=['POST'])  # dati della treshold
def my_form_post():
    if 'cambio_valore' in request.form:
        prova_singola = request.form['cambio_valore']
        dove_pushare = request.form['invia_valore']
        global for_value_status
        if prova_singola == "off":
            for_value_status[dove_pushare]['value'] = "on"
        else:
            for_value_status[dove_pushare]['value'] = "off"
        mqtt.publish(dove_pushare, prova_singola)
    else:
        nome_riferimento = request.form['cambioVal']
        print(mqtt_array)
        valore_publish = 0
        for element in mqtt_array:
            if 'threshold' in element:
                for row in element['threshold']:
                    if row['id'] == nome_riferimento:
                        print(request.form[nome_riferimento])
                        row['value'] = request.form[nome_riferimento]
                        valore_publish = request.form[nome_riferimento]
        dove_publico = request.form['whertoPublish']
        mqtt.publish(dove_publico, valore_publish)
    global far_alert_values
    alert = far_alert_values
    return render_template("index.html", mqt=mqtt_array, status=for_value_status, update=for_value_update, alert=alert)


# salvare all'interno del database il log dato da lastwill
def last_will_dispositivo(some_data):
    spliting = some_data.split(" ")
    for element in mqtt_array:
        if spliting[0] == element['mac']:
            mqtt_array.pop(mqtt_array.index(element))
            with app.app_context():
                if spliting[1] == "off":
                    global Mac_cancelled
                    Mac_cancelled = spliting[0]
                    global lwill
                    lwill = True
                    cur = mysql.connection.cursor()
                    cur.execute(
                        "INSERT INTO log(MAC, event) VALUES (%s, %s)", [spliting[0], 'out'])
                    mysql.connection.commit()
                    cur.execute(
                        "DELETE FROM device WHERE MAC = (%s)", [spliting[0]])
                    mysql.connection.commit()
                    cur.close()
                    return url_for("do_discovery")


if __name__ == "__main__":
    app.run(host='127.0.0.1', port=8181)
