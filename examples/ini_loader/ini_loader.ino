//
// this code writes a SALT ini file to address zero ... in fram
// When using realterm it may be handy to create a shortcut with a variant of this setup string:
//		baud=115200 linedly=100 tab=send display=1 rows=60 fs=8 cols=80 port=10
// baud and linedly are required to be as specified above the others may be set to your personal preferences
//
// This tool is awkward to use.
//	1. compile and/or load this program into SALT
//	2. close arduino serial monitor window if open
//	3. open windows device manager so that you can see all of the ports (click the plus at ports)
//	4. start realterm
//	5. choose the the ini file (the ... button)
//	6. make sure that realterm's port is set to the same port as Teeny USB serial
//	7. close the port
//	9. press and release SALT reset
//	10. in realterm, open the port
//	11. in realterm, go to the Send tab and click the Send File button to send the ini file to SALT
//

#include <Systronix_MB85RC256V.h>
#include <SALT_settings.h>
#include <SALT_FETs.h>
#include <SALT.h>
#include <SALT_utilities.h>
#include <SALT_JX.h>

#include "ini_loader.h"				// not actually a shared file but should be exactly the same as the file used by ini_loader_SD.ino

Systronix_MB85RC256V fram;
SALT_settings settings;
SALT_utilities utils;

SALT_FETs FETs;						// to turn of lights fans, alarm
SALT_JX coreJ2;						// to turn off heat pads, lamps, drawer locks
SALT_JX coreJ3;
SALT_JX coreJ4;

//---------------------------< D E F I N E S >----------------------------------------------------------------

#define		END_OF_FILE		0xFFFF

#define		SYSTEM	0xDF
#define		USERS	0xEF

#define		STOP	0
#define		START	1
#define		TIMING	0xFFFFFFFF

//---------------------------< P A G E   S C O P E   V A R I A B L E S >--------------------------------------

char		rx_buf [8192];
char		out_buf [8192];
char		ln_buf [256];
//uint16_t	err_cnt = 0;
uint16_t	total_errs = 0;
uint16_t	warn_cnt = 0;
char		pins[USERS_MAX_NUM+1][6] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};


//---------------------------< C O M M O N _ F U N C T I O N S >----------------------------------------------
//
// apparently must be a .h file; compiler can't find the same file in the same location if it has a .inl extension
//
// Functions in the common file are shared with ini_loader_SD
//

#include	"ini_loaders_common.h"


//---------------------------< S E R I A L _ G E T _ C H A R S >----------------------------------------------
//
// Fetches characters from the Serial usb port and writes them into rx_buf.  CRLF ('\r\n') is converted to LF
// ('\n').  Returns the number of characters received.  Times-out after one second of serial inactivity.
// If the function receives enough characters to fill the allotted space in fram then something is not right;
// null terminates the buffer and returns the number of characters received.
//

uint16_t serial_get_chars (char* rx_ptr)
	{
	uint16_t	char_cnt = 0;
	time_t		last_char_time = millis();		// initialize to current time so we don't immediately fall out
	char		c;								// temp holding variable
	
	while (1)
		{
		while (Serial.available())				// while stuff to get
			{
			c = Serial.read();					// read and save the byte
			last_char_time = millis();			// reset the timer
			if (EOL_MARKER == c)
				{
				char_cnt++;						// tally
				*rx_ptr++ = c;					// save and bump the pointer
				*rx_ptr = EOF_MARKER;			// add end-of-file whether we need to or not
				*(rx_ptr+1) = '\0';				// null terminate whether we need to or not
				
				Serial.print (".");				// show that we're received a line
				continue;
				}
			else if ('\r' == c)					// carriage return
				{
				char_cnt ++;					// tally
				continue;						// but don't save; we only want the newline as a line marker
				}
			else
				{
				char_cnt++;						// tally
				*rx_ptr++ = c;					// save and bump the pointer
				*rx_ptr = EOF_MARKER;			// add end-of-file whether we need to or not
				*(rx_ptr+1) = '\0';				// null terminate whether we need to or not
				}

			if (8191 <= char_cnt)	// rcvd too many chars? not a SALT ini file?
				{
				*rx_ptr = '\0';					// null terminate rx_buf
				return char_cnt;
				}
			}
		
		while (!Serial.available())				// spin here waiting for more characters or a time-out
			{
			if (1000 < (millis()-last_char_time))
				return char_cnt;				// timed out; exit
			}
		}
	}


