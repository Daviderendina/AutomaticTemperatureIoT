from flask import Flask, render_template, request, url_for
from flask_mqtt import Mqtt
from flask_socketio import SocketIO
from flask_mysqldb import MySQL
import time
import json
import eventlet
import WindowControl  # file utilizzato per l'oggetto stesura Alert e chiamte API
# rt/counter <- potrei ricevere semplicemente qui il numero di porte, così posso fare io i controlli
api_topic = "rt/api/weather"
alert_api_topic = "rt/alert/weather"
api_message = ""
eventlet.monkey_patch()
influxdb_url = "http://149.132.178.180:8086"
influxdb_token = "Rzwt8QckFYOq0VH9NlHpIEZichC5MHPoKswYLvuiPK1JRpjxD5QHb0tXDG1RouJx1DfLbWR-Ddt4T_vlZ1yqCA=="
influxdb_org = "labiot-org"
influxdb_bucket = "ataraboi-bucket"
influxdb_pointDevice = "terzo-assignment"

# parte relativa alle credenziali MQTT
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
static_alert = {}
static_alert["rt/alert/windowOpen"] = {
    'name': "Alert window open:", 'value': "off"}
# mi salvo solo i topic, se è in questo topic e il messaggio è "off" -1, altrimenti +1
for_status_devices = {}


@mqtt.on_connect()
def handle_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    for topic in mqtt_topic:
        mqtt.subscribe(topic)
    mqtt.subscribe(api_topic)
    mqtt.subscribe("rt/alert/windowOpen")
    mqtt.publish("rt/discovery/response", "update_request")
# The callback for when a PUBLISH message is received from the server.


