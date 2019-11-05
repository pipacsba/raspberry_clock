//---------------------- CLOCK.C --------------------------------------
/*
This clock.c is driving a 4x7 segment display (ht16K33v110) to display the time.
The dimming is set based on a light sensor value from TSL2561 (CS package!).
The display and the light sensor are connected to a raspberry pi 2 I2C outputs.

Inputs to call:
 - input 1: verbose settings 
			0: no output
			1: full output
			2: reduced output (only communication to the display)
 - input 2: save hh:min, lux value into a file
			0: no save
			1: save
 - input 3: enables the display light measurement function (the display displays an average value, not the time, and the dimming changes in every minute)
			any >0 integer can be used as enable
Neither of the input are mandatory, but only verosity can be defined solely.
e.g.: 
	./clock - no output to standard out or to file
	./clock 0 1 - output to file, but not to standard output
	./clock 1 - output to standard output, but not to file
	
to compile:
	 gcc -Wall clock.c -lpaho-mqtt3c -lm -li2c -o clock
	 
for cygwin:
	gcc -c  clock.c -g -O3 -D noI2C -DNDEBUG  -o .//clock.c.o -I. -I.
	gcc -o clock @"RaspberryPI_clock.txt" -L. -LC:\\cygwin64\\usr\\local\\lib  -lpaho-mqtt3c
*/
//--------------------------------------------------------------------

//---------------------- INCLUDES AND DEFINES --------------------------------------

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include "MQTTClient.h"

#ifndef noI2C
	#include <sys/ioctl.h>
	#include <linux/i2c-dev.h>
	#include <i2c/smbus.h>
	#define I2C_DEV_H_INCLUDED
#endif
#ifdef noI2C
	#include <I2C_DEV_Fake\i2c-dev_fake.h>
	#define I2C_DEV_H_INCLUDED
#endif


//Defines for MQTT
#ifndef noI2C
	#define ADDRESS     "tcp://192.168.17.118:1883"
#endif
#ifdef noI2C
	#define ADDRESS     "tcp://10.58.193.203:1883"
#endif
#define CLIENTID    "ExampleClientPub"
#define TOPIC       "clock/light"
#define QOS         0
#define TIMEOUT     5000L

//---------------------- END OF INCLUDES AND DEFINES -------------------------------

//----------------------------STRUCTURE DEFINITIONS--------------------

/* LIGHT SENSOR MEASUREMENT STRUCT
create a structure to contain the measured sensor values and the calculated lux
	s_ir : infrared value
	s_broadband  : broadband value
	lux: calculated lux value
*/
struct light_sensor_data
{
	int s_ir, s_broadband;
	float lux;
};

/* DISPLAY MEMORY VALUES STRUCT
create a structure to contain all the display memory values which needs refresh in every minute
	displ_h1 : hour first digit
	disp_h2  : hour second digit
	disp_min1: minute first digit
	char disp_min2: minute second digit
	char disp_dim : dimming
*/
struct disp_refresh_values
{
	unsigned char disp_h1, disp_h2, disp_min1, disp_min2, disp_dim;
};

/* SUN-RISE CALCULATION STRUCT
this structure is defined to provide the output of the sun-set sun-rise calculation
	set_hour: is the hour of the given day when the sun sets
	set_min : is the minute when the sun sets
	rise_hour: is the hour of the given day when the sun rises
	rise_min : is the minute when the sun rises
*/
struct sunup
{
	int set_hour, set_min, rise_hour, rise_min;
};

/* DIMMING VALUE STRUCT
this sturcture contains the dimming values
  lightchange: represents the direction of the dimming chage (-1: decrease; 0: no change; 1:increase)
  currlight  : the current dimming value (0..15)
  dimming_max: maximum dimming value (15)
  dimming_min: minimum dimming value (0)
*/
struct display_dimming
{
	int lightchange;
	unsigned char currlight;
	unsigned char dimming_max;
	unsigned char dimming_min;
};

//---------------------END OF STRUCTURE DEFINITIONS--------------------

//------------------------FUNCTION DECLARATIONS------------------------

/* FUNCTION: GET_HEX_CODE
sub-function is created to get hex code for a single digit =meaning this shall be called four times for HH:MM format
 input
		anum: a number value between 0-900000000
output
		integer with the value representing the digit anum on the display memory
*/
unsigned char get_hex_code(int anum);

/* FUNCTION: DISPLAY_INIT
sub-function is created to turn on and turn off the display
 inputs
		onoff: 1 to turn the display and the oscillator on; 0 to turn it off
		verbose: 1, or greater integer if you want to have messages on the standard output
 output: ----
*/
int display_init(unsigned char onoff,int file, int verbose);

/* FUNCTION: SENSOR_INIT
sub-function is created to turn on and turn off the display
 inputs
		onoff: 1 to turn the display and the oscillator on; 0 to turn it off
		verbose: 1, or greater integer if you want to have messages on the standard output
 output: ----
*/
int sensor_init(unsigned char onoff,int file, int verbose);

/* FUNCTION: READ_LUX_VALUES
sub-function is created to read lux values for dimming from file
	file name is stored in lux_file variable
		the file shall contain lux dimming pairs in each row, both as integer values
	input: pointer to the array to be filled
*/
void read_lux_values(int * lux_array, char * filepath);

/* FUNCTION: GET_DISPL_VALUES
sub-function to update the display content
 inputs:
	displ_h1 - memory content of the first character of the hour
	displ_h2 - memory content of the second character of the hour
	disp_min1 - memory content of the first character of the minute
	disp_min2 - memory content of the second character of the minute
 output: ---
*/
struct disp_refresh_values get_displ_values(struct tm *a_tm,unsigned char currlight, int verbose);

/* FUNCTION: DISPLAY_UPDATE
this function send the defined values to the display device via the I2C bus
 inputs:
	adisp_refresh_values	:	contains the register values of the display segments
	verbose					:	if 1 some information will be sent to the standard output
*/
int display_update(struct disp_refresh_values adisp_refresh_values, int file, int verbose);

/* FUNCTION: UPDATE_DIMMING
this function is responsible to modify the current dimming settings in function of the current time
inputs:
 struct tm *a_tm					: including the current time
 struct display_dimming adimming	: including the previous dimming settings
 struct sunup thissunup				: including the sun-set sun-rise times to know when the dimming needs to change
 verbose							: gives some text to the standard output
output								: including the current dimming settings (with memory)
*/
struct display_dimming update_dimming(struct tm *a_tm,struct display_dimming adimming,struct sunup thissunup, int verbose);

