# Automatic Temperature IoT
This project has been developed for "Laboratorio Internet of Things" course of UniMiB Computer science master degree. The work has been done in group with @conerns.

Automatic Temperature IoT is a system that automatically handles temperature in an ambient such as a restaurant or pub. 
The system consists of a network of nodes that communicate with each other using MQTT. To date, the system consists of 4 different nodes: a Flask server, two NodeMCUs and a Telegram bot.

### DESCRIPTION ###

There's one NodeMCU that collects temperature and humidity and communicates everything on MQTT. Heating or cooling devices receive this temperature update and change their status if the temperature is too high or low. There's also one sensor able to notify when a window gets opened or closed. If at least one heating or cooling device is on while at least one window is open, the system raises an alert to the user. 
We also developed a little WebApp and one Telegram bot to interact with the system.

Further information about the system can be found in slides.pdf file.


### ASSIGNMENT ###
The idea is to build an IoT monitor system which includes all the technologies we have explored during the lessons: databases (mysql or influxdb), mqtt, smart networks, http/api, telegram, json, web server, low power consumption solutions, ecc.
Use the first and second assignments as starting point. You are free to design your project on the basis of your ideas, the only requirement is that the project should include all the technologies listed above. 
