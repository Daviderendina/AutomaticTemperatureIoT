import logging
import paho.mqtt.client as mqtt
import json
import asyncio
import requests
import sys
import datetime
from telegram import Update, ReplyKeyboardMarkup
from telegram.ext import Updater, CommandHandler, CallbackQueryHandler, CallbackContext, MessageHandler, Filters
logging.basicConfig(
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s', level=logging.INFO
)
logger = logging.getLogger(__name__)
api_topic = "rt/api/weather"
alert_api_topic = "rt/alert/weather"
mqtt_array = []
far_alert_values = {}
# i need the client to publish outside the on_publish method
save_chat_id = []
save_chat_id = ["315683328"]
for_status_devices = {}
counter_windowOpen = 0
static_alert = {}
static_alert["rt/alert/windowOpen"] = {
    'name': "Alert window open:", 'value': "off"}


def start(update: Update, context: CallbackContext) -> None:
    """Sends a message with three inline buttons attached."""
    string = " 🤖 This bot will let you perform some task that were been available only via webserver. 🤖 \n" +\
        "\n" +\
        "You will also get notified if there is an alert on the going! Be sure to use /help " +\
        "so you can get a tour of the main functionalities you can perform."
    if 'save_chat_id' in globals():
        global save_chat_id
        if not str(update.message.chat.id) in save_chat_id:
            save_chat_id.append(str(update.message.chat.id))
        print(save_chat_id)
    update.message.reply_text(string)


def readMeasures(update: Update, context: CallbackContext) -> None:
    if 'save_chat_id' in globals():
        global save_chat_id
        if not str(update.message.chat.id) in save_chat_id:
            save_chat_id.append(str(update.message.chat.id))
        print(save_chat_id)
    string = "From here you can perform two different task.\n" +\
        "\n" +\
        "Either you just ask all the measures now availble via /getMeasure, " +\
        "or you could use the following signature!\n/getMeasure <MAC-address> <Name> \n\n Value of interest are listed below. \n\n"
    for valore in mqtt_array:
        if 'status_update' in valore:
            if valore['status_update']['update'] != {}:
                string += "🔷 Mac address" + valore['mac'] + "\n"
            for elemento_singolo in valore['status_update']['update']:
                if valore['status_update']['update'] != {}:
                    string += "   🔹" + \
                        valore['status_update']['update'][elemento_singolo]['name'] + "\n"
    update.message.reply_text(string)


def threshold(update: Update, context: CallbackContext) -> None:
    if 'save_chat_id' in globals():
        global save_chat_id
        if not str(update.message.chat.id) in save_chat_id:
            save_chat_id.append(str(update.message.chat.id))
        print(save_chat_id)
    """Sends a message with three inline buttons attached."""
    string = "To change a specific threshold value you will have to use the following signature!\n\n" +\
        "/changethreshold <MAC-address> <Threashold Name> <value> \n\n Value of interest are listed below. \n\n"
    for valore in mqtt_array:
        if 'threshold' in valore:
            string += "🔷 Mac address: " + valore['mac'] + "\n"
            for i, cerca_tresh in enumerate(valore['threshold']):
                string += "   🔹" + cerca_tresh['id'] + "\n"
            string + "\n"
    update.message.reply_text(string)


def getMeasures(update: Update, context: CallbackContext) -> None:
    if 'save_chat_id' in globals():
        global save_chat_id
        if not str(update.message.chat.id) in save_chat_id:
            save_chat_id.append(str(update.message.chat.id))
        print(save_chat_id)
    return_string = ""
    print(len(context.args))
    if len(context.args) == 2 or len(context.args) == 3:
        """
        context.args[0] = MAC
        context.args[1] = name
        """
        for single_slice in mqtt_array:
            if 'status_update' in single_slice:
                for elemento in single_slice['status_update']['update']:
                    if single_slice['mac'] == str(context.args[0]):
                        save_string = str(context.args[1])
                        if len(context.args) == 3:
                            save_string = str(
                                context.args[1]) + " " + str(context.args[2])
                        if single_slice['status_update']['update'][elemento]['name'] == save_string:
                            return_string += "📍" + save_string + ": " + \
                                single_slice['status_update']['update'][elemento]['value']

    else:  # se non viene specificato alcun parametro allora ritorno tutte le misure
        for single_slice in mqtt_array:
            for elemento in single_slice['status_update']['update']:
                return_string += "📍 " + single_slice['status_update']['update'][elemento]['name'] + \
                    ": " + \
                    single_slice['status_update']['update'][elemento]['value'] + "\n"
    if return_string == "":
        return_string = "❌ Something went wrong!"
    update.message.reply_text(return_string)