/* FUNCTION: UPDATE_DIMMING_BY_LUX
this function is responsible to modify the current dimming settings in function of the measured lux
inputs:
 int lux							: measured lux value
 struct display_dimming adimming	: including the previous dimming settings
 int * lux_array					: look-up table for dimming vs lux
output								: including the current dimming settings (with memory)
*/
struct display_dimming update_dimming_by_lux(int lux, int * lux_array, struct display_dimming adimming, int verbose);


/* FUNCTION: CALCULATE_SUN_UP
To calculate the sun-set and sun-rise times for a given location, on the current day this function should be called
Necessary inputs:
Ln_deg: north longitudial coordinate, as double (for locations south to the equador it is a negative value)
Lw_deg: west latituial coordinate, as double (for location east to Greenwich it is a negative value)
verbose: if it is set to 1, some intermediate information will be printed to the standard output (printf)
Output will be provided as struct sunup
*/
struct sunup calculate_sun_up(double Ln_deg,double Lw_deg, int verbose);

/* FUNCTION: PROGRAM_SLEEP
this subfunction calles the nanosleep() function for the defined seconds
Input:
	sec: the seconds till the program needs to sleep
	verbose: writes slept time to the standard output
*/
void program_sleep(float sec, int verbose);

/* FUNCTION OPENI2C_BUS
This function opens the I2C bus
Inputs:
	adapter_nr: integer, describes which I2C bus to be opened
	verbose: puts success information to the standard output
	address: address of the device
Output:
	variable to store I2C bus handle
*/
int openI2C_bus(int adapter_nr,unsigned char address, int verbose);

/* FUNCTION: GET_UTC_CORRECTION
this function converts the time-zone and summer time information to a single hour difference between UTC and the system time
Input:
	struct tm *a_tm: including the current time
Output:
	hour difference between UTC and the system time
*/
int get_UTC_correction(struct tm *a_tm);

/* FUNCTION: MEASURE_LUX
this functional reads the light sensor measured data, and calls the lux calculation, than returns the calculated lux
Input:
	int file: file descriptor of the light sensor
	verbose: puts information to the standard output
Output:
	struct light_sensor_data: measured data and calculated lux
*/
struct light_sensor_data measure_lux(int file, int verbose);

/* FUNCTION: CALCULATE_LUX
this function calculates the lux value from the measured light sensor data
Input:
	float broadband: bradband sensor measured value
	float ir: infrared sensor measured value
Output:
	float lux	: the calculated lux value
*/
float calculate_lux(float broadband, float ir);

// stuff for detecting keypress
int getkey();
int is_key_pressed(void);

// stuff to properly shutdown the process
void term(int signo);

//------------------END OF FUNCTION DECLARATIONS------------------------

//-------------------------CONSTANTS------------------------------------
// define adapter number of I2C bus
const unsigned char adapter_nr = 1;
// Define display I2C address
const unsigned char disp_address = 0x70;
// display internal address of first character
const unsigned char Hour1_address = 0x00;
// display internal address of second character
const unsigned char Hour2_address = 0x02;
// display internal address of third character
const unsigned char Min1_address = 0x06;
// display internal address of fourth character
const unsigned char Min2_address = 0x08;
// display internal address of colon character
const unsigned char Colon_address = 0x04;
// define constant for maximum dimming value
const unsigned char MaxDimming = 15;

// create constant to define if light sensor shall be used
const int use_light_sensor = 1;
// Sensor I2C address
const unsigned char sensor_address = 0x39;
// Base Command value
const unsigned char Sensor_command = 0x80;
// Command extension to read word
const unsigned char Sensor_Read_Word = 0x20;
// Command value to change power on/off
const unsigned char Sensor_Power = 0x0;
// The TIMING register defaults to 02h at power on. - which should be good, anyway to change it the command value extension
const unsigned char Sensor_Timing = 0x1; 
// Command value to change interrupt settings
const unsigned char Sensor_interrupt = 0x6;
// ADC Channel Data Registers (Ch - Fh)
const unsigned char Broadband_Low = 0xC;
const unsigned char Broadband_High = 0xD;
const unsigned char IR_Low = 0xE;
const unsigned char IR_High = 0xF;

// filename which conatains lux values for dimming
const char lux_file[] = "lux_dimming.txt";


//--------------------END OF CONSTANTS----------------------------------

//-------------------------GLOBAL VARIABLES-----------------------------
// variable created to handle external KILL signal
volatile sig_atomic_t done = 0;
// variable to handle display light measure status
volatile int display_light_meas = -1;
// variable to store argv[0] without passing it

//---------------------END OF GLOBAL VARIABLES--------------------------


