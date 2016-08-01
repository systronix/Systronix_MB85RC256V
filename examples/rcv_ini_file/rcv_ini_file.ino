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

Systronix_MB85RC256V fram;
SALT_settings settings;

//---------------------------< P A G E   S C O P E   V A R I A B L E S >--------------------------------------

char		rx_buf [1024];
char		ln_buf [256];

//---------------------------< S E T U P >--------------------------------------------------------------------

void setup()
	{
	uint16_t	rcvd_count;
	
	Serial.begin(5);						// usb; could be any value
	while((!Serial) && (millis()<10000));		// wait until serial monitor is open or timeout
	Serial.println("write config to fram:");
	
	fram.setup (0x50);
	fram.begin ();								// join i2c as master
	fram.set_addr16 (0);						// set starting address where we will begin writing

	Serial.println ("ready");
	while (!Serial.available());
	Serial.print ("receiving ");
	while (1)
		{
		rcvd_count = Serial.readBytesUntil ('\n', ln_buf, 256);		// 1 second timeout
		if (0 == rcvd_count)
			break;								// timed out
		if (1 == rcvd_count)
			continue;							// newline; we don't save newlines in fram
		if (strstr (ln_buf, "#"))
			continue;							// comment; we don't save comments in fram
//		Serial.println (rcvd_count);
		fram.control.wr_buf_ptr = (uint8_t*)ln_buf;
		fram.control.rd_wr_len = strlen ((char*)ln_buf);
		fram.page_write();
		Serial.print (".");
		}

	fram.control.wr_byte = '\4';				// EOF marker
	fram.byte_write();

	Serial.println("\r\n\r\nfram write complete\r\n\r\n");

	settings.dump_settings ();							// dump the settings to the monitor
	Serial.println("\r\n\r\ndone");
	}

void loop()
	{
	}