@mqtt.on_message()
def handle_messages(client, userdata, msg):
    with app.app_context():
        my_json = msg.payload.decode('utf8')
        if msg.topic == api_topic:
            global api_message
            api_message = str(my_json)
        if msg.topic == "rt/discovery":
            present = 0
            try:
                my_json = my_json.replace("'", '"')
                payload = json.loads(my_json)
                while my_json[-1] != "}":
                    my_json = my_json[:-1]
                my_json = my_json[:-1]
                my_json += ", 'status_update':{ 'status':{}, 'update':{} }}"
                my_json = my_json.replace("'", '"')
                payload = json.loads(my_json)

                global Mac_cancelled
                Mac_cancelled = ""
                global lwill
                lwill = False
                for i, element in enumerate(mqtt_array):
                    if payload['mac'] == element['mac']:
                        mqtt_array.pop(i)
                        print("Present value")
            except ValueError as e:
                present = 1
                print("Formato non JSON, siamo in LastWill message")
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
                if 'deviceStatus' in payload:
                    value = str(payload['deviceStatus']) == "on"
                    for_status_devices[data_for_response["status"]
                                       ] = payload['deviceStatus']
                    element["deviceStatus"] = {
                        "mac": payload['mac'], "status": payload['deviceStatus']}
                    if value:
                        wet_api_call.deviceOn(payload['mac'])
                    else:
                        wet_api_call.deviceOff(payload['mac'])
                else:
                    # default value
                    for_status_devices[data_for_response["status"]] = "off"
                for topic_dei_status in for_status_devices:
                    mqtt.subscribe(topic_dei_status)
                if 'temperaturehumidity' in payload:
                    if payload['temperaturehumidity'] == "true":
                        data_for_response['temperaturehumidity'] = "rt/measures/temperaturehumidity"
                if 'threshold' in payload:
                    for i, treash in enumerate(element['threshold']):
                        data_for_response['threshold'][treash['id']] = "rt/devices/" + \
                            payload['type']+"/threshold/"+payload['mac'] + \
                            "/"+payload['threshold'][i]['id']
                        element['threshold'][i]['valueread'] = data_for_response['threshold'][treash['id']]
                        """
                        In questa parte mi faccio la subscribe a tutti i treshold, in tal modo posso fare soglie da Telegram
                        """
                        mqtt.subscribe(element['threshold'][i]['valueread'])
                        cur.execute(
                            "INSERT INTO treshold(MAC, name, type, topic) VALUES (%s, %s,%s,%s)", (payload['mac'], payload['threshold'][i]['id'], payload['threshold'][i]['type'], data_for_response['threshold'][treash['id']]))
                        mysql.connection.commit()
                if 'sensors' in payload:
                    for i, sensori in enumerate(element['sensors']):
                        data_for_response['sensors'][sensori['id']] = {"status": "rt/measures/" + sensori['measure'] + "/" + payload['mac'] + "/status", "update": "rt/measures/" +
                                                                       sensori['measure'] + "/" + payload['mac'] + "/update"}
                        element['status_update']['update'][str(i)] = {"topic": "rt/measures/" + sensori['measure'] + "/" +
                                                                      payload['mac'] + "/update", 'name': sensori['name'], 'value': sensori['value']}
                        element['status_update']['status'][str(i)] = {"topic": "rt/measures/" + sensori['measure'] + "/" +
                                                                      payload['mac'] + "/status", 'name': sensori['name'], 'value': sensori['status']}
                        if sensori['value'] == "open":
                            wet_api_call.windowOpened(payload['mac'])
                        else:
                            if sensori['value'] == "close":
                                wet_api_call.windowClosed(payload['mac'])
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
                            data_for_response['observes'][sensori] = "rt/measures/temperaturehumidity"
                        else:
                            if sensori == "alert":
                                data_for_response['observes'][sensori] = "rt/alert/#"
                            else:
                                if sensori == "deviceStatus":
                                    data_for_response['observes'][sensori] = "rt/devices/+/+/status"
                                else:
                                    data_for_response['observes'][sensori] = "rt/measures/" + \
                                        sensori + \
                                        "/+/update"
                        cur.execute(
                            "INSERT INTO observe(MAC,  measure) VALUES (%s, %s)", (payload['mac'], sensori))
                        mysql.connection.commit()
                y = json.dumps(data_for_response)
                # prova vedere se posso accedere
                mqtt_array[len(mqtt_array)-1] = element
                time.sleep(1)
                mqtt.publish("rt/discovery/response", y)
                for single_slice in mqtt_array:
                    for elemento in single_slice['status_update']['update']:
                        mqtt.subscribe(
                            single_slice['status_update']['update'][elemento]['topic'])
                        mqtt.subscribe(
                            single_slice['status_update']['status'][elemento]['topic'])
                for topic in far_alert_values:
                    mqtt.subscribe(topic)
                cur.execute(
                    "INSERT INTO log(MAC, event) VALUES (%s, %s)", (payload['mac'], 'in'))
                mysql.connection.commit()
                cur.close()
                return render_template("index.html", mqt=mqtt_array, mac_add=Mac_cancelled, lwill=lwill)
        else:
            if msg.topic in static_alert:
                static_alert[msg.topic]['value'] = my_json
                far_alert_values[msg.topic] = static_alert[msg.topic]
            for single_slice in mqtt_array:  # qui piuttosto che ascoltare su /+/ ascolto quelli che hi
                for elemento in single_slice['status_update']['update']:
                    if msg.topic == single_slice['status_update']['update'][elemento]['topic']:
                        single_slice['status_update']['update'][elemento]['value'] = my_json
                        ricerca_mac = ritornaMac(msg.topic)
                        if my_json == "close":  # parte relativa al tilt sensor
                            wet_api_call.windowClosed(ricerca_mac)
                        else:
                            if my_json == "open":
                                wet_api_call.windowOpened(ricerca_mac)
            # have to update status value from outside, that way we can perform updates via telegram
            for single_slice in mqtt_array:
                for elemento in single_slice['status_update']['status']:
                    if single_slice['status_update']['status'][elemento]['topic'] == msg.topic:
                        if my_json == "off":  # se il json è off
                            single_slice['status_update']['status'][elemento]['value'] = "off"
                            single_slice['status_update']['update'][elemento]['value'] = "0"
                            # just in case, it migth be a window
                            mac_scrittura = ritornaMac(msg.topic)
                            wet_api_call.windowClosed(mac_scrittura)
                        else:
                            single_slice['status_update']['status'][elemento]['value'] = "on"
            """
            aggiornamento delle treshold
            """
            for aggiorna_treshold in mqtt_array:
                if 'threshold' in aggiorna_treshold:
                    for cerca_tresh in aggiorna_treshold['threshold']:
                        if cerca_tresh['valueread'] == msg.topic:
                            cerca_tresh['value'] = my_json

            if msg.topic in far_alert_values:  # gestione delle update / value relative alle alert
                far_alert_values[msg.topic]['value'] = my_json
                # if msg.topic == "rt/alert/windowAlert":
            if msg.topic in for_status_devices:
                if my_json == "on":
                    mac_dispoitivo = ritornaMac(msg.topic)
                    for singolo in mqtt_array:
                        if 'deviceStatus' in singolo:
                            if singolo['deviceStatus']['mac'] == mac_dispoitivo:
                                singolo['deviceStatus']['status'] = my_json
                    wet_api_call.deviceOn(mac_dispoitivo)
                else:
                    if my_json == "off":
                        mac_dispoitivo = ritornaMac(msg.topic)
                        for singolo in mqtt_array:
                            if 'deviceStatus' in singolo:
                                if singolo['deviceStatus']['mac'] == mac_dispoitivo:
                                    singolo['deviceStatus']['status'] = my_json
                        wet_api_call.deviceOff(mac_dispoitivo)
                if wet_api_call.calculateActualAlertStatus():
                    wet_api_call.checkAlert()
            if wet_api_call.windowCounter.getCount() == 0:  # controllo il numero di finestrre per il alert
                if "api_message" in globals():
                    api_message = "fine"
            mqtt.publish("rt/counter/windowOpen",
                         wet_api_call.windowCounter.getCount())
        return url_for("do_discovery")