int main (int argc, char *argv[])
{
	// prepare function to be killed properly
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = term;
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTRAP, &action, NULL);
	
	//allocate lux_path to calling command
	char lux_path[50];
	char filepath[50];
	snprintf(lux_path,50, "%s",argv[0]);
	//Get executable location from argv[0] (see main function), and append with lux_file
	char *s = strrchr(lux_path, '/');
	if (s) {
		*s = '\0';
	}
	//snprintf(filepath,50,"%s", lux_path);
	snprintf(filepath,50,"%s/%s", lux_path, lux_file);
	
	// create variables to have terminal messages
	int verbose = 0;
	// if the program is started with a number argument above or equal to 1, than turn on terminal messages
	if (argc > 1)
	{
		verbose = atol(argv[1]);
	}
	// second input parameter is used to define if lux values shall be save to file
	int savetofile = 0;
	if (argc > 2)
	{
		savetofile = atol(argv[2]);
	}
	if (argc > 3)
	{
		display_light_meas = 0;
	}	
	
	// define variable to store I2C bus handle
	int display_file_descriptor;
	// define variable to store I2C bus handle
	int sensor_file_descriptor;

	// open th I2C bus for the communication with the display
	display_file_descriptor = openI2C_bus(adapter_nr,disp_address, verbose);

	// open th I2C bus for the communication with the display
	sensor_file_descriptor = openI2C_bus(adapter_nr,sensor_address, verbose);
	int light_sensor_available = 0;
	if (sensor_file_descriptor > 0)
	{
		light_sensor_available = 1;
		if (verbose)
		{
			printf("Light sensor available\n");			
		}
	}
	int light_sensor_dead = 0;
	int light_sensor_dead_lim = 5;
	
	//set up MQTT
	char mqtt_payload[100];
	int MQTT_Connected = 0;
	MQTTClient client;
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token;

	MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	conn_opts.keepAliveInterval = 70;
	conn_opts.cleansession = 1;
	
	// memory allocation for lux based dimming
	int lux_values[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};  
	// is the lux value set up?
	int lux_read = 0;

	// The location coordinates (Budapest) as double
	double Ln_deg    = 47.5;
	double Lw_deg    = -19;

	// create a sunup type struct, with invalid (not HH:MM) values, {-1,-1,-1,-1}
	struct sunup thissunup={-1,-1,-1,-1};

	// create dimming status structure, and fill it with initial values
	//		adimming.lightchange=0;
	//		adimming.currlight=0;
	//		adimming.dimming_max=15;
	//		adimming.dimming_min=0;
	// used for sun-set based calculation
	struct display_dimming adimming={0, 0, MaxDimming, 0};

	// create structure variable for display refresh values
	struct disp_refresh_values adisp_refresh_values;

	// create time management structure to get the current time and the used timezone, summer time information
	struct tm *a_tm;

	// define variable for pressed key
	int key;

	// create a variable to show if this is the first entering to minute change "if", and to show that in this case there is no need for a 55sec wait afterwards
	int dontwait=1;
	int sleep_sec = 50;

	// define variable for I2C bus read/wrtie event results
	int res=0;
	int disp_status;

	// create int to store previous time value, set it to a not a minute value (e.g. 66)
	int amin=66;
	
	// define variable to store lux value
	float lux = 0.0;
	struct light_sensor_data ls_data;
	ls_data.lux = 0.0;
	ls_data.s_ir = 0;
	ls_data.s_broadband = 0;

	// Turn on display
	disp_status=display_init(1, display_file_descriptor, verbose);
	if ((disp_status < 0) && verbose)
	{
		printf("DISPLAY INIT FAILED\n");
	}
	
	// Turn on sensor
	res=sensor_init(1, sensor_file_descriptor, verbose);
	if ((res < 0) && verbose)
	{
		printf("LIGHT SENSOR INIT FAILED\n");
	}
	
	// continous operation (while(1))
	// done parameter can be changed by application kill signal
	while(!done)
	{
		// create variable where the time information will be stored [type: time_t]
		time_t now=time(NULL);
		// get the current time from the system
		a_tm = localtime(&now);

		// if it is 4 o'clock in the morning, or the sunset is not yet calculated, than let's calculate it
		if (!((light_sensor_available == 1) && (use_light_sensor == 1)))
		{
			if (((a_tm->tm_hour == 4) && (a_tm->tm_min == 0))||(thissunup.set_hour == -1))
			{
				// calculate sun-set and sun-rise times
				thissunup=calculate_sun_up(Ln_deg, Lw_deg,verbose);

				if (verbose == 1)
				{
					printf("sun set is expected at %02d:%02d\n", thissunup.set_hour, thissunup.set_min);
					printf("sun rise is expected at %02d:%02d\n", thissunup.rise_hour, thissunup.rise_min);
				}

				// if currlight is not yet initialized
				if (dontwait)//(currlight==-1)
				{
					// if the current time is smaller or equal than the sun-rise time, or higher than the sun-set time, than
					if (((a_tm->tm_hour*100+a_tm->tm_min) <= (thissunup.rise_hour*100+ thissunup.rise_min)) ||
						((a_tm->tm_hour*100+a_tm->tm_min) > (thissunup.set_hour*100+ thissunup.set_min)))
					{
						// set the display light to minimum
						adimming.currlight=adimming.dimming_min;
					}
					else
					{
						// else set it to the maximum
						adimming.currlight=adimming.dimming_max;
					}
				}
			}
		}
		else
		{
			if (((a_tm->tm_hour == 4) && (a_tm->tm_min == 0)) || (lux_read == 0))
			{
				read_lux_values(lux_values, filepath);
				if (verbose)
				{
					printf("Lux file read: %s\n", filepath);
					printf("The lux values are: ");
					for(int i = 0; i <= MaxDimming; i++) {
						printf("%d ", lux_values[i]);
					} 
					printf("\n");
				}
				lux_read = 1;
			}
		}
		
		// if current minute is not equal to the previous
		if (amin != a_tm->tm_min)
		{
			// store previous minute value
			amin=a_tm->tm_min;

			// define current dimming settings
			if (!((light_sensor_available == 1) && (use_light_sensor == 1)))
			{
				// if light sensor is not availbale or not to be used, based on sunup/sunrise
				adimming=update_dimming(a_tm,adimming,thissunup, verbose);
			}
			else
			{
				// else based on the sensor reading
				adimming=update_dimming_by_lux(lux, lux_values, adimming, verbose);
			}
			// define memory values for the display
			adisp_refresh_values=get_displ_values(a_tm, adimming.currlight,verbose);

			// Set display content and dimming
			disp_status= display_update(adisp_refresh_values, display_file_descriptor, verbose);
			if ((disp_status < 0) && verbose)
			{
				printf("DISPLAY UPDATE FIALED\n");
			}
			
			if (verbose)
			{
				printf("The hour is: %02d, display code is %#.2x;%#.2x, result: %d \n",a_tm->tm_hour,adisp_refresh_values.disp_h1,adisp_refresh_values.disp_h2,res);
				printf("The minute is: %02d, display code is %#.2x;%#.2x, result: %d \n",a_tm->tm_min,adisp_refresh_values.disp_min1,adisp_refresh_values.disp_min2,res);
				printf("display dimming is set to %d, display memory to set: %#.2x, result: %d \n", adimming.currlight, adisp_refresh_values.disp_dim,res);
			}
			
			// if light sensor failure occured, than try restart the light sensor
			if (light_sensor_dead == light_sensor_dead_lim)
			{
				res = sensor_init(0, sensor_file_descriptor, verbose);
				if (res >= 0)
				{
					program_sleep(0.5,verbose);
					res=sensor_init(1, sensor_file_descriptor, verbose);
				}
				if (res >= 0)
				{
					lux = 0.0;
				}
				else
				{
					light_sensor_dead = 0;
				}
			}
			
			//Do MQTT connect, publish and disconnect
			//first check if the client is connected, if not then connect
			if (light_sensor_available == 1)
			{	
				if (MQTTClient_isConnected(client) == 1)
				{
					MQTT_Connected = 1;
					if (light_sensor_dead == light_sensor_dead_lim)
					{
						MQTT_Connected = 3; // tried to restart sensor
					}
					if (verbose)
					{
						printf("MQTT is connected\n");
					}
				}
				else
				{
					MQTT_Connected = 0;
					 if (MQTTClient_connect(client, &conn_opts) == MQTTCLIENT_SUCCESS)
					 {
						 MQTT_Connected = 2;
						if (verbose)
						{
							printf("MQTT is reconnected\n");
						}
					 }
				}
				if (MQTT_Connected >= 1)
				{
					snprintf(mqtt_payload,100,"{\"lux\": %.5f, \"dimming\": %i, \"mqtt\": %i, \"ir\": %i, \"broadband\": %i, \"disp_err\": %i}",
						lux, adimming.currlight, MQTT_Connected, ls_data.s_ir, ls_data.s_broadband, disp_status);
					pubmsg.payload = mqtt_payload;
					pubmsg.payloadlen = strlen(mqtt_payload);
					pubmsg.qos = QOS;
					pubmsg.retained = 1;
					MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
					MQTTClient_waitForCompletion(client, token, 5000);
				}
			}
			
			// get sleep time
			a_tm = localtime(&now);
			int StopSleepBeforeSec = 58;
			if (a_tm->tm_sec < StopSleepBeforeSec)
			{
				sleep_sec= StopSleepBeforeSec - a_tm->tm_sec;
			}
			else
			{
				sleep_sec= 1;
			}
			
			if (dontwait == 0)
			{
				program_sleep(sleep_sec,verbose);
				// measure lux value after the long wait if communication with the sensor is OK
				if (light_sensor_available == 1)
				{
					//some low-pass filtering on lux value ~4min (y += alpha * ( x - y ) )
					ls_data = measure_lux(sensor_file_descriptor, verbose);
					lux = lux + ((ls_data.lux - lux) / 4.0f);
					// if communication is not OK, disable usage of sensor
					if (lux < 0.0)
					{
						lux = 0.0;
					}
					if (lux < 0.01)
					{
						light_sensor_dead = light_sensor_dead + 1;
						if (light_sensor_dead > light_sensor_dead_lim +1)
						{
							light_sensor_dead = light_sensor_dead_lim +1;
						}
					}
					else
					{
						light_sensor_dead = 0;
					}
				}
				if (verbose)
				{
					printf("The measured lux is: %.4f\n", lux);
				}
				// if save to file lux and time parirs
			 	if (savetofile == 1)
				{
					// open file to append ("a")
					FILE *f = fopen("/run/user/1000/gvfs/smb-share:server=turrisnas,share=movies/scripts/light_sensor_data.txt", "a");
					if (f == NULL)
					{
						printf("File open failed\n");
					}
					else
					// print to file
					fprintf(f,"%02d:%02d,%.4f\n", a_tm->tm_hour, a_tm->tm_min, lux);
					// close file (as writing happens in 1 minute cycle, it might time out by the operating system, or something)
					fclose(f);
				}
			}
			else
			{
				// dontwait is only needed at startup
				dontwait = 0;
			}
		}

		// exit on key-press, only works if verbose > 1
		if (verbose && is_key_pressed())
		{
			key = getkey();
			printf("key pressed: %c \n",key);
			break;
		}
		// sleep for 0.9 second using nanosleep (to ensure that the next minute change will be detected within 1 second)
		program_sleep(0.2,verbose);
	}
	// Turn off display
	res = display_init(0, display_file_descriptor, verbose);
	if ((res < 0) && verbose)
	{
		printf("DISPLAY SHUTDOWN FAILED\n");
	}
	// Turn off sensor
	res = sensor_init(0, sensor_file_descriptor, verbose);
	if ((res < 0) && verbose)
	{
		printf("SENSOR SHUTDOWN FAILED\n");
	}
	//Disconnect and Destroy MQTT
	MQTTClient_disconnect(client, 10000);
	MQTTClient_destroy(&client);
	return 0;
}