def handeStatusDevice(update: Update, context: CallbackContext) -> None:
    if 'save_chat_id' in globals():
        global save_chat_id
        if not str(update.message.chat.id) in save_chat_id:
            save_chat_id.append(str(update.message.chat.id))
        print(save_chat_id)
    string = "🚦 To change a specific sensors status you will have to use the following signature!🚦 \n\n" +\
        "/sensorStatus <MAC-address> <Name> [<on;off>] \n\n Value of interest are listed below. \n\n"
    for valore in mqtt_array:
        if 'status_update' in valore:
            for elemento_singolo in valore['status_update']['update']:
                if valore['status_update']['update'] != {}:
                    string += valore['mac'] + " " + \
                        valore['status_update']['update'][elemento_singolo]['name'] + "\n"
    update.message.reply_text(string)


def sensorStatus(update: Update, context: CallbackContext) -> None:
    if 'save_chat_id' in globals():
        global save_chat_id
        if not str(update.message.chat.id) in save_chat_id:
            save_chat_id.append(str(update.message.chat.id))
        print(save_chat_id)
    return_string = "no way"
    if len(context.args) == 3 or len(context.args) == 4:
        """
        context.args[0] = MAC
        context.args[1] = name
        context.args[2] = [on;off]
        """
        for single_slice in mqtt_array:
            for elemento in single_slice['status_update']['status']:
                if single_slice['mac'] == str(context.args[0]):
                    i = 0
                    string = str(context.args[1])
                    if len(context.args) == 4:
                        i = 1
                        string = str(context.args[1]) + \
                            " " + str(context.args[2])
                    if single_slice['status_update']['status'][elemento]['name'] == string:
                        if str(context.args[2+i]) == "off":
                            single_slice['status_update']['status'][elemento]['value'] = "on"
                            single_slice['status_update']['update'][elemento]['value'] = "0"
                            client.publish(
                                single_slice['status_update']['status'][elemento]['topic'], "off", retain=True, qos=1)
                            return_string = "✅ Status changed!"
                        else:
                            single_slice['status_update']['status'][elemento]['value'] = "on"
                            client.publish(
                                single_slice['status_update']['status'][elemento]['topic'], "on", retain=True, qos=1)
                            return_string = "✅ Status changed!"
        if return_string == "":
            return_string = "❌ Something went wrong!"
    else:
        return_string = "❌ Wrong parameters"
    update.message.reply_text(return_string)


def changethreshold(update: Update, context: CallbackContext):
    if 'save_chat_id' in globals():
        global save_chat_id
        if not str(update.message.chat.id) in save_chat_id:
            save_chat_id.append(str(update.message.chat.id))
        print(save_chat_id)
    return_string = ""

    if len(context.args) == 3:
        """
        context.args[0] = MAC
        context.args[1] = threshid
        context.args[2] = newvalue
        """
        for valore in mqtt_array:
            if 'threshold' in valore:
                print("Contengo threshold")
                if valore['mac'] == str(context.args[0]):
                    for cerca_tresh in valore['threshold']:
                        if cerca_tresh['id'] == str(context.args[1]):
                            cerca_tresh['value'] = int(context.args[2])
                            client.publish(
                                cerca_tresh['valueread'], int(context.args[2]))
                            return_string = "✅ Threshold set successfully!"
        if return_string == "":
            return_string = "❌ Something went wrong!"
    else:
        return_string = "❌ Wrong parameters"
    update.message.reply_text(return_string)


def help_command(update: Update, context: CallbackContext) -> None:
    if 'save_chat_id' in globals():
        global save_chat_id
        if not str(update.message.chat.id) in save_chat_id:
            save_chat_id.append(str(update.message.chat.id))
        print(save_chat_id)
    """Displays info on how to use the bot."""
    string = "What do you want to perform via bot? \n" + \
        "⚙️ Set a threshold: /threshold \n" + \
        "📵 Turn off a single sensor: /changeStatus \n" + \
        "🔎 Read a measurment: /readMeasures \n"
    update.message.reply_text(string)


def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("rt/discovery")
    client.subscribe("rt/counter/windowOpen")
    client.subscribe("rt/alert/windowOpen")

# The callback for when a PUBLISH message is received from the server.