def ritornaMac(stringa):
    mac_ricavat = ""
    while stringa[-1] != "/":
        stringa = stringa[:-1]
    stringa = stringa[:-1]
    while stringa[-1] != "/":
        mac_ricavat += str(stringa[-1])
        stringa = stringa[:-1]
    return mac_ricavat[::-1]


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
    global api_message
    return render_template("index.html", mqt=mqtt_array, mac_add=a,
                           l_will=b, alert=alert, windowCount=wet_api_call.windowCounter.getCount(),
                           deviceCount=wet_api_call.deviceCounter.getCount(),
                           a_mes=api_message)


@ app.route('/', methods=['POST'])  # dati della treshold
def my_form_post():
    global api_message
    if 'cambio_valore' in request.form:  # <!-- se io ho on, il dispositivo è on -->
        prova_singola = request.form['cambio_valore']
        dove_pushare = request.form['invia_valore']
        for single_slice in mqtt_array:
            for elemento in single_slice['status_update']['status']:
                if single_slice['status_update']['status'][elemento]['topic'] == dove_pushare:
                    if prova_singola == "off":
                        single_slice['status_update']['status'][elemento]['value'] = "on"
                        single_slice['status_update']['update'][elemento]['value'] = "0"
                        mac_scrittura = ritornaMac(dove_pushare)
                        wet_api_call.windowClosed(mac_scrittura)
                        mqtt.publish(dove_pushare, "on",
                                     retain=True, qos=1)
                    else:
                        single_slice['status_update']['status'][elemento]['value'] = "off"
                        mqtt.publish(dove_pushare, "off",
                                     retain=True, qos=1)
                    # se spengo il device non dovrei poter avere il valore rilevato, dato che si è spento

    else:
        nome_riferimento = request.form['cambioVal']
        valore_publish = 0
        for element in mqtt_array:
            if 'threshold' in element:
                for row in element['threshold']:
                    if row['id'] == nome_riferimento:
                        row['value'] = request.form[nome_riferimento]
                        valore_publish = request.form[nome_riferimento]
        dove_publico = request.form['whertoPublish']
        mqtt.publish(dove_publico, valore_publish)
    global far_alert_values
    alert = far_alert_values
    return render_template("index.html", mqt=mqtt_array, alert=alert, windowCount=wet_api_call.windowCounter.getCount(), deviceCount=wet_api_call.deviceCounter.getCount(), a_mes=api_message)


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
                    wet_api_call.deviceOff(element['mac'])
                    cur.close()
                    return url_for("do_discovery")


if __name__ == "__main__":
    wet_api_call = WindowControl.WindowAlertHandler(mqtt)
    app.run(host='127.0.0.1', port=8181,
            debug=True, use_reloader=False)