/* FUNCTION OPENI2C_BUS
This function opens the I2C bus
Inputs:
	adapter_nr: integer, describes which I2C bus to be opened
	verbose: puts success information to the standard output
Output:
	file: variable to store I2C bus handle
*/
int openI2C_bus(int adapter_nr,unsigned char address, int verbose)
{
	int file=0;
	// if I2C bus option is on (compile time switch to support debugging)
	#ifdef I2C_DEV_H_INCLUDED
		int res=0;
		char filename[20];

		snprintf(filename, 19, "/dev/i2c-%d", adapter_nr);
		file = open(filename, O_RDWR);
		// if fake I2C header is used, than file opening will fail
		#ifdef I2C_INC_FAKE
			if (file < 0)
			{
				file = -file;
			}
		#endif
		if (file < 0)
		{
			// ERROR HANDLING; you can check errno to see what went wrong
			if (verbose == 1)
			{
				printf("FAILED OPENING I2C BUS\n");
			}
			exit(1);
		}
		else
		{
			// actually this linkes the address to the file
			res=ioctl(file, I2C_SLAVE, address);
			if ((res < 0) && verbose)
			{
				printf("OPENING CHANNEL FOR %#02x IS FAILED\n",address);
			}
		}
	#endif
	return file;
}

/* FUNCTION: SENSOR_INIT
 sub-function is created to turn on and turn off the light sensor, the function also turn off interrupts
 inputs
		onoff: 1 to turn the sensor on, 0 to turn the sensor off
		verbose: 1, or greater integer if you want to have messages on the standard output
*/
int sensor_init(unsigned char onoff, int file, int verbose)
{
	int res=0;
	int ares=0;
	unsigned char command = Sensor_command;
	// Power off value
	unsigned char power_command = 0x00; 
	if (onoff)
	{
		// Power on value
		power_command = 0x03; 
	}

	// Turn on/off light sensor power
	#ifdef I2C_DEV_H_INCLUDED
		// Using SMBus commands
		command = Sensor_command + Sensor_Power;
		res = i2c_smbus_write_byte_data(file, command, power_command);
		if (res < 0)
		{
			// ERROR HANDLING: i2c transaction failed
			ares=res;
		}
	#endif

	if (verbose == 1)
	{
		printf("Light sensor control register is set to %d, with result %d \n", power_command, res);
	}
	
	if (onoff)
	{
		// Turn off interrupts
		#ifdef I2C_DEV_H_INCLUDED
			// Using SMBus commands
			command = Sensor_command + Sensor_interrupt;
			res = i2c_smbus_write_byte_data(file, command, 0x0);
			if (res < 0)
			{
				// ERROR HANDLING: i2c transaction failed
				ares=res;
			}
		#endif
		if (verbose == 1)
		{
			printf("Light sensor interrupts are turned off, with result %d \n", res);
		}
	}
	
	return ares;
}


