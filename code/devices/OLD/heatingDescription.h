// Use @MAC@ and MAC address will be added automatically
// Use @TON@ string in SENSOR_DESCRIPTIon in threshold Ton value field 
// Use @TOFF@ string in SENSOR_DESCRIPTIon in threshold TOFF value field 

#define HT_DESCRIPTION "{ 'mac': '@MAC@', 'name': 'Heating', 'description': 'Sensor description', 'type': 'heating', 'observes': [ 'temperature' ], 'threshold': [ { 'id': 'TOFF', 'name': 'Temperatura spegnimento', 'type': 'Integer', 'value' : '@TOFF@' }, { 'id': 'Ton', 'name': 'Temperatura accensione', 'type': 'Integer', 'value' : '@TON@' } ] }"

#define HT_TON_THRESHOLD_DEFAULT 19   
#define HT_TOFF_THRESHOLD_DEFAULT 21  
