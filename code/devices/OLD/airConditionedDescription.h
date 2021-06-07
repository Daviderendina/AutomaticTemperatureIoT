// Use @MAC@ and MAC address will be added automatically

#define AC_DESCRIPTIon "{ 'mac': '@MAC@', 'name': 'Cooling', 'description': 'Sensor description', 'type': 'cooling', 'observes': [ 'temperaturehumidity'], 'threshold': [ { 'id': 'TMED', 'name': 'Temperatura media', 'type': 'Integer', 'value' : '@TMED@' }, { 'id': 'THIGH', 'name': 'Temperatura alta', 'type': 'Integer', 'value' : '@THIGH@' }, { 'id': 'HHIGH', 'name': 'Umidità alta', 'type': 'Integer', 'value' : '@HHIGH@' } ] }"

// Default value for threshold THIGH, which indicate the limit temperature for turning on air conditioned.
#define AC_TEMPERATURE_HIGH_DEFAULT 32  

// Default value for threshold TMED and HHIGH, which are used to check if dehumidifier must be turned on
#define AC_TEMPERATURE_MED_DEFAULT 28   
#define AC_HUMID_HIGH_DEFAULT 80        