//---------------------------< S E T U P >--------------------------------------------------------------------

void setup()
	{
	pinMode(PERIPH_RST, OUTPUT);
	digitalWrite(PERIPH_RST, LOW);				// resets asserted
	digitalWrite(PERIPH_RST, HIGH);				// resets released
	FETs.setup (I2C_FET);						// constructor for SALT_FETs, and PCA9557
	FETs.begin ();
	FETs.init ();								// lights, fans, and alarms all off
	
	coreJ2.setup (I2C_J2);						// heat pads, lamps, drawlocks all off
	coreJ3.setup (I2C_J3);
	coreJ4.setup (I2C_J4);
	
	coreJ2.begin ();
	coreJ3.begin ();
	coreJ4.begin ();
	
	coreJ2.init ();
	coreJ3.init ();
	coreJ4.init ();
	
	coreJ2.JX_data.outdata.as_u32_word = 0;		// all drawers unlocked
	coreJ2.update();
	coreJ3.JX_data.outdata.as_u32_word = 0;		// heatlamps and heat pads off
	coreJ3.update();
	coreJ4.JX_data.outdata.as_u32_word = 0;		// heatlamps and heat pads off
	coreJ4.update();
	
	fram.setup (0x50);
	fram.begin ();								// join i2c as master
	fram.init();

	Serial.begin(115200);						// usb; could be any value
	while((!Serial) && (millis()<10000));		// wait until serial monitor is open or timeout

	Serial.print ("NAP ini loader: ");
	Serial.print ("build time: ");				// assemble
	Serial.print (__TIME__);					// the
	Serial.print (" ");							// startup
	Serial.print (__DATE__);					// message

	if (!fram.control.exists)
		{
		Serial.println ("fatal error: cannot communicate with fram");
		Serial.println("loader stopped; reset to restart");		// give up and enter an endless
		while(1);								// loop
		}
	}


//---------------------------< L O O P >----------------------------------------------------------------------
//
//
//