/* FUNCTION: DISPLAY_INIT
sub-function is created to turn on and turn off the display
 inputs
		onoff: 1 to turn the display and the oscillator on; 0 to turn it off
		verbose: 1, or greater integer if you want to have messages on the standard output
*/
int display_init(unsigned char onoff, int file, int verbose)
{
	unsigned char display_switch = 0x80+onoff;
	unsigned char display_osc    = 0x20+onoff;
	int res = 0;
	int ares = 0;

	#ifdef I2C_DEV_H_INCLUDED
		// Using SMBus commands
		res = i2c_smbus_read_byte_data(file, display_osc);
		if (res < 0)
		{
			// ERROR HANDLING: i2c transaction failed
			ares = res;
		}
	#endif
	if (verbose == 1)
	{
		printf("Display message set to %#.2x, with result %d \n",display_osc,res);
	}

	#ifdef I2C_DEV_H_INCLUDED
		// Using SMBus commands
		res = i2c_smbus_read_byte_data(file, display_switch);
		if (res < 0)
		{
			// ERROR HANDLING: i2c transaction failed
			ares = res;
		}
	#endif
	if (verbose == 1)
	{
		printf("Display message set to %#.2x, with result %d \n",display_switch,res);
	}

	if (onoff)
	{
		// Turn on the colon
		#ifdef I2C_DEV_H_INCLUDED
			// Using SMBus commands
			res = i2c_smbus_write_byte_data(file, Colon_address, 0x02);
			if (res < 0)
			{
				// ERROR HANDLING: i2c transaction failed
				ares = res;
			}
		#endif
		if (verbose == 1)
		{
			printf("Colon is turned on, with result %d \n",res);
		}
	}

	return ares;
}

/* FUNCTION: DISP_REFRESH_VALUES
sub-function to update the display content
 inputs:
	displ_h1 - memory content of the first character of the hour
	displ_h2 - memory content of the second character of the hour
	disp_min1 - memory content of the first character of the minute
	disp_min2 - memory content of the second character of the minute
 output: ---
*/
struct disp_refresh_values get_displ_values(struct tm *a_tm, unsigned char currlight, int verbose)
{
	struct disp_refresh_values adisp_refresh_values;
	// define display hex codes for HH:MM
	adisp_refresh_values.disp_h1 = get_hex_code(a_tm->tm_hour/10);
	adisp_refresh_values.disp_h2 = get_hex_code(a_tm->tm_hour%10);
	adisp_refresh_values.disp_min1 = get_hex_code(a_tm->tm_min/10);
	adisp_refresh_values.disp_min2 = get_hex_code(a_tm->tm_min%10);
	// get display hex code for dimming
	adisp_refresh_values.disp_dim = 0xE0+ currlight;

	if (display_light_meas >= 0)
	{
		// set the light to "average"
		adisp_refresh_values.disp_h1 = 0x1F;
		adisp_refresh_values.disp_h2 = 0x0F;
		adisp_refresh_values.disp_min1 = 0x0F;
		adisp_refresh_values.disp_min2 = 0x0F;
		adisp_refresh_values.disp_dim = 0xE0+ display_light_meas;
		// if verbose, behave so
		if (verbose)
		{
			printf("Current dimming value is: %02d, for measurement reason.\n",display_light_meas);
		}
		// increase the dimming value for the next cycle
		display_light_meas = display_light_meas +1;
		// but keep in mind that the maximum is 15
		if (display_light_meas == MaxDimming + 1)
		{
			display_light_meas = 0;
		}
	}
	
	return adisp_refresh_values;
}

/* FUNTION: DISPLAY_UPDATE
sub-function to update the display content
 inputs:
	displ_h1 - memory content of the first character of the hour
	displ_h2 - memory content of the second character of the hour
	disp_min1 - memory content of the first character of the minute
	disp_min2 - memory content of the second character of the minute
 output: ---
*/
int display_update(struct disp_refresh_values adisp_refresh_values, int file, int verbose)
{
	#ifdef I2C_DEV_H_INCLUDED
		int res = 0;
	#endif
	int ares = 0;

	// displ_h1,displ_h1
	#ifdef I2C_DEV_H_INCLUDED
		// Using SMBus commands
		res = i2c_smbus_write_byte_data(file, Hour1_address, adisp_refresh_values.disp_h1);
		if (res < 0)
		{
			// ERROR HANDLING: i2c transaction failed
			ares = res;
		}
	#endif
	#ifdef I2C_DEV_H_INCLUDED
		// Using SMBus commands
		res = i2c_smbus_write_byte_data(file, Hour2_address, adisp_refresh_values.disp_h2);
		if (res < 0)
		{
			// ERROR HANDLING: i2c transaction failed
			ares = res;
		}
	#endif

	// displ_h1,displ_h1
	#ifdef I2C_DEV_H_INCLUDED
		// Using SMBus commands
		res = i2c_smbus_write_byte_data(file, Min1_address, adisp_refresh_values.disp_min1);
		if (res < 0)
		{
			//ERROR HANDLING: i2c transaction failed
			ares = res;
		}
	#endif
	#ifdef I2C_DEV_H_INCLUDED
		// Using SMBus commands
		res = i2c_smbus_write_byte_data(file, Min2_address, adisp_refresh_values.disp_min2);
		if (res < 0)
		{
			// ERROR HANDLING: i2c transaction failed
			ares=res;
		}
	#endif

	//dimming
	#ifdef I2C_DEV_H_INCLUDED
		// Using SMBus commands
		res = i2c_smbus_read_byte_data(file, adisp_refresh_values.disp_dim);
		if (res < 0)
		{
			// ERROR HANDLING: i2c transaction failed
			ares=res;
		}
	#endif
	return ares;
}


