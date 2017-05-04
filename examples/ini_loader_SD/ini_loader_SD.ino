//
// this code writes a SALT ini file to address zero ... in fram
// When using realterm it may be handy to create a shortcut with a variant of this setup string:
//
// This version of ini loader reads an ini file from the uSD flash
//
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
#include <SPI.h>
#include <SdFat.h>

#include "ini_loader.h"				// not exactly a shared file but is the same file as used for ini_loader.ino

Systronix_MB85RC256V fram;
SALT_settings settings;
SALT_utilities utils;

SALT_FETs FETs;						// to turn of lights fans, alarm
SALT_JX coreJ2;						// to turn off heat pads, lamps, drawer locks
SALT_JX coreJ3;
SALT_JX coreJ4;

SdFat SD;
SdFile file;

//---------------------------< D E F I N E S >----------------------------------------------------------------

#define		END_OF_FILE		0xE0FF

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


//---------------------------< F I L E _ G E T _ C H A R S >--------------------------------------------------
//
// Fetches characters from the Serial usb port and writes them into rx_buf.  CRLF ('\r\n') is converted to LF
// ('\n').  Returns the number of characters received.  Times-out after one second of serial inactivity.
// If the function receives enough characters to fill the allotted space in fram then something is not right;
// null terminates the buffer and returns the number of characters received.
//