void loop()
	{
	uint16_t	crc;
	uint16_t	rcvd_count;
	uint32_t	millis_prev;
	time_t		elapsed_time;
	uint8_t		waiting_counter = 0;
	uint8_t		ret_val;
	uint8_t		heading = 0;
	char*		rx_ptr = rx_buf;				// point to start of rx_buf
	char*		out_ptr = out_buf;				// point to start of out_buf
//	char*		ln_ptr;							// pointer to ln_buf

	memset (out_buf, EOF_MARKER, 8192);			// fill out_buf with end-of-file markers

	for (uint8_t i=1; i<=USERS_MAX_NUM; i++)	// loop through pins array and reset them to zero length
		pins[i][0] = '\0';

	millis_prev = millis();						// init
	Serial.print ("\r\n\r\nloader> waiting for file ...");
	while (!Serial.available())
		{
		if (10000 < (millis() - millis_prev))
			{
			millis_prev = millis();				// reset
			Serial.print ("\r\nloader> waiting for file (");
			Serial.print (waiting_counter++);
			Serial.print (") ...");
			}
		}
	Serial.println ("\r\nreceiving");
	stopwatch (START);
	rcvd_count = serial_get_chars (rx_buf);

	elapsed_time = stopwatch (STOP);				// capture the time
	Serial.print ("\r\nrecieved ");
	Serial.print (rcvd_count);						// number of characters received
	Serial.print (" characters in ");
	Serial.print (elapsed_time);					// elapsed time
	Serial.println ("ms");

	settings.line_num = 0;							// reset to count lines taken from rx_buf
	Serial.println ("\r\nchecking");
	stopwatch (START);								// reset

	rx_ptr = rx_buf;								// initialize
	while (rx_ptr)
		{
		rx_ptr = array_get_line (ln_buf, rx_ptr);	// returns null pointer when no more characters in buffer
		if (!rx_ptr)
			break;
	
		if (EOF_MARKER == *ln_buf)						// when we find the end-of-file-marker
			{
			*out_ptr = *ln_buf;						// add it to out_buf
			break;									// done reading rx_buf
			}

		settings.line_num ++;						// tally

		if (('\r' == *ln_buf) || (EOL_MARKER == *ln_buf))	// do these here so we have source line numbers for error messages
			continue;								// cr or lf; we don't save newlines in fram
		if (strstr (ln_buf, "#"))
			continue;								// comment; we don't save comments in fram

		ret_val = normalize_kv_pair (ln_buf);		// trim, spaces to underscores; returns ptr to null terminated string

		if (ret_val)								// if an error or a heading (otherwise returns SUCCESS)
			{
			if (INI_ERROR == ret_val)				// not a heading, missing assignment operator
				settings.err_msg ((char *)"not key/value pair");
			else									// found a new heading
				{
				heading = ret_val;					// so remember which heading we found
				out_ptr = array_add_line (out_ptr, ln_buf);		// copy to out_buf; reset pointer to eof marker
				}
			continue;								// get the next line
			}

		if (SYSTEM == heading)						// validate the various settings according to their headings
			check_ini_system (ln_buf);
		else if (HABITAT_A == heading)
			check_ini_habitat_A (ln_buf);
		else if (HABITAT_B == heading)
			check_ini_habitat_B (ln_buf);
		else if (HABITAT_EC == heading)
			check_ini_habitat_EC (ln_buf);
		else if (USERS == heading)
			check_ini_users (ln_buf);
		}

	check_min_req_users ();							// there must be at minimum 1 each leader, service, & it tech user

	elapsed_time = stopwatch (STOP);				// capture the time
	Serial.print ("\r\nchecked ");
	Serial.print (settings.line_num);				// number of lines
	Serial.print (" lines in ");
	Serial.print (elapsed_time);					// elapsed time
	Serial.print ("ms; ");
	if (total_errs)									// if there have been any errors
		{
		Serial.print (total_errs);
		Serial.print (" error(s); ");
		Serial.print (warn_cnt);					// number of warnings
		Serial.println (" warning(s); ");
		Serial.println ("configuration not written.");
		}
	else
		{
		Serial.print ("0 error(s); ");				// no errors
		Serial.print (warn_cnt);					// number of warnings
		Serial.println (" warning(s).");
		}

	if (warn_cnt && !total_errs)					// warnings but no errors
		{
		if (!utils.get_user_yes_no ((char*)"loader", (char*)"ignore warnings and write settings to fram?", false))		// default answer no
			{
			total_errs = warn_cnt;					// spoof to prevent writing fram
			Serial.println("\n\nabandoned");
			}
		}

	if (!total_errs)								// if there have been errors, no writing fram
		{
		write_settings_to_out_buf (out_buf);		// write settings to the output buffer

		Serial.println ("\r\nerasing fram settings");
		stopwatch (START);						// reset
		utils.fram_fill (EOF_MARKER, FRAM_SETTINGS_START, (FRAM_SETTINGS_END - FRAM_SETTINGS_START + 1));
		elapsed_time = stopwatch (STOP);			// capture the time

		Serial.print ("\r\nerased ");
		Serial.print (FRAM_SETTINGS_END - FRAM_SETTINGS_START + 1);		// number of bytes
		Serial.print (" fram bytes in ");
		Serial.print (elapsed_time);		// elapsed time
		Serial.println ("ms");


		settings.line_num = 0;						// reset
		Serial.println ("\r\nwriting");

		stopwatch (START);

		fram.set_addr16 (FRAM_SETTINGS_START);		// set starting address where we will begin writing
		
		out_ptr = out_buf;							// point to the buffer
		while (out_ptr)
			{
			out_ptr = array_get_line (ln_buf, out_ptr);		// returns null pointer when no more characters in buffer
			if (!out_ptr)
				break;
			settings.line_num ++;							// tally
			fram.control.wr_buf_ptr = (uint8_t*)ln_buf;
			fram.control.rd_wr_len = line_len ((char*)ln_buf);
			fram.page_write();								// write it
			Serial.print (".");
			}
		
		fram.control.wr_byte = EOF_MARKER;			// write the EOF marker
		fram.byte_write();

//		fram.set_addr16 (0);						// set fram control block starting address
//		fram.byte_read();

//-----
// once stable, remove this bit of code
//		memset (ln_buf, '\0', 256);
//		fram.set_addr16 (0);					// set fram control block starting address
//		fram.control.wr_buf_ptr = (uint8_t*)ln_buf;
//		fram.control.rd_wr_len = 256;
//		fram.page_write();						// erase the control block
//-----
		elapsed_time = stopwatch (STOP);			// capture the time

		Serial.print ("\r\nwrote ");
		Serial.print (settings.line_num);			// number of lines
		Serial.print (" lines to fram in ");
		Serial.print (elapsed_time);				// elapsed time
		Serial.println ("ms");

		stopwatch (START);
		crc = get_crc_array ((uint8_t*)out_buf);	// calculate the crc across the entire buffer

		if (settings.get_crc_fram (FRAM_SETTINGS_START) == crc)	// calculate the crc across the settings in fram
			{
			fram.set_addr16 (FRAM_CRC_LO);				// set address for low byte of crc
			fram.control.wr_int16 = crc;
			fram.int16_write();
			}
		else
			{
			if (utils.get_user_yes_no ((char*)"loader", (char*)"crc match failure.  dump settings from fram?", true))	// default answer yes
				utils.fram_hex_dump (0);
			Serial.println("\r\ncrc match failure. loader stopped; reset to restart");	// give up an enter and endless
			while(1);									// loop
			}
	
		elapsed_time = stopwatch (STOP);
<<<<<<< Updated upstream
		Serial.printf ("\r\ncrc value (0x%.4X) calculated and written to fram in %dms", crc, elapsed_time);
=======
		Serial.print ("\r\ncrc value (");
		Serial.printf ("0x%X", crc);
		Serial.print (") calculated and written to fram in ");
		Serial.print (elapsed_time);					// elapsed time
		Serial.println ("ms");
>>>>>>> Stashed changes
			
		Serial.print("\r\n\r\nfram settings write complete\r\n\r\n");

		if (utils.get_user_yes_no ((char*)"loader", (char*)"dump settings from fram?", true))	// default answer yes
			{
//			settings.dump_settings ();					// dump the settings to the monitor
			utils.fram_hex_dump (0);
			}
		
		Serial.println("\n\ndone");
		}

	if (utils.get_user_yes_no ((char*)"loader", (char*)"dump fram log memory?", true))	// default answer yes
		utils.fram_hex_dump (FRAM_LOG_START);
		
	if (utils.get_user_yes_no ((char*)"loader", (char*)"initialize fram log memory?", false))		// default answer no
		{
		Serial.println ("\r\ninitializing fram log memory");
		stopwatch (START);								// reset
		utils.fram_fill (EOF_MARKER, FRAM_LOG_START, (FRAM_LOG_END - FRAM_LOG_START + 1));
		elapsed_time = stopwatch (STOP);				// capture the time

		Serial.print ("\r\nerased ");
		Serial.print (FRAM_LOG_END - FRAM_LOG_START + 1);		// number of bytes
		Serial.print (" fram bytes in ");
		Serial.print (elapsed_time);					// elapsed time
		Serial.println ("ms");
		}

	if (!utils.get_user_yes_no ((char*)"loader", (char*)"load another file", false))	// default answer no
		{
		Serial.println("\r\nloader stopped; reset to restart");				// give up and enter an endless
		while(1);										// loop
		}
	memset (rx_buf, '\0', 8192);						// clear rx_buf to zeros
	for (uint8_t i=0; i<=USERS_MAX_NUM; i++)
		{
		user[i].name[0] = '\0';							// make sure that the user list is reset
		user[i].pin[0] = '\0';
		user[i].rights[0] = '\0';
		}
	total_errs = 0;										// reset these so they aren't misleading
	warn_cnt = 0;
	}
