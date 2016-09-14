

#include <Systronix_MB85RC256V.h>
#include <SALT.h>
#include <SALT_settings.h>

Systronix_MB85RC256V fram;

SALT_settings settings;				// settings class in SALT_settings

//---------------------------< T R I M >----------------------------------------------------------------------
//
// code stolen from stackoverflow.com
//

char* trim (char* str)
	{
	size_t len = 0;
	char *frontp = str;
	char *endp = NULL;

	if (NULL == str)
		return NULL;			// pointer is null so return NULL pointer
	if ('\0' == str[0])			// string is empty so return the empty string
		return str;

	len = strlen ((char*)str);	// get the string's length
	endp = str + len;			// use it to make a pointer to the end of the string

	/* Move the front and back pointers to address the first non-whitespace
	* characters from each end.
	*/
	while (isspace (*frontp) && *frontp)	// swallow leading white space by moving the front pointer
		++frontp;
		
	if (endp != frontp)						// swallow trailing white space by moving the end pointer
		while( isspace(*(--endp)) && endp != frontp );

	if (str + len - 1 != endp)				// what is this for?
		*(endp + 1) = '\0';
	else if( frontp != str &&  endp == frontp )
		*str = '\0';

	return frontp;
	/* Shift the string so that it starts at str so that if it's dynamically
	* allocated, we can still free it on the returned pointer.  Note the reuse
	* of endp to mean the front of the string buffer now.
	*/
//    endp = str;
//    if( frontp != str )
//    {
//            while( *frontp ) { *endp++ = *frontp++; }
//            *endp = '\0';
//    }


//    return str;
	}





//---------------------------< P A R S E _ I N I >------------------------------------------------------------

void parse_ini (void)
	{
	uint16_t ret_val;
	char		read_buf[256];
	char*	value;
	char*	key;
	fram.set_addr16 (0);			// reset the address to zero
	uint16_t line_num=0;

	do
		{
		ret_val = fram_get_line ((uint8_t*)read_buf);
		line_num++;						// tally
		if ('#' == read_buf[0])
			continue;					// don't care about comments; get next line
		if ('\4' == read_buf[0])
			return;						// end of file marker; quit

		value = strchr (read_buf, '=');		// find the assignment operator
		if (NULL == value)
			{
			if (!stricmp (read_buf, "[system]"))
				{
				Serial.print (line_num);
				Serial.print (": ");
				Serial.println ("system group");
				}
			continue;					// no assignment operator; get next line
			}
			
		*value++ = '\0';				// null terminate the key and point to the value
		key = trim (read_buf);
		key = trim (key);
		value = trim (value);

		if (!stricmp (key, "config"))
			{
			Serial.print (line_num);
			Serial.print (": ");
			Serial.print ("config: ");
			if (!stricmp (value, "SS"))
				settings.sys_settings.config = SS;
			else if (!stricmp (value, "SSWEC"))
				settings.sys_settings.config = SSWEC;
			else if (!stricmp (value, "B2B"))
				settings.sys_settings.config = B2B;
			else if (!stricmp (value, "B2BWEC"))
				settings.sys_settings.config = B2BWEC;
			else if (!stricmp (value, "SBS"))
				settings.sys_settings.config = SBS;
			else
				{
				Serial.print ("unknown config in line ");
				Serial.println (line_num);
				}

			Serial.println (settings.sys_settings.config);
			}
		else if (!stricmp (key, "lights on"))
			{
			Serial.print (line_num);
			Serial.print (": ");
			Serial.println ("lights on");
			settings.sys_settings.time_lights_on = (time_t)atoi(value);
			
			if (0 == settings.sys_settings.time_lights_on)
				{
				Serial.print ("invalid time in line ");
				Serial.println (line_num);
				}
			
			Serial.println (settings.sys_settings.time_lights_on);
			}
		else if (!stricmp (key, "lights off"))
			{
			Serial.print (line_num);
			Serial.print (": ");
			Serial.println ("lights off");
			settings.sys_settings.time_dusk = (time_t)atoi(value);
			
			if (0 ==settings.sys_settings.time_dusk)
				{
				Serial.print ("invalid time in line ");
				Serial.println (line_num);
				}
			
			Serial.println (settings.sys_settings.time_dusk);
			}
		else if (!stricmp (key, "fan temp 1"))
			{
			Serial.print (line_num);
			Serial.print (": ");
			Serial.println ("fan temp 1");
			settings.sys_settings.temperature_auto_fan_1 = (uint16_t)atoi(value);
			
			if (0 ==settings.sys_settings.temperature_auto_fan_1)
				{
				Serial.print ("invalid temperature in line ");
				Serial.println (line_num);
				}
			
			Serial.println (settings.sys_settings.temperature_auto_fan_1);
			}
		else if (!stricmp (key, "fan temp 2"))
			{
			Serial.print (line_num);
			Serial.print (": ");
			Serial.println ("fan temp 2");
			settings.sys_settings.temperature_auto_fan_2 = (uint16_t)atoi(value);
			
			if (0 == settings.sys_settings.temperature_auto_fan_2)
				{
				Serial.print ("invalid temperature in line ");
				Serial.println (line_num);
				}
			
			Serial.println (settings.sys_settings.temperature_auto_fan_2);
			}
		else if (!stricmp (key, "fan temp 3"))
			{
			Serial.print (line_num);
			Serial.print (": ");
			Serial.println ("fan temp 3");
			settings.sys_settings.temperature_auto_fan_3 = (uint16_t)atoi(value);
			
			if (0 == settings.sys_settings.temperature_auto_fan_3)
				{
				Serial.print ("invalid temperature in line ");
				Serial.println (line_num);
				}
			
			Serial.println (settings.sys_settings.temperature_auto_fan_3);
			}
		else if (!stricmp (key, "ip"))
			{
			Serial.print (line_num);
			Serial.print (": ");
			Serial.println ("ip");
			}
		else if (!stricmp (key, "mask"))
			{
			Serial.print (line_num);
			Serial.print (": ");
			Serial.println ("mask");
			}
		else if (!stricmp (key, "server ip"))
			{
			Serial.print (line_num);
			Serial.print (": ");
			Serial.println ("server ip");
			}
		else if (!stricmp (key, "tz"))
			{
			Serial.print (line_num);
			Serial.print (": ");
			Serial.println ("tz");
			}
		else if (!stricmp (key, "dst"))
			{
			Serial.print (line_num);
			Serial.print (": ");
			Serial.println ("dst");
			}
		
//		Serial.print ("key: ");
//		Serial.print ((char*)key);
//		Serial.print (" value: ");
//		Serial.println ((char*)value);
		
//		if (String("[system]").equals(String((char*)read_buf)))
//			Serial.println ("found system");
//		if (ret_val)
//			Serial.println ((char*)read_buf);
		} while (ret_val);

	}







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

