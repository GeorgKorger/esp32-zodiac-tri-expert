/*
TODO
DONE: start main loop even if wifi not connected
reconnect if wifi disconnect
DONE: power rückmeldung with retain
? publish loglevel after boot 
DONE: dont store power 101 in flash
DONE: change dynamic loglevel via mqtt
DONE: reorder if abfrage for power topic
DONE: check event->data_len>0 before atoi
DONE: copy event->data to printBuf and terminate with \0 before atoi
DONE: try: only compare subtopic
DONE: publish power if received wrong value
DONE: if mqtt disconnect: unsubscribe power
DONE: if mqtt connect: publish power before subscribe to power 
Stop boost after x hours
Flash write only if values changed
publish extra bytes
DONE: remove example from example_connect
DONE: remove unnessecary code from example_connect
relais for pump and heater
keys for local on/off
local timers for on/off
some led`s
*/
//power on startup if flash is empty
#define DEFAULT_POWER 70

//delay on startup [s]
#define INIT_DELAY 5

//delay between tri-expert commands (minimal 10 sec!)
#define TRI_EXPERT_LOOP_DELAY 5

#if( TRI_EXPERT_LOOP_DELAY < 10 )
  #define TRI_EXPERT_LOOP_DELAY 10
#endif

// maximal Boost duration in minutes
#define BOOST_DURATION 5


//mqtt broker
#define MQTT_BROKER "mqtt://eli"

//client id of mqtt client
#define MQTT_CLIENT_ID "PumpBoothMaster"

/*
	AQUAL_WITHOUT_UART: after receiving fake data are inserted,
	so you can debug on chip without connection to tri-expert.
	Must also be defined when mocking.
*/
#define AQUAL_WITHOUT_UART

//blue led GPIO PIN 2
#define BLUE_LED 2
#define BLUE_LED_DELAY 100

//UART pins and parameters
//#define AQUAL_TXD (23)
#define AQUAL_TXD (5)
//#define AQUAL_RXD (22)
#define AQUAL_RXD (15)
//#define AQUAL_RTS (18)
#define AQUAL_RTS (18)
#define BAUD_RATE (9600)