/* FUNCTION: UPDATE_DIMMING
this function is responsible to modify the current dimming settings in function of the current time
inputs:
 struct tm *a_tm					: including the current time
 struct display_dimming adimming	: including the previous dimming settings
 struct sunup thissunup				: including the sun-set sun-rise times to know when the dimming needs to change
 verbose							: gives some text to the standard output
output								: including the current dimming settings (with memory)
*/
struct display_dimming update_dimming(struct tm *a_tm,struct display_dimming adimming,struct sunup thissunup, int verbose)
{
	struct display_dimming bdimming=adimming;
	// If the sunset is now, or dimming decrease ongoing
	if (((thissunup.set_hour == a_tm->tm_hour) && (thissunup.set_min == a_tm->tm_min)) || (bdimming.lightchange == -1))
	{
		if (verbose == 1)
		{
			printf("decrease dimming\n");
		}

		// If not yet on min light
		// Minimum dimming setting is 0
		if (bdimming.currlight > bdimming.dimming_min)
		{
			// decrease light
			if (verbose == 1)
			{
				printf("dimming-1\n");
			}
			bdimming.currlight--;
			// Set slightly passed
			bdimming.lightchange=-1;
		}
		else // If min light
		{
			// So further light change is required (dimming ongoing to be stopped)
			if (verbose == 1)
			{
				printf("stop dimming change\n");
			}
			bdimming.lightchange=0;
		}
	}
	// If the sunrise is now, or slightly passed
	else if (((thissunup.rise_hour == a_tm->tm_hour) && (thissunup.rise_min == a_tm->tm_min)) || (bdimming.lightchange == 1))
	{
		// If not yet on max light
		// Maximum dimming setting is 15
		if (bdimming.currlight < bdimming.dimming_max)
		{
			// increase light
			if (verbose == 1)
			{
				printf("dimming+1\n");
			}
			bdimming.currlight++;
			bdimming.lightchange = 1;
		}
		else // If max light
		{
			// So further light change is required
			if (verbose == 1)
			{
				printf("stop dimming change\n");
			}
			bdimming.lightchange = 0;
		}
	}

	return bdimming;
}

/* FUNCTION: UPDATE_DIMMING_BY_LUX
this function is responsible to modify the current dimming settings in function of the measured lux
inputs:
 int lux							: measured lux value
 struct display_dimming adimming	: including the previous dimming settings
 int * lux_array					: look-up table for dimming vs lux
output								: including the current dimming settings (with memory)
*/
struct display_dimming update_dimming_by_lux(int lux, int * lux_array, struct display_dimming adimming, int verbose)
{
	int dimming = 0;
	int hysteresis = 5; // [%]
	// for each possible dimming value (starting from the lowest)
	for (int i = 0; i <= MaxDimming; i++)
	{
		// if the current element of the lux_array is greater than the current lux
		// and this value is not 0 in the vector
		if ((lux > lux_array[i]) && (lux_array[i] > 0))
		{
			// than this is the required dimming value
			// at the end the highest dimming is selected which fulfills the criteria above
			dimming = i;
		}
	}
	// adding hysteresis for changing the dimming value
	// if dimming increases
	if (dimming > adimming.currlight)
	{
		// do not increase if the current light is below the hysteresis limit
		if (lux < (lux_array[dimming] * (100 + hysteresis) / 100))
		{
			dimming = adimming.currlight;
		}
	}
	if (verbose)
	{
		printf("Look-up table dimming: %d, for lux %d\n", dimming, lux);
	}
	
	// Create dimming status structure, and fill it with initial val
	//		bdimming.lightchange=0;
	//		bdimming.currlight=0;
	//		bdimming.dimming_max=15;
	//		bdimming.dimming_min=0; */
	struct display_dimming bdimming={0, 0, MaxDimming, 0};
	// set the dimming value as found above
	bdimming.currlight = dimming;
	return bdimming;
}

/* FUNTION: GET_HEX_CODE
sub-function is created to get hex code for a single digit =meaning this shall be called four times for HH:MM format
 input
	anum: a number value between 0-900000000
 output:
	integer with the value representing the digit anum on the display memory

Decimal value of each segment:
	------  1
	I    I
32	I    I  2
	I    I
	------  64
	I    I
16	I    I  4
	I    I
	------  8
	Value of decimal dot: 128=0x80
*/
unsigned char get_hex_code(int anum)
{
	switch (anum)
	{
		case 0:
			// 63 = 0x3F
			return 0x3F;
			break;
		case 1:
			// 6 = 0x06
			return 0x06;
			break;
		case 2:
			// 91 = 0x5B
			return 0x5B;
			break;
		case 3:
			// 79 = 0x4F
			return 0x4F;
			break;
		case 4:
			// 102 = 0x66
			return 0x66;
			break;
		case 5:
			// 109 = 0x6D
			return 0x6D;
			break;
		case 6:
			// 125 = 0x7D
			return 0x7D;
			break;
		case 7:
			// 7 = 0x07
			return 0x07;
			break;
		case 8:
			// 127 = 0x7F
			return 0x7F;
			break;
		case 9:
			// 111 = 0x6F
			return 0x6F;
			break;
		default:
			// nothin' = 0x00
			return 0x00;
			break;
	}
}

/* FUNTION: CALCULATE_SUN_UP
Calculates the sun-set and sun-rise times for a given location, on the current day
Necessary inputs:
Ln_deg: north longitudial coordinate, as double (for locations south to the equador it is a negative value)
Lw_deg: west latituial coordinate, as double (for location east to Greenwich it is a negative value)
verbose: if it is set to 1, some intermediate information will be printed to the standard output (printf)
The accuracy of this function is appx +-15minutes. If you would consider a more accurate value, than please consider using different code.
(An option for accuracy improvement could be to repeat the calculation of M_deg, C, lambda_deg and J_transit (noon_prev) recursively several times)
Due to the large number of Julian date, and the required precisity the used type is double during the calculation
*/
struct sunup calculate_sun_up(double Ln_deg,double Lw_deg, int verbose)
{
	// create time management structure to get the current time and the used timezone, summer time information
	struct tm *a_tm;
	// create variable where the time information will be stored
	time_t now = time(NULL);
	// outptt variable to store the reurn values
	struct sunup asunup;
	// calculate PI (3.14...) as double
	double aPI = acos(-1.0);
	// get the current time from the system
	a_tm = localtime(&now);
	// calculate current year
	int ayear = a_tm->tm_year+1900;
	// Calculate correction hours compared to UTC (Greenwich)
	int UTC_corr = get_UTC_correction(a_tm);

	// Calculate Julian date
	// first calculate the Jdate
	// number of years since 2000 multiplied by 365: (ayear-2000)*365
	// add the number of extra days (int)((ayear-2000)/4)
	// add the number of days in the current year: a_tm->tm_yday
	int Jdate = (ayear - 2000) * 365 + (int)((ayear - 2000) / 4) + 2451545 + a_tm->tm_yday;

