//
// this code writes a SALT ini file to address zero ... in fram
// When using realterm it may be handy to create a shortcut with a variant of this setup string:
//		baud=115200 tab=send display=1 rows=60 fs=8 cols=80 port=10
// baud required to be as specified above the others may be set to your personal preferences
// this version does not require line delay
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

SALT_FETs FETs;
Systronix_MB85RC256V fram;
SALT_settings settings;

//---------------------------< P A G E   S C O P E   V A R I A B L E S >--------------------------------------

char		rx_buf [8192];
char		ln_buf [256];

//---------------------------< C R C 1 6 _ U P D A T E >------------------------------------------------------
//
// Used to calculte the settings crc.  Seed with 0xFFFF.  When checking a data with a crc, Seed with 0xFFFF,
// separate the current crc into high and low bytes.  Calculate a new crc across the data then include crc low
// byte followed by crc high byte.  The result should be zero.
//

uint16_t crc16_update (uint16_t crc, uint8_t data)
	{
	unsigned int i;

	crc ^= data;
	for (i = 0; i < 8; ++i)
		{
		if (crc & 1)
			crc = (crc >> 1) ^ 0xA001;
		else
			crc = (crc >> 1);
		}
	return crc;
	}


//---------------------------<  A R R A Y _ G E T _L I N E >--------------------------------------------------
//
// copies settings line by line into a separate buffer for error checking, formatting before the setting is
// written to fram.
//

char* array_get_line (char* dest, char* array)
	{
	while (*array && (('\r' != *array) && ('\n' != *array)))
		*dest++ = *array++;						// copy array to destination until we find \n or \0

	if (('\r' == *array) || ('\n' == *array))	// if we stopped copying because \r or \n
		*dest++ = *array++;						// add it to the destination
	*dest = '\0';								// null terminate for safety

	if ('\0' == *array)							// if we stopped copying because \0
		return NULL;							// return a null pointer
	return array;								// else return a pointer to the start of the next line
	}


//---------------------------< S E T U P >--------------------------------------------------------------------

void setup()
	{
	uint16_t	rcvd_count;
	uint16_t	crc = 0xFFFF;
	uint32_t	millis_prev, millis_now;
	uint8_t		waiting_counter = 0;
	uint16_t	line_cnt = 0;
	
	pinMode(PERIPH_RST, OUTPUT);
	digitalWrite(PERIPH_RST, LOW);				// resets asserted
	digitalWrite(PERIPH_RST, HIGH);				// resets released
	FETs.setup (I2C_FET);						// constructor for SALT_FETs, and PCA9557
	FETs.begin ();
	FETs.init ();								// lights, fans, and alarms all off

	Serial.begin(115200);						// usb; could be any value
	while((!Serial) && (millis()<10000));		// wait until serial monitor is open or timeout
	Serial.print ("8k write config to fram: ");
	Serial.print ("build time: ");				// assemble
	Serial.print (__TIME__);					// the
	Serial.print (" ");							// startup
	Serial.print (__DATE__);					// message

	
	fram.setup (0x50);
	fram.begin ();								// join i2c as master
	fram.set_addr16 (FRAM_SETTINGS_START);		// set starting address where we will begin writing

	char* rx_ptr = rx_buf;						// point to start of rx_buf
	
	Serial.println ("\r\nready");
	millis_prev = millis();						// init
	while (!Serial.available())
		{
		if (10000 < (millis() - millis_prev))
			{
			millis_prev = millis();				// reset
			Serial.print ("waiting for file (");
			Serial.print (waiting_counter++);
			Serial.println (") ...");
			}
		}

	Serial.println ("receiving");
	millis_prev = millis();
	while (1)
		{
		rcvd_count = Serial.readBytesUntil ('\n', ln_buf, 256);		// 1 second timeout
		if (0 == rcvd_count)
			break;								// timed out
		
		line_cnt++;								// if here we got a line
		
		if (1 == rcvd_count)
			continue;							// newline; we don't save newlines in fram
		if (strstr (ln_buf, "#"))
			continue;							// comment; we don't save comments in fram
												// TODO: trim leading and trailing white space?
//--------new
		strcpy (rx_ptr, ln_buf);
		rx_ptr += strlen (ln_buf);				// advance the rx_ptr
//--------/new		
//-------old
//		Serial.println (rcvd_count);

//		fram.control.wr_buf_ptr = (uint8_t*)ln_buf;
//		fram.control.rd_wr_len = strlen ((char*)ln_buf);
//		fram.page_write();
		Serial.print (".");
//-------/old
		}
	millis_now = millis();
	Serial.print ("\r\nrecieved ");
	Serial.print (line_cnt);
	Serial.print (" lines in ");
	Serial.print (millis_now-millis_prev);
	Serial.println ("ms");
	
	Serial.println ("\r\nwriting");
	millis_prev = millis();

	line_cnt = 0;
	rx_ptr = rx_buf;
	while (rx_ptr)
		{
		rx_ptr = array_get_line (ln_buf, rx_ptr);
		line_cnt++;

		fram.control.wr_buf_ptr = (uint8_t*)ln_buf;
		fram.control.rd_wr_len = strlen ((char*)ln_buf);
		fram.page_write();
		Serial.print (".");
		}

/*-----------------------------
	// calculate the crc across the entire buffer
	rx_ptr = rx_buf;							// reset the pointer
	while (*rx_ptr)								// while not null
		{
		crc = crc16_update (crc, *rx_ptr);		// calc crc for this byte
		rx_ptr++;								// bump the pointer
		}
		
	Serial.print ("\ncrc: ");					// should be the crc value
	Serial.println (crc, HEX);					
	// check the crc
	rx_ptr = rx_buf;							// reset the pointer
	uint8_t crc_h = (uint8_t)(crc>>8);			// the previously calculated crc split into bytes
	uint8_t crc_l = (uint8_t)crc;
	crc = 0xFFFF;								// reset the seed value
	while (*rx_ptr)
		{
		crc = crc16_update (crc, *rx_ptr);
		rx_ptr++;
		}
	
	crc = crc16_update (crc, crc_l);
	crc = crc16_update (crc, crc_h);

	Serial.print ("crc: ");						// should be 0
	Serial.println (crc, HEX);					
----------------------------*/	
	
	fram.control.wr_byte = '\4';				// EOF marker
	fram.byte_write();
	millis_now = millis();

	Serial.print ("\r\nwrote ");
	Serial.print (line_cnt);
	Serial.print (" lines to fram in ");
	Serial.print (millis_now-millis_prev);
	Serial.println ("ms");

	Serial.print("\r\n\r\nfram write complete\r\n\r\n");
	settings.dump_settings ();							// dump the settings to the monitor
	Serial.println("\n\ndone");
	}

void loop()
	{
	}