//---------------------------< S E T U P >--------------------------------------------------------------------

void setup()
	{
	uint8_t i;
	
	Serial.begin(115200);     // use max baud rate
	while((!Serial) && (millis()<10000));    // wait until serial monitor is open or timeout
	Serial.println("fram test");
	
	fram.setup (0x50);

	fram.begin ();		// join i2c as master
	
	uint16_t address = 0x0005;
	uint8_t		read_buf[256];
//	uint8_t		write_buf[16] = {15, 14, 13 ,12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0xAA};
	uint8_t		write_buf[][128] =
					{
					"# lines beginning with '#' are comments\n",
					"# all values in this setup file are to be decimal; ultimately in standard units where appropriate\n",
					"# one element per line; beginning at the left margin; no blank lines\n",
					"#\n",
					"[system]\n",
					"# config shall be one of: SS, SSWEC, B2B, B2BWEC, SBS\n",
					"config = SSWEC\n",
					"#lights on/off times ultimately in hh:mm (24hr clock); don't really need seconds here\n",
					"lights on = 25200\n",
					"lights off = 68400\n",
					"#fan and other temperatures to be given in degrees F\n",
					"fan temp 1 = 3413\n",
					"fan temp 2 = 3768\n",
					"fan temp 3 = 4124\n",
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


/*//----< W R I T E   I N I   F I L E >-----
	fram.set_addr16 (0);			// set starting address where we will begin writing
	for (i=0; i<30; i++)
		{
		if ('\4' == write_buf[i][0])		// if end of file marker
			break;
		fram.control.wr_buf_ptr = (uint8_t*)write_buf[i];
//		Serial.print((char*)write_buf[i]);
		fram.control.rd_wr_len = strlen ((char*)write_buf[i]);
//		Serial.println (fram.control.rd_wr_len);
		fram.page_write();
		}

	fram.control.wr_byte = '\4';	// EOF marker
	fram.byte_write();
*/
	parse_ini();
/*	fram.set_addr16 (0);			// reset the address

	fram.control.addr.as_struct.high = 0;		// set upper byte to 0	//
	fram.control.addr.as_struct.low = 0;		// set lower byte to i	//
	Serial.print ("fram current address read from address 0x");
	Serial.print (fram.control.addr.as_struct.low, HEX);
	Serial.print (": ");
	fram.byte_read ();
	Serial.println (char(fram.control.rd_byte));

	do 
		{
		Serial.print ("fram current address read from address 0x");
		Serial.print (fram.control.addr.as_struct.low, HEX);
		Serial.print (": ");
		
		fram.current_address_read ();
		Serial.println (char(fram.control.rd_byte));
		} while (fram.control.rd_byte != '\4');			// while not equal to end of file marker


	uint16_t ret_val;
	fram.set_addr16 (0);			// reset the address
	do
		{
		ret_val = fram_get_line (read_buf);
		if (String("[system]").equals(String((char*)read_buf)))
			Serial.println ("found system");
		if (ret_val)
			Serial.println ((char*)read_buf);
		} while (ret_val);
	Serial.print ("end");
*/
/*
	fram.control.rd_buf_ptr = read_buf;
	fram.control.rd_wr_len = 200;
	fram.page_read();
	
	for (i=0; i<64; i++)
		{
		Serial.print ("fram page read from address 0x");
		Serial.print (i, HEX);
		Serial.print (": ");
		Serial.println (char(read_buf[i]));
		}
		

	Serial.print ("u16: 0x");
	Serial.println (fram.control.addr.as_u16, HEX);
	
	fram.device_id (&fram.manufID, &fram.prodID);
	Serial.print ("Manufacturer ID: 0x");
	Serial.println (fram.manufID, HEX);
	Serial.print ("Product ID: 0x");
	Serial.println (fram.prodID, HEX);
*/	}

void loop()
{

}