	// Start of sunset sun-rise calculation, based on: http://users.electromagnetic.net/bu/astro/sunrise-set.php
	double Ln_rad    = Ln_deg*2*aPI/360;
	double n_        = (Jdate - 2451545 - 0.0009) - (Lw_deg/360);
	int    n         = round(n_);
	double noon_prev = 2451545 + 0.0009 + (Lw_deg/360) + n ;
	double M_deg     = fmodf (357.5291 + 0.98560028 * (noon_prev - 2451545),(float)360);
	double M_rad     = M_deg *2* aPI/360;
	double C 		 = (1.9148 * sin(M_rad)) + (0.0200 * sin(2 * M_rad)) + (0.0003 * sin(3 * M_rad)) ;
	double lambda_deg= fmodf(M_deg + 102.9372 + C + 180,360) ;
	double lambda_rad= lambda_deg *2* aPI/360;
	double J_transit = noon_prev + (0.0053 * sin(M_rad)) - (0.0069 * sin(2 * lambda_rad));
	double theta     = asin( sin(lambda_rad) * sin(23.45*2*aPI/360) );
	double H_rad     = acos( (sin(-0.83*2*aPI/360) - sin(Ln_rad) * sin(theta)) / (cos(Ln_rad) * cos(theta)) );
	double H_deg     = H_rad*360/2/aPI;
	double noon      = 2451545 + 0.0009 + ((H_deg + Lw_deg)/360) + n;
	double sunset    = noon + (0.0053 * sin(M_rad)) - (0.0069 * sin(2 * lambda_rad)) ;
	double sunrise   = J_transit - (sunset - J_transit) ;
	// End of sunset sun-rise calculation, based on: http://users.electromagnetic.net/bu/astro/sunrise-set.php


	// Calculate it to human readeable times in HH:MM format
	// Julian calendar contains the date as the integer number, the number after the decimal point refers to the time within the day
	//  so if the integer is removed from the Julian date and the result is multiplied with 24, than it is the hour within the Julian day
	// Julian calendar changes day at noon, so xx.0=12:00h, so to get a value in the afternoon, you have to add 12 to get the 24hour time format
	// Julian calendar gives the time according to UTC timezone so the result shuld be corrigated
	double set_hour_d = (12+(sunset-(int)sunset)*24)+UTC_corr;
	int    set_hour   = (int)set_hour_d;
	// similarly to the hour calculation, the value after the hour number represents the minutes within the hour, so multiplying with 60 gives the value in minutes
	int    set_min    = (int)((set_hour_d-set_hour)*60);

	// calculation of the sin-rise time in HH:MM format is similar to the calculation of the sun-set
	// except that you should extract 12 from the value
	double rise_hour_d = ((sunrise-(int)sunrise)*24-12)+UTC_corr;
	int    rise_hour   = (int)rise_hour_d;
	int    rise_min    = (int)((rise_hour_d-rise_hour)*60);

	if (verbose==1)
	{
		printf("timezone is: %s\n",a_tm->tm_zone);
		printf("hour difference to UTC is %d hours.\n",UTC_corr);
		printf("Jdate                      = %d\n", Jdate);
		printf("rounds around the sun      = %d\n", n);
		printf("prevision for noon (Jdate) = %f\n", noon_prev);
		printf("M                          = %f\n", M_deg);
		printf("C                          = %f\n", C);
		printf("lambda                     = %f\n", lambda_deg);
		printf("J_transit                  = %f\n", J_transit);
		printf("theta                      = %f\n", theta);
		printf("H                          = %f\n", H_deg);
		printf("noon                       = %f\n", noon);
		printf("sunset                     = %f\n", sunset);
		printf("sunrise                    = %f\n", sunrise);
	}
	asunup.set_hour=set_hour;
	asunup.set_min=set_min;
	asunup.rise_hour=rise_hour;
	asunup.rise_min=rise_min;

	return asunup;
}

/* FUNCTION: GET_UTC_CORRECTION
this function converts the time-zone and summer time information to a single hour difference between UTC and the system time
Input:
	struct tm *a_tm: including the current time
Output:
	hour difference between UTC and the system time
*/
int get_UTC_correction(struct tm *a_tm)
{
	int UTC_corr = 0;
	// if the system provided time zone is CET (Central Europian Time), or CEST (Central Europen Summer Time)
	if ((strcmp(a_tm->tm_zone,"CET") == 0) || (strcmp(a_tm->tm_zone,"CEST") == 0))
	{
		// CET from UTC: +1h
		UTC_corr++;
	}
	else
	{
		// place for other timezones if needed
		;
	} 
	if (a_tm->tm_isdst == 1)
	{
		// summer time adds an other hour from the UTC timezone
		UTC_corr++;
	}
	return UTC_corr;
}

/* FUNCTION: PROGRAM_SLEEP
this subfunction calles the nanosleep() function for the defined seconds
Input:
	sec: the seconds till the program needs to sleep
	verbose: writes slept time to the standard output
*/
void program_sleep(float sec, int verbose)
{
	// create nanosleep structure
	struct timespec ts;
	// convert input to nanosleep structure
	ts.tv_sec = (int)sec;
	ts.tv_nsec = (sec-(int)sec)*1000000000;
	// perform the sleep
	nanosleep(&ts, NULL);
	if (verbose == 1)
	{
		printf("slept for %g sec\n",sec);
	}
}

/* FUNCTION: MEASURE_LUX
this functional reads the light sensor measured data, and calls the lux calculation, than returns the calculated lux
Input:
	int file: file descriptor of the light sensor
	verbose: puts information to the standard output
Output:
	struct light_sensor_data: measured data and calculated lux
*/
struct light_sensor_data measure_lux(int file, int verbose)
{
	struct light_sensor_data measurement;
	int res = 0;
	float lux = 0.0;
	int broadband = 0;
	int ir = 0;
	float f_broadband = 0.0;
	float f_ir = 0.0;
	// set the gain value of the sensor if needed
	int gain = 1;
	// set the command values for the read
	int command_broadband = Sensor_command + Sensor_Read_Word + Broadband_Low;
	int command_ir = Sensor_command + Sensor_Read_Word + IR_Low;
	
	int command_gain = 0;
	if (gain == 16)
	{
		command_gain = Sensor_command + Sensor_Timing;
		#ifdef I2C_DEV_H_INCLUDED
		res = i2c_smbus_write_byte_data(file, command_gain, 0x12);
		if (res < 0)
		{
			// ERROR HANDLING: i2c transaction failed
			lux=res;
		}
		// wait for finish one measurement integration in the sensor (datasheet)
		program_sleep(0.402, verbose);
		#endif
	}
		