def on_message(client, userdata, msg):
    my_json = msg.payload.decode('utf8')
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
            for i, element in enumerate(mqtt_array):
                if payload['mac'] == element['mac']:
                    mqtt_array.pop(i)
                    print("Present value")
        except ValueError as e:
            print(my_json)
            present = 1
            print("We do not catch exception inside bot")
            last_will_dispositivo(my_json)
        if present == 0:
            mqtt_array.append(payload)
            element = mqtt_array[len(mqtt_array)-1]
            data_for_response = {"mac": payload['mac'],
                                 "status": "rt/devices/"+payload['type']+"/"+payload['mac']+"/status",
                                 "threshold": {},  # le uso per modificare le soglie
                                 "sensors": {},  # le uso per poter fare on/off dei singoli sensori
                                 "alert": {}  # used to catch alert
                                 }

            element[data_for_response["status"]
                    ] = data_for_response["status"]
            for_status_devices[data_for_response["status"]] = "off"
            print(data_for_response["status"])
            for topic_dei_status in for_status_devices:
                client.subscribe(topic_dei_status)
            if 'threshold' in payload:
                for i, treash in enumerate(element['threshold']):
                    data_for_response['threshold'][treash['id']] = "rt/devices/" +\
                        payload['type']+"/threshold/"+payload['mac'] + \
                        "/"+payload['threshold'][i]['id']
                    element['threshold'][i]['valueread'] = data_for_response['threshold'][treash['id']]
                    """
                    In questa parte mi faccio la subscribe a tutti i threshold, in tal modo posso fare soglie da Telegram
                    """
                    client.subscribe(element['threshold'][i]['valueread'])
            if 'sensors' in payload:
                for i, sensori in enumerate(element['sensors']):
                    data_for_response['sensors'][sensori['id']] = {"status": "rt/measures/" + sensori['measure'] + "/" + payload['mac'] + "/status", "update": "rt/measures/" +
                                                                   sensori['measure'] + "/" + payload['mac'] + "/update"}
                    element['status_update']['update'][str(i)] = {"topic": "rt/measures/" + sensori['measure'] + "/" +
                                                                  payload['mac'] + "/update", 'name': sensori['name'], 'value': sensori['value']}
                    element['status_update']['status'][str(i)] = {"topic": "rt/measures/" + sensori['measure'] + "/" +
                                                                  payload['mac'] + "/status", 'name': sensori['name'], 'value': sensori['status']}
            if 'alert' in payload:
                for i, singoloAlert in enumerate(element['alert']):
                    data_for_response['alert'][singoloAlert] = "rt/alert/" + \
                        singoloAlert
                    global far_alert_values
                    far_alert_values["rt/alert/" + singoloAlert] = {
                        'name': singoloAlert, 'value': "off"}
            # prova vedere se posso accedere
            mqtt_array[len(mqtt_array)-1] = element
            print(element)
            for single_slice in mqtt_array:
                for elemento in single_slice['status_update']['update']:
                    client.subscribe(
                        single_slice['status_update']['update'][elemento]['topic'])
                    client.subscribe(
                        single_slice['status_update']['status'][elemento]['topic'])
            for topic in far_alert_values:
                client.subscribe(topic)
    else:
        if msg.topic in static_alert:
            static_alert[msg.topic]['value'] = my_json
            far_alert_values[msg.topic] = static_alert[msg.topic]
        """
        aggiornamento delle threshold
        """
        for aggiorna_threshold in mqtt_array:
            if 'threshold' in aggiorna_threshold:
                for cerca_tresh in aggiorna_threshold['threshold']:
                    if cerca_tresh['valueread'] == msg.topic:
                        cerca_tresh['value'] = my_json
        """
        Se rilevo un alert lo devo far sapere subito su Telegram
        """
        for single_slice in mqtt_array:  # qui piuttosto che ascoltare su /+/ ascolto quelli che hi
            for elemento in single_slice['status_update']['update']:
                if msg.topic == single_slice['status_update']['update'][elemento]['topic']:
                    single_slice['status_update']['update'][elemento]['value'] = my_json

        for single_slice in mqtt_array:
            for elemento in single_slice['status_update']['status']:
                if msg.topic == single_slice['status_update']['status'][elemento]['topic']:
                    if my_json == "off":
                        single_slice['status_update']['status'][elemento]['value'] = "off"
                        single_slice['status_update']['update'][elemento]['value'] = "0"
                    else:
                        single_slice['status_update']['status'][elemento]['value'] = "on"
        if msg.topic in far_alert_values:  # gestione delle update / value relative alle alert
            far_alert_values[msg.topic]['value'] = my_json
            string_to_send = "⚠️ We catched an alert for you! ⚠️\n\n" + \
                far_alert_values[msg.topic]['name'] + " is now " + str(my_json)
            data = {'chat_id': save_chat_id[0],
                    'text': string_to_send}
            telegram_url = f'https://api.telegram.org/bot{tele_token}/sendMessage'
            if 'save_chat_id' in globals():
                for singolo in save_chat_id:
                    data['chat_id'] = singolo
                    requests.get(telegram_url, params=data)
            else:
                requests.get(telegram_url, params=data)
        if msg.topic in for_status_devices:
            for ricerca_mac_notifica in mqtt_array:
                if msg.topic in ricerca_mac_notifica:
                    # should send an alert to Telegram
                    if my_json:
                        string_to_send = ricerca_mac_notifica['mac'] + \
                            " is now offline."
                    else:
                        string_to_send = ricerca_mac_notifica['mac'] + \
                            " is now online."
                        updater.bot.sendMessage(
                            chat_id=save_chat_id, text=string_to_send)
        if msg.topic == "rt/counter/windowOpen":
            global counter_windowOpen
            counter_windowOpen = int(my_json)


