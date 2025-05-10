//power on startup if flash is empty
#define DEFAULT_POWER 70


//delay on startup [s]
#define INIT_DELAY 10

//delay between tri-expert commands
#define TRI_EXPERT_LOOP_DELAY 10

/*
//delay on startup [s]
#define INIT_DELAY 3

//delay between tri-expert commands
#define TRI_EXPERT_LOOP_DELAY 2
*/
/*
	For mocking:
		#define MOCKED
		"set(COMPONENTS main)" in root CMakeLists.txt after cmake_minimum_required
		"idf --preview set-target linux" in terminal
	For compilation:
		"idf set-target esp32<c6>" in terminal
		
*/
//#define MOCKED
/*
	AQUAL_WITHOUT_UART: after receiving fake data are inserted,
	so you can debug on chip without connection to tri-expert.
	Must also be defined when mocking.
*/
//#define AQUAL_WITHOUT_UART


//blue led GPIO PIN 2
#define BLUE_LED 2
#define BLUE_LED_DELAY 100

//UART pins and parameters
#define AQUAL_TXD (23)
#define AQUAL_RXD (22)
#define AQUAL_RTS (18)
#define BAUD_RATE (9600)

//Wifi Parameters (except credentials)
#define WIFI_MAXIMUM_RETRY  5
#define WIFI_BREAK_DURATION 5