	// Read Broadband sensor value
	#ifdef I2C_DEV_H_INCLUDED
		// Using SMBus commands
		broadband = i2c_smbus_read_word_data(file, command_broadband);
	#endif
	// Read infrared sensor value
	#ifdef I2C_DEV_H_INCLUDED
		// Using SMBus commands
		ir = i2c_smbus_read_word_data(file, command_ir);
	#endif	
	// if ir or broadband is negative than something failed during the measurement
	if ((ir >= 0) && (broadband >= 0))
	{
		// if gain is not 16, that means gain = 1
		if (gain != 16)
		{
			// make conversion based on datasheet (the proposed calculation is for gain=16)
			f_broadband= (float)broadband * 16.0;
			f_ir = (float)ir * 16.0;
		}
		else
		{
			f_broadband= (float)broadband;
			f_ir = (float)ir;
		}
		// calculate lux from measured values
		lux = calculate_lux(f_broadband, f_ir);
	}
	measurement.s_ir = ir;
	measurement.s_broadband = broadband;
	measurement.lux = lux;
	return measurement;
}

/* FUNCTION: CALCULATE_LUX
this function calculates the lux value from the measured light sensor data
Input:
	float broadband: bradband sensor measured value
	float ir: infrared sensor measured value
Output:
	float lux	: the calculated lux value
*/
float calculate_lux(float broadband, float ir)
{
	// T, FN, and CL Package
	// For 0 < CH1/CH0	< 0.50 Lux = 0.0304 * CH0 - 0.062 * CH0*((CH1/CH0)^1.4)
	// For 0.50 < CH1/CH0 < 0.61 Lux = 0.0224 * CH0 - 0.031 * CH1
	// For 0.61 < CH1/CH0 < 0.80 Lux = 0.0128 * CH0 - 0.0153 * CH1
	// For 0.80 < CH1/CH0 < 1.30 Lux = 0.00146 * CH0 - 0.00112 * CH1
	// For CH1/CH0 > 1.30 Lux = 0
	float lux = 0.0;
	float ratio = 0.0;
	// calculate ration with division of zero protection
	if (broadband > 0.0) ratio = ir / broadband;
	// make the necessary calculations based on the database
	// For 0 < CH1/CH0	< 0.50 Lux = 0.0304 * CH0 - 0.062 * CH0*((CH1/CH0)^1.4)
	if ((ratio <= 0.50) && (broadband > 0.0))
	{
		lux = 0.0304 * broadband - 0.062 * broadband * (pow(ratio,1.4));
	}
	// For 0.50 < CH1/CH0 < 0.61 Lux = 0.0224 * CH0 - 0.031 * CH1
	else if ((ratio <= 0.61) && (broadband > 0.0))
	{
		lux = 0.0224 * broadband - 0.031 * ir;
	}
	// For 0.61 < CH1/CH0 < 0.80 Lux = 0.0128 * CH0 - 0.0153 * CH1
	else if ((ratio <= 0.8) && (broadband > 0.0))
	{
		lux = 0.0128 * broadband - 0.0153 * ir;
	}
	// For 0.80 < CH1/CH0 < 1.30 Lux = 0.00146 * CH0 - 0.00112 * CH1
	else if ((ratio <= 1.3) && (broadband > 0.0))
	{
		lux = 0.00146 * broadband - 0.00112 * ir;
	}
	// For CH1/CH0 > 1.30 Lux = 0
	else
	{
		lux = 0.02;
	}
	// if a calculation went out of limits, than set the lux value to 0
	if (isnan(lux))
	{
		lux = 0.02;
	}
	else if (lux < 0.02)
	{
		lux = 0.02;
	}
	return lux;
}

/* FUNCTION: READ_LUX_VALUES
sub-function is created to read lux values for dimming from file
	file name is stored in lux_file variable
		the file shall contain lux dimming pairs in each row, both as integer values
	input: pointer to the array to be filled
the result will contain as many elements as MaxDimming, and 
- for each defined dimming point the minimum lux level will be the contant
- will contain 0 if the input file do not specify lux value for the dimming
*/
void read_lux_values(int * lux_array, char * filepath)
{
    // reset all values to 0
	for (int i = 0; i <= MaxDimming; i++)
	{
		lux_array[i] = 0;
	}

	// open file to append ("a")
	FILE *f = fopen(filepath, "r");
	if (f == NULL)
	{
		printf("Lux-dimming file open failed\n");
	}
	else
	{
		// or other suitable maximum line size
		char line [ 128 ]; 
		// read a line
		while ( fgets ( line, sizeof line, f ) != NULL ) 
		{
			char *token;
			// Token will point to the part before the " ".
			token = strtok(line, " ");
			int alux = atoi(token);
			// Token will point to the part after the " ".
			token = strtok(NULL, " ");
			// convert token to number
			int adimming = atoi(token);
			lux_array[adimming] = alux;
		}
		// close file
		fclose(f);
	}
}


/* stuff to detect key press */
int getkey()
{
	int character;
	struct termios orig_term_attr;
	struct termios new_term_attr;

	// Set the terminal to raw mode
	tcgetattr(fileno(stdin), &orig_term_attr);
	memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
	new_term_attr.c_lflag &= ~(ECHO|ICANON);
	new_term_attr.c_cc[VTIME] = 0;
	new_term_attr.c_cc[VMIN] = 0;
	tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

	// read a character from the stdin stream without blocking
	//  returns EOF (-1) if no character is available
	character = fgetc(stdin);

	// Restore the original terminal attributes
	tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

	return character;
}

/* stuff to avoid the program from waiting for key press */
int is_key_pressed(void)
{
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);

	select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
	return FD_ISSET(STDIN_FILENO, &fds);
}

/* stuff to handle KILL request */
void term(int signo)
{
	done = 1;
}

/*
DATA TYPES:
Type 			Bits 	Possible Values
char 				8 		-127 to 127
unsigned char 		8 		0 to 255
short 				16 		-32,767 to 32,767
unsigned short 		16 		0 to 65,535
int 				32 		-2,147,483,647 to 2,147,483,647
unsigned int 		32 		0 to 4,294,967,295
long 				32 		-2,147,483,647 to 2,147,483,647
unsigned long 		32 		0 to 4,294,967,295
long long 			64 		-9,223,372,036,854,775,807 to 9,223,372,036,854,775,807
unsigned long long 	64 		0 to 18,446,744,073,709,551,615
float 				32 		1e-38 to 1e+38
double 				64 		2e-308 to 1e+308
long double 		64 		2e-308 to 1e+308
*/