uint16_t file_get_chars (char* rx_ptr)
	{
	uint16_t	char_cnt = 0;
	char		c;								// temp holding variable
	
	while (file.available())				// while stuff to get
		{
		c = file.read();					// read and save the byte
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
	return char_cnt;
	}


/*******************************************
//---------------------------< H E X _ D U M P _ C O R E >----------------------------------------------------
//
// Render a 'page' (16 rows of 16 columns) of hex data and its character equivalent.  data_ptr MUST point to
// a buffer of at least 256 bytes.  This function always renders 256 items regardless of the size of the source.
// TODO: add support for 32-bit addressing

void hex_dump_core (uint16_t address, uint8_t* data_ptr)
	{
	uint8_t*	text_ptr = data_ptr;					// walks in tandem with data_ptr; prints the text
	
	for (uint8_t i=0; i<16; i++)						// dump 'pages' that are 16 lines of 16 bytes
		{
		Serial.print ("\r\n\r\n");						// open some space
		utils.hex_print (address);
		Serial.print ("    ");							// space between address and data

		
		for (uint8_t j=0; j<8; j++)						// first half
			{
			utils.hex_print (*data_ptr++);
			Serial.print (" ");							// space between bytes
			}
		Serial.print (" -  ");							// mid array separator

		for (uint8_t j=0; j<8; j++)						// second half
			{
			utils.hex_print (*data_ptr++);
			Serial.print (" ");							// space between bytes
			}
		Serial.print ("   ");							// space between hex and character data

		for (uint8_t j=0; j<16; j++)
			{
			if (' ' > *text_ptr)						// if a control character (0x00-0x1F)
				Serial.print ('.');						// print a dot
			else
				Serial.print ((char)*text_ptr);
			text_ptr++;									// bump the pointer
			}

		address += 16;
		address &= 0x7FFF;								// limit to 32k
		}
	}


//---------------------------< H E X _ D U M P _ A R R A Y >--------------------------------------------------
//
// print a hex dump of an internal array in 256 byte chunks.  If the array is smaller than 256 byte, this
// function will print whatever is in memory beyond the last element of the array.
//
// TODO: add an array length argument so that the function will only print pages that cover the array.
//

void hex_dump_array (uint8_t* array_ptr, size_t len)
	{
	uint16_t	index = 0;									// array index
	
	while (1)
		{
		Serial.print ("@ ");
		Serial.print ((uint32_t)array_ptr);
		Serial.print (" (0x");
		Serial.print ((uint32_t)array_ptr, HEX);
		Serial.print (")");
		hex_dump_core (index, array_ptr);

		Serial.println ("");									// insert a blank line
		array_ptr += 256;										// next 'page' starting address
		index += 256;											// and index

		if (len > index)
			{
			if (!utils.get_user_yes_no ((char*)"loader", (char*)"another page?", true))	// default answer yes
				return;
			}
		else
			{
			if (!utils.get_user_yes_no ((char*)"loader", (char*)"end of array; more?", false))	// default answer no
				return;
			}
		}
	}
*******************************************/


//---------------------------< S E T U P >--------------------------------------------------------------------

void setup()
	{
	Serial.begin(115200);						// usb; could be any value
	while((!Serial) && (millis()<10000));		// wait until serial monitor is open or timeout

												// make sure all spi chip selects inactive
	pinMode (FLASH_CS_PIN, INPUT_PULLUP);		// SALT FLASH_CS(L)
	pinMode (T_CS_PIN, INPUT_PULLUP);			// SALT T_CS(L)
	pinMode (ETH_CS_PIN, INPUT_PULLUP);			// SALT ETH_CS(L)
	pinMode (DISP_CS_PIN, INPUT_PULLUP);		// SALT DISP_CS(L)
	
	pinMode (uSD_DETECT, INPUT_PULLUP);			// so we know if a uSD is in the socket

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

	Serial.print ("NAP ini loader [SD]: ");
	Serial.print ("build time: ");				// assemble
	Serial.print (__TIME__);					// the
	Serial.print (" ");							// startup
	Serial.println (__DATE__);					// message

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

char file_list [11][64];							// a 1 indexed array of file names; file_list[0] not used


void loop()
	{
	uint16_t	crc;
	uint16_t	rcvd_count;
	time_t		elapsed_time;
	uint8_t		ret_val;
	uint8_t		heading = 0;
	char*		rx_ptr = rx_buf;					// point to start of rx_buf
	char*		out_ptr = out_buf;					// point to start of out_buf
	uint8_t		file_count;							// indexer into file_list; a 1-indexed array; file_list[0] not used
	
	memset (out_buf, EOF_MARKER, 8192);				// fill out_buf with end-of-file markers

	for (uint8_t i=1; i<=USERS_MAX_NUM; i++)		// loop through pins array and reset them to zero length
		pins[i][0] = '\0';

	if (digitalRead (uSD_DETECT))
		{
		Serial.println ("insert uSD");
		while (digitalRead (uSD_DETECT));
		delay (50);									// installed, now wait a bit for things to settle
		}
	
	Serial.println ("uSD installed");
	
	if (!SD.begin(uSD_CS_PIN, SPI_HALF_SPEED))
		{
		Serial.printf ("\r\nerror initializing uSD; cs: %d; half-speed mode\r\n", uSD_CS_PIN);
		SD.initErrorHalt("loader stopped; reset to restart\r\n");
		while (1);
		}

	Serial.println ("choose ini file to load:");

	file_count = 0;	
	SD.vwd()->rewind();									// rewind to start of virtual working idrectory
	while (file.openNext(SD.vwd(), O_READ))				// open next file for reading
		{
		if (!file.isFile())								// files only
			{
			file.close ();								// close this directory
			continue;									// from the top
			}
			
		file_count++;									// bump to next file list location
		file.getName (file_list[file_count], 32);		// get and save the file name
		ret_val = strlen (file_list[file_count]) - 4;			// make an index to the last four characters of the file name
		if (stricmp(&file_list[file_count][ret_val], ".ini"))	// if last four characters are not ".ini"
			{
			file_count--;								// not a .ini file so recover this spot in the list
			file.close();								// close the file
			continue;									// from the top
			}
		
//		Serial.printf ("\n%d ", file.fileSize());		// is this of any value?  Get it but don't list a file with length 0?
		Serial.printf (" [%2d] %-32s ", file_count, file_list[file_count]);		// print menu item, file name, file date
		file.printModifyDateTime(&Serial);				// and time/date  TODO: must we use this?  is there no better way to getfile  date and time?
		Serial.println ("");							// terminate the menu
		file.close();									// 

		if (10 <= file_count)							// only list 10 files
			break;
		}

	if (0 == file_count)
		{
		Serial.printf ("\r\nno .ini files found on uSD card.\r\nloader stopped; reset to restart");
		while (1);										// hang it up
		}
		
	while (1)
		{
		while (Serial.available())
			ret_val = Serial.read();					// if anything in the Serial input, get rid of it

		Serial.printf ("SALT/loader> ");				// print the SALT prompt and accept serial input

		while (!Serial.available());
		ret_val = Serial.read();
		Serial.write (ret_val);
		Serial.println ("");
		ret_val -= '0';									// convert text to number
		if ((0 < ret_val) && (ret_val <= file_count))
			{
			if (file.open (file_list[ret_val]))
				{
				Serial.printf ("\r\nreading %s\r\n", file_list[ret_val]);
				ret_val = 0xFF;
				}
			else
				Serial.printf ("\r\nfile %s did not open\r\n", file_list[ret_val]);
			break;
			}
		else
			Serial.printf ("\r\nselection must be between 1 and %d\r\n", file_count);
		}
		
	Serial.println ("\r\nreading");
	stopwatch (START);
	rcvd_count = file_get_chars (rx_buf);
	file.close();

	elapsed_time = stopwatch (STOP);					// capture the time
	Serial.print ("\r\nread ");
	Serial.print (rcvd_count);							// number of characters received
	Serial.print (" characters in ");
	Serial.print (elapsed_time);						// elapsed time
	Serial.println ("ms");

	settings.line_num = 0;								// reset to count lines taken from rx_buf
	Serial.println ("\r\nchecking");
	stopwatch (START);									// reset

	rx_ptr = rx_buf;									// initialize
	while (rx_ptr)
		{
		rx_ptr = array_get_line (ln_buf, rx_ptr);		// returns null pointer when no more characters in buffer
		if (!rx_ptr)
			break;
	
		if (EOF_MARKER == *ln_buf)						// when we find the end-of-file-marker
			{
			*out_ptr = *ln_buf;							// add it to out_buf
			break;										// done reading rx_buf
			}

		settings.line_num ++;							// tally

		if (('\r' == *ln_buf) || (EOL_MARKER == *ln_buf))	// do these here so we have source line numbers for error messages
			continue;									// cr or lf; we don't save newlines in fram
		if (strstr (ln_buf, "#"))
			continue;									// comment; we don't save comments in fram

		ret_val = normalize_kv_pair (ln_buf);			// trim, spaces to underscores; returns ptr to null terminated string

		if (ret_val)									// if an error or a heading (otherwise returns SUCCESS)
			{
			if (INI_ERROR == ret_val)					// not a heading, missing assignment operator
				settings.err_msg ((char *)"not key/value pair");
			else										// found a new heading
				{
				heading = ret_val;						// so remember which heading we found
				out_ptr = array_add_line (out_ptr, ln_buf);		// copy to out_buf; reset pointer to eof marker
				}
			continue;									// get the next line
			}

		if (SYSTEM == heading)							// validate the various settings according to their headings
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

	check_min_req_users ();								// there must be at minimum 1 each leader, service, & it tech user
	
	elapsed_time = stopwatch (STOP);					// capture the time
	Serial.print ("\r\nchecked ");
	Serial.print (settings.line_num);					// number of lines
	Serial.print (" lines in ");
	Serial.print (elapsed_time);						// elapsed time
	Serial.print ("ms; ");
	if (total_errs)										// if there have been any errors
		{
		Serial.print (total_errs);
		Serial.print (" error(s); ");
		Serial.print (warn_cnt);						// number of warnings
		Serial.println (" warning(s); ");
		Serial.println ("configuration not written.");
		}
	else
		{
		Serial.print ("0 error(s); ");					// no errors
		Serial.print (warn_cnt);						// number of warnings
		Serial.println (" warning(s).");
		}

	if (warn_cnt && !total_errs)						// warnings but no errors
		{
		while (Serial.available())
			ret_val = Serial.read();					// if anything in the Serial input, get rid of it
		if (!utils.get_user_yes_no ((char*)"loader", (char*)"ignore warnings and write settings to fram?", false))		// default answer no
			{
			total_errs = warn_cnt;						// spoof to prevent writing fram
			Serial.println("\n\nabandoned");
			}
		}

	if (!total_errs)									// if there have been errors, no writing fram
		{
		write_settings_to_out_buf (out_buf);			// write settings to the output buffer

		Serial.println ("\r\nerasing fram settings");
		stopwatch (START);								// reset
		utils.fram_fill (EOF_MARKER, FRAM_SETTINGS_START, (FRAM_SETTINGS_END - FRAM_SETTINGS_START + 1));
		elapsed_time = stopwatch (STOP);				// capture the time

		Serial.print ("\r\nerased ");
		Serial.print (FRAM_SETTINGS_END - FRAM_SETTINGS_START + 1);		// number of bytes
		Serial.print (" fram bytes in ");
		Serial.print (elapsed_time);					// elapsed time
		Serial.println ("ms");


		settings.line_num = 0;							// reset
		Serial.println ("\r\nwriting");

		stopwatch (START);

		fram.set_addr16 (FRAM_SETTINGS_START);			// set starting address where we will begin writing
		
		out_ptr = out_buf;								// point to the buffer
		while (out_ptr)
			{
			out_ptr = array_get_line (ln_buf, out_ptr);		// returns null pointer when no more characters in buffer
			if (!out_ptr)
				break;
			settings.line_num ++;							// tally
			fram.control.wr_buf_ptr = (uint8_t*)ln_buf;
			fram.control.rd_wr_len = line_len ((char*)ln_buf);
			fram.page_write();							// write it
			Serial.print (".");
			}
		fram.control.wr_byte = EOF_MARKER;				// write the EOF marker
		fram.byte_write();

		elapsed_time = stopwatch (STOP);				// capture the time

		Serial.print ("\r\nwrote ");
		Serial.print (settings.line_num);				// number of lines
		Serial.print (" lines to fram in ");
		Serial.print (elapsed_time);					// elapsed time
		Serial.println ("ms");

		stopwatch (START);
		crc = get_crc_array ((uint8_t*)out_buf);		// calculate the crc across the entire buffer

		if (settings.get_crc_fram (FRAM_SETTINGS_START) == crc)	// calculate the crc across the settings in fram
			{
			fram.set_addr16 (FRAM_CRC_LO);				// set address for low byte of crc
			fram.control.wr_int16 = crc;
			fram.int16_write();
			}
		else
			{
			while (Serial.available())
				ret_val = Serial.read();				// if anything in the Serial input, get rid of it
			if (utils.get_user_yes_no ((char*)"loader", (char*)"crc match failure.  dump settings from fram?", true))	// default answer yes
				utils.fram_hex_dump (0);
			Serial.println("\r\ncrc match failure. loader stopped; reset to restart");	// give up an enter and endless
			while(1);									// loop
			}
	
		elapsed_time = stopwatch (STOP);
		Serial.printf ("\r\ncrc value (0x%.4X) calculated and written to fram in %dms", crc, elapsed_time);
			
		Serial.print("\r\n\r\nfram settings write complete\r\n\r\n");

		while (Serial.available())
			ret_val = Serial.read();					// if anything in the Serial input, get rid of it
		if (utils.get_user_yes_no ((char*)"loader", (char*)"dump settings from fram?", true))	// default answer yes
			{
//			settings.dump_settings ();					// dump the settings to the monitor
			utils.fram_hex_dump (0);
			}
		
		Serial.println("\n\ndone");
		}

		while (Serial.available())
			ret_val = Serial.read();					// if anything in the Serial input, get rid of it
	if (utils.get_user_yes_no ((char*)"loader", (char*)"dump fram log memory?", true))	// default answer yes
		utils.fram_hex_dump (FRAM_LOG_START);
		
		while (Serial.available())
			ret_val = Serial.read();					// if anything in the Serial input, get rid of it
	if (utils.get_user_yes_no ((char*)"loader", (char*)"initialize fram log memory?", false))		// default answer no
		{
		Serial.println ("\r\ninitializing fram log memory");
		stopwatch (START);								// reset
		utils.fram_fill (EOF_MARKER, FRAM_LOG_START, (FRAM_LOG_END - FRAM_LOG_START + 1));
		elapsed_time = stopwatch (STOP);				// capture the time

		Serial.print ("\r\nerased ");
		Serial.print (FRAM_LOG_END - FRAM_LOG_START + 1);	// number of bytes
		Serial.print (" fram bytes in ");
		Serial.print (elapsed_time);					// elapsed time
		Serial.println ("ms");
		}

		while (Serial.available())
			ret_val = Serial.read();					// if anything in the Serial input, get rid of it
	if (!utils.get_user_yes_no ((char*)"loader", (char*)"load another file", false))	// default answer no
		{
		Serial.println("\r\nloader stopped; reset to restart");		// give up and enter an endless
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
