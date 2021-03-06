//
// this code writes a SALT ini file to address zero ... in fram
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


//---------------------------< S E T U P >--------------------------------------------------------------------

void setup()
	{
	uint8_t i;
	
	Serial.begin(115200);     // use max baud rate
	while((!Serial) && (millis()<10000));    // wait until serial monitor is open or timeout
	Serial.println("write config to fram:");
	
	fram.setup (0x50);
	fram.begin ();		// join i2c as master
	
	uint8_t		write_buf[][128] =		// the configuration to write
					{
					"# lines beginning with '#' are comments\n",
					"# all values in this setup file are to be decimal; ultimately in standard units where appropriate\n",
					"# one element per line; beginning at the left margin; no blank lines\n",
					"#\n",
					"[system]\n",
					"# config shall be one of: SS, SSWEC, B2B, B2BWEC, SBS\n",
					"config = SSWEC\n",
					"#lights on/off times ultimately in hh:mm (24hr clock); don't really need seconds here\n",
					"lights on = 07:00\n",		// 25200 seconds = 07:00
					"lights off = 19:00\n",		// 68400 seconds = 19:00
					"#fan and other temperatures to be given in degrees F\n",
					"fan temp 1 = 80.0\n",		// 3413 = 0x0D55 = 80F = 26.6640C
					"fan temp 2 = 85.0\n",		// 3768 = 0x0EB8 = 85F = 29.4375C
					"fan temp 3 = 90.0\n",		// 4124 = 0x101C = 90F = 32.21875C
					"# ethernet settings\n",
					"ip = 10.2.1.100\n"
					"mask = 255.255.255.0\n",
					"server ip = 10.2.1.1\n",
					"#\n",
					"# time and date\n",
					"tz = pst\n",
					"dst = yes\n",
					'\4'			// end of file marker; must be the last element in this array
					};


//-----< W R I T E   I N I   F I L E >-----
	fram.set_addr16 (0);			// set starting address where we will begin writing
	for (i=0; i<30; i++)			// i large enough to include all members of the write_buf array
		{
		if ('\4' == write_buf[i][0])		// if end of file marker
			break;
		fram.control.wr_buf_ptr = (uint8_t*)write_buf[i];
		fram.control.rd_wr_len = strlen ((char*)write_buf[i]);
		fram.page_write();
		}

	fram.control.wr_byte = '\4';	// EOF marker
	fram.byte_write();

	dump_settings ();				// dump the settings to the monitor
	Serial.println("\n\ndone");

	}

void loop()
{

}