def last_will_dispositivo(some_data):
    spliting = some_data.split(" ")
    for element in mqtt_array:
        if spliting[0] == element['mac']:
            mqtt_array.pop(mqtt_array.index(element))


def on_publish(client, userdata, mid):
    print("mid: "+str(mid))


async def api_waiter():
    weatherapi_url = "https://api.openweathermap.org/data/2.5/onecall?lat=51.30&lon=0.7&exclude=current,minutely,daily,alerts&appid=92545a4cef2a7d3e637aa8d08b537e93"
    weatherapi_resultnum = 10
    while True:
        print(save_chat_id)
        data = {'chat_id': save_chat_id[0],
                'text': 'Sta piovendo, ma fortunatamente hai la finestra chiusa!'}
        telegram_url = f'https://api.telegram.org/bot{tele_token}/sendMessage'
        resp = requests.get(weatherapi_url)
        ora_da_attendere = 15
        await asyncio.sleep(ora_da_attendere)
        if resp.status_code != 200:
            print("API ERROR CODE: ", resp.status_code)
        else:
            flag = True
            for weather_hour in resp.json()["hourly"][0:weatherapi_resultnum]:
                dt = datetime.datetime.fromtimestamp(
                    int(weather_hour["dt"])).strftime('%H:%M')

                weatherID = weather_hour["weather"][0]["id"]
                global counter_windowOpen
                # manca il controllo delle cose aperte
                if(weatherID < 800 and counter_windowOpen > 0 and flag == True):
                    flag = False
                    data['text'] = " 🌧 Will start raining at " + \
                        dt + ", be sure to close your windows."
                    client.publish(api_topic, data['text'])
                    client.publish(alert_api_topic, "on")
                    if 'save_chat_id' in globals():
                        for singolo in save_chat_id:
                            data['chat_id'] = singolo
                            requests.get(telegram_url, params=data)
                    else:
                        requests.get(telegram_url, params=data)
        ora_da_attendere = 60*10
        await asyncio.sleep(ora_da_attendere)
        # return dt  # posso fare la chiamata qui


def setup():
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_publish = on_publish
    # client.will_set(topic, payload=None, qos=0, retain=False)
    client.username_pw_set(mqtt_username, mqtt_password)

    client.connect(mqtt_broker_ip, 1883, 60)
    """Run the bot."""
    # Create the Updater and pass it your bot's token.
    updater.dispatcher.add_handler(CommandHandler('start', start))
    updater.dispatcher.add_handler(CommandHandler('threshold', threshold))
    updater.dispatcher.add_handler(
        CommandHandler('changeStatus', handeStatusDevice))
    updater.dispatcher.add_handler(
        CommandHandler('readMeasures', readMeasures))
    return_measures = CommandHandler('getMeasure', getMeasures)
    updater.dispatcher.add_handler(return_measures)

    updater.dispatcher.add_handler(CommandHandler('help', help_command))
    thresholdHandler = CommandHandler('changethreshold', changethreshold)
    updater.dispatcher.add_handler(thresholdHandler)

    monitoring_status = CommandHandler('sensorStatus', sensorStatus)
    updater.dispatcher.add_handler(monitoring_status)
    try:
        updater.start_polling()
        client.loop_start()
        asyncio.run(api_waiter())
    except KeyboardInterrupt:
        print('Interrupted')
        sys.exit(0)
    # Start the Bot

    updater.idle()
    # Run the bot until the user presses Ctrl-C or the process receives SIGINT,
    # SIGTERM or SIGABRT


if __name__ == '__main__':
    # 1711171627:AAGVd05VBThvokJL8-69f_hBNLBxYiO1FMY"
    tele_token = "1708266260:AAHmTaSg_yEE0zWJTcHVlPifooSBaGaNJR4"
    updater = Updater(tele_token)
    mqtt_username = "ataraboi"
    mqtt_password = "iot829904"
    mqtt_broker_ip = "149.132.178.180"
    client = mqtt.Client()
    setup()
# API
# mando messaggio, e qui posso fare le scelte [si][no] con le callback
# se dico si, allora mando il chiudiFinestra (da pensare come)
