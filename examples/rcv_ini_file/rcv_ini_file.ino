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

Systronix_MB85RC256V fram;

//---------------------------< F R A M _ G E T _ L I N E >----------------------------------------------------
//
// call this after setting the appropriate values in the fram control struct.  This function reads characters
// until it encounters any control character (0x00 to 0x1F; 0x7F +), or gets to the end
// of the fram memory address space (address rolls over to zero)
//

uint16_t fram_get_line (uint8_t* buf)
	{
	uint16_t count = 0;
	
	fram.byte_read ();						// get the first byte
	if ((' ' > fram.control.rd_byte) || (0x7F <= fram.control.rd_byte) || (0 == fram.control.addr.as_u16))
		{
		*buf = '\0';
		return 0;
		}
	else
		{
		*buf++ = fram.control.rd_byte;		// save the byte and 
		count ++;							// bump the counter
		}
	
	do
		{
		fram.current_address_read ();		// get next byte
		if ((' ' > fram.control.rd_byte) || (0x7F <= fram.control.rd_byte) || (0 == fram.control.addr.as_u16))
			{								// not printable or we wrapped the address pointer back to zero
			*buf = '\0';					// null terminate the receiving buffer
			return count;					// and return number of bytes we read
			}

		*buf++ = fram.control.rd_byte;		// save the byte we read
		count++;							// tally it
		} while (1);

	}


//---------------------------< D U M P _ S E T T I N G S >----------------------------------------------------
//
// Dumps the system settings 'imi' file from fram to the serial monitor.
//

void dump_settings (void)
	{
	uint16_t	ret_val;
	char		read_buf[256];
	
	fram.set_addr16 (0);			// reset the address
	do
		{
		ret_val = fram_get_line ((uint8_t*)read_buf);
		if (ret_val)
			Serial.println (read_buf);
		} while (ret_val);
	}

	char		rx_buf [1024];
	char		ln_buf [256];

//---------------------------< S E T U P >--------------------------------------------------------------------

void setup()
	{
	uint8_t		i;
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
		
//		Serial.println (rcvd_count);
		fram.control.wr_buf_ptr = (uint8_t*)ln_buf;
		fram.control.rd_wr_len = strlen ((char*)ln_buf);
		fram.page_write();
		Serial.print (".");
		}

	fram.control.wr_byte = '\4';				// EOF marker
	fram.byte_write();

	Serial.println("\r\n\r\nfram write complete\r\n\r\n");

	dump_settings ();							// dump the settings to the monitor
	Serial.println("\r\n\r\ndone");
	}

void loop()
{

}