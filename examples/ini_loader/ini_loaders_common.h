// this file holds functions that are common to both ini_loader.ino and ini_loader_SD.ino
//
// Alas, it doesn't seem that arduino allows it to be seen from another example folder.  So, there are two
// copies.  Changes made to one should be made to the other by copying the whole changed file on top of the
// file in the other folder.  Sigh.


//---------------------------< S T O P W A T C H >------------------------------------------------------------
//
// Start and stop an elapsed timer.
//
// returns start_time (current millis() value) in response to START;
// return elapsed_time in response to STOP
//

time_t stopwatch (boolean start_stop)
	{
	static uint32_t		state = STOP;
	static time_t		start_time = 0;
	static time_t		end_time = 0;
	static time_t		elapsed_time = TIMING;		// invalidate until started and stopped
	
	switch (state)
		{
		case STOP:
			if (start_stop)							// start command
				{
				start_time = millis();				// set start time
				elapsed_time = TIMING;				// invalidate until stopped
				state = TIMING;						// bump the state
				break;								// done
				}
			return TIMING;							// command to stop while stopped
		
		case TIMING:
			if (!start_stop)						// stop command
				{
				end_time = millis();				// set the end time
				elapsed_time = end_time - start_time;	// calculate
				state = STOP;						// bump the state
				return elapsed_time;				// done
				}
			start_time = millis();					// start command while timing is a restart
		}
	return start_time;						// return new start time
	}


//---------------------------< G E T _ C R C _ A R R A Y >----------------------------------------------------
//
// calculate a crc16 across a end-of-file marker terminated array
//

uint16_t get_crc_array (uint8_t* array_ptr)
	{
	uint16_t	crc = 0xFFFF;
	uint16_t	byte_count = 0;
	
	while (EOF_MARKER != *array_ptr)					// while not end-of-file marker
		{
		crc = settings.crc16_update (crc, *array_ptr);		// calc crc for this byte
		byte_count++;
		array_ptr++;								// bump the pointer
		}
	Serial.print ("byte count: ");
	Serial.println (byte_count);
	return crc;
	}


//---------------------------<  A R R A Y _ G E T _ L I N E >-------------------------------------------------
//
// copies settings line by line into a separate buffer for error checking, formatting before the setting is
// written to fram.
//

char* array_get_line (char* dest, char* array)
	{
	while ((EOF_MARKER != *array) && ('\r' != *array) && (EOL_MARKER != *array))
		*dest++ = *array++;						// copy array to destination until we find \n or \0

	if (('\r' == *array) || (EOL_MARKER == *array))	// if we stopped copying because \r or \n
		{
		*dest++ = *array++;						// add it to the destination
		*dest = '\0';							// null terminate for safety
		return array;							// else return a pointer to the start of the next line
		}
	else										// must be end-of-file marker
		{
		*dest = '\0';							// null terminate for safety
		return NULL;							// return a null pointer
		}
	}


//---------------------------<  A R R A Y _ A D D _ L I N E >-------------------------------------------------
//
// copies a line of text into a buffer and adds an end-of-file marker; returns a pointer to the eof marker.
// source is a null-terminated string without eof terminator.
//

char* array_add_line (char* array_ptr, char* source_ptr)
	{
	while (*source_ptr)
		*array_ptr++ = *source_ptr++;			// copy source into array

	*array_ptr++ = EOL_MARKER;						// add end-of-line marker
	*array_ptr = EOF_MARKER;						// add end-of-file marker
	return array_ptr;							// return a pointer to the end-of-line character
	}


//---------------------------< N O R M A L I Z E _ K V _ P A I R >--------------------------------------------
//
// normalize key value pairs by: removing excess whitespace; converting whitespace in key name to underscore;
// key name to lowercase.
//

uint8_t normalize_kv_pair (char* key_ptr)
	{
	char*	value_ptr;						// pointer to the key's value
	char*	i_ptr;							// an intermediate pointer used to reassemble the k/v pair

	value_ptr = strchr (key_ptr, '=');		// find the assignment operator; assign its address to value_ptr
	if (NULL == value_ptr)
		{
		key_ptr = settings.trim (key_ptr);
		if (settings.is_section_header (key_ptr))
			{
			if (strstr (key_ptr, "[system]"))
				return SYSTEM;
			else if (strstr (key_ptr, "[habitat A]"))
				return HABITAT_A;
			else if (strstr (key_ptr, "[habitat B]"))
				return HABITAT_B;
			else if (strstr (key_ptr, "[habitat EC]"))
				return HABITAT_EC;
			else if (strstr (key_ptr, "[users]"))
				return USERS;
			}
		return INI_ERROR;					// missing assignment operator or just junk text
		}
		
	*value_ptr++ = '\0';					// null terminate key at the assignment operator and point to the value portion of the original string
	key_ptr = settings.trim (key_ptr);		// trimming key_ptr returns a pointer to the first non-whitespace character of the 'key'
	settings.strip_white_space (key_ptr);	// remove all whitespace from key: 'lights on' becomes 'lights_on'
	settings.str_to_lower (key_ptr);		// make lowercase for future comparisons
	value_ptr = settings.trim (value_ptr);	// adjust value_ptr to point at the first non-whitespace character
	
	i_ptr = key_ptr + strlen (key_ptr);		// point to the key's null terminator
	*i_ptr++ = '=';							// insert assignment operator and bump to point at location for value
	if (i_ptr != value_ptr)
		strcpy (i_ptr, value_ptr);			// move value adjacent to assignment operator

	return SUCCESS;
	}


//---------------------------< W A R N _ M S G >--------------------------------------------------------------
//
//
//

void warn_msg (void)
	{
	Serial.print ("no value in line ");
	Serial.print (settings.line_num);
	Serial.println ("; system will use default value");
	warn_cnt++;								// tally number of warnings
	}


//---------------------------< I S _ U N I Q U E _ P I N >----------------------------------------------------
//
//
//

boolean is_unique_pin (uint8_t index, char* value_ptr)
	{
	uint8_t i;
	
	if ('\0' != pins[index][0])							// if we have already used this index, something wrong
		return false;
	strcpy (pins[index], value_ptr);					// copy the pin into the pins array
	
	for (i=1; i<=10; i++)								// loop through all comparing new pin to old pins
		{
		if (i == index)									// if index is same as loop counter; 
			continue;									// don't compare against self
			
		if (!strcmp (value_ptr, pins[i]))				// if same then new pin is not unique
			return false;
		}
	return true;										// so far, pin is unique
	}


//---------------------------< I S _ G O O D _ P I N >--------------------------------------------------------
//
//
//

boolean is_good_pin (const char* pin_ptr)
	{
	uint8_t i;
	char*	test_ptr;
	char	c;
	
	test_ptr = (char*)pin_ptr;
	if (5 != strlen (test_ptr))				// must have proper length
		return false;
	
	test_ptr = (char*)pin_ptr;
	for (i=0; i<5; i++)
		if (!isdigit (*test_ptr++))			// must be digits only
			return false;
	
	test_ptr = (char*)pin_ptr+1;
	for (i=0; i<4; i++)						// must not be a string of all same digits
		{
		if (*pin_ptr != *test_ptr)
			break;
		}
	if (4 == i)
		return false;
	
	test_ptr = (char*)pin_ptr;
	c = *test_ptr++;						// c gets first digit; bump pointer to second digit
	for (i=0; i<4; i++)
		{
		if ('9' == c)
			c = '0';
		else
			c++;
		if (c != *test_ptr++)				// is content of pointer one more than previous digit?
			break;
		}
	if (4 == i)
		return false;
		
	test_ptr = (char*)pin_ptr;
	c = *test_ptr++;						// c gets first digit; bump pointer to second digit
	for (i=0; i<4; i++)
		{
		if ('0' == c)
			c = '9';
		else
			c--;
		if (c != *test_ptr++)				// is content of pointer one less than previous digit?
			break;
		}
	if (4 == i)
		return false;
	return true;
	}


//---------------------------< C H E C K _ I N I _ S Y S T E M >----------------------------------------------
//
// reads the ini file from fram line-at-a-time looking for the [system] section header.  Once found, continues
// to read key/value pairs, converting the value as necessary and assigning it to the appropriate member of the
// sys_settings struct.  Processing ends when a new heading is found, or when the end of file marker is read.
//

void check_ini_system (char* key_ptr)
	{
	uint32_t	temp32;						// temp variable for holding uint32_t sized variables
	uint16_t	temp16;						// temp variable for holding uint16_t sized variables
	char		temp_array[32];

	char*	value_ptr;						// pointers to the key and value items

	value_ptr = strchr (key_ptr, '=');		// find the assignment operator; assign its address to value_ptr
	if (NULL == value_ptr)
		{
		settings.err_msg ((char *)"not key/value pair");		// should never get here
		total_errs++;						// make sure that we don't write to fram
		return;
		}
	
	*value_ptr++ = '\0';					// null terminate the key and bump the pointer to point at the value
	
	settings.err_cnt = 0;							// reset the counter
	
	if (16 < strlen (value_ptr))
		{
		settings.err_msg ((char *)"value string too long");
		return;
		}
	
	strcpy (temp_array, value_ptr);			// a copy to use for evaluation
	
	if (!strcmp (key_ptr, "config"))
		{
		if (stricmp (value_ptr, "SSWEC") && stricmp (value_ptr, "SS") &&
			stricmp (value_ptr, "B2BWEC") && stricmp (value_ptr, "B2B") &&
			stricmp (value_ptr, "SBS"))
				settings.err_msg ((char *)"unknown config");
		else
			strcpy (system_config, value_ptr);
		}
	else if (!strcmp (key_ptr, "dawn"))
		{
		if (*value_ptr)
			{
			temp32 = settings.string_to_time_stamp (temp_array);		// convert hh:mm to seconds
			
			if (TIME_ERROR == temp32)						// if can't be converted
				settings.err_msg ((char *)"invalid time");
			else
				strcpy (system_dawn, value_ptr);
			}
		else
			warn_msg ();
		}
	else if (!strcmp (key_ptr, "dusk"))
		{
		if (*value_ptr)
			{
			temp32 = settings.string_to_time_stamp (temp_array);		// convert hh:mm to seconds
			
			if (TIME_ERROR == temp32)						// if can't be converted
				settings.err_msg ((char *)"invalid time");
			else
				strcpy (system_dusk, value_ptr);
			}
		else
			warn_msg ();
		}
	else if (!strcmp (key_ptr, "fan_temp_1"))
		{
		if (*value_ptr)
			{
			temp16 = settings.fahrenheit_string_to_raw13 (temp_array);	// convert to binary raw13 format

			if (TEMP_ERROR == temp16)							// if can't be converted
				settings.err_msg ((char *)"invalid temperature");
			else
				strcpy (system_auto_fan_1, value_ptr);
			}
		else
			warn_msg ();
		}
	else if (!strcmp (key_ptr, "fan_temp_2"))
		{
		if (*value_ptr)
			{
			temp16 = settings.fahrenheit_string_to_raw13 (temp_array);	// convert to binary raw13 format
			
			if (TEMP_ERROR == temp16)							// if can't be converted
				settings.err_msg ((char *)"invalid temperature");
			else
				strcpy (system_auto_fan_2, value_ptr);
			}
		else
			warn_msg ();
		}
	else if (!strcmp (key_ptr, "fan_temp_3"))
		{
		if (*value_ptr)
			{
			temp16 = settings.fahrenheit_string_to_raw13 (temp_array);	// convert to binary raw13 format
			
			if (TEMP_ERROR == temp16)							// if can't be converted
				settings.err_msg ((char *)"invalid temperature");
			else
				strcpy (system_auto_fan_3, value_ptr);
			}
		else
			warn_msg ();
		}
	else if (!strcmp (key_ptr, "dhcp"))
		{
		if (*value_ptr)
			{
			if (stricmp (value_ptr, "yes") && stricmp (value_ptr, "no"))
				settings.err_msg ((char *)"invalid dhcp setting");
			else
				strcpy (system_dhcp, value_ptr);
			}
		else
			warn_msg ();
		}
	else if (!strcmp (key_ptr, "ip"))
		{
		if (*value_ptr)
			{
			if (!settings.is_dot_decimal (temp_array))
				settings.err_msg ((char *)"invalid ip");
			else
				strcpy (system_ip, value_ptr);
			}
		else
			warn_msg ();
		}
	else if (!strcmp (key_ptr, "mask"))
		{
		if (*value_ptr)
			{
			if (!settings.is_dot_decimal (temp_array))
				settings.err_msg ((char *)"invalid mask");
			else
				strcpy (system_mask, value_ptr);
			}
		else
			warn_msg ();
		}
	else if (!strcmp (key_ptr, "server_ip"))
		{
		if (*value_ptr)
			{
			if (!settings.is_dot_decimal (temp_array))
				settings.err_msg ((char *)"invalid server ip");
			else
				strcpy (system_server_ip, value_ptr);
			}
		else
			warn_msg ();
		}
	else if (!strcmp (key_ptr, "tz"))
		{
		if (*value_ptr)
			{
			if (stricmp (value_ptr, "pst") && stricmp (value_ptr, "mst") &&
				stricmp (value_ptr, "cst") && stricmp (value_ptr, "est") &&
				stricmp (value_ptr, "akst") && stricmp (value_ptr, "hast"))
					settings.err_msg ((char *)"unsupported time zone");
			else
				strcpy (system_tz, value_ptr);
			}
		else
			warn_msg ();
		}
	else if (!strcmp (key_ptr, "dst"))
		{
		if (*value_ptr)
			{
			if (stricmp (value_ptr, "no") && stricmp (value_ptr, "yes"))				// default value
				settings.err_msg ((char *)"invalid dst setting");
			else
				strcpy (system_dst, value_ptr);
			}
		else
			warn_msg ();
		}
	else
		settings.err_msg ((char *)"unrecognized setting");

	*(value_ptr-1) = '=';					// restore the '='
	total_errs += settings.err_cnt;					// accumulate for reporting later
	}


//---------------------------< C H E C K _ I N I _ H A B I T A T _ A >----------------------------------------
//
// reads the ini file from fram line-at-a-time looking for the [habitat A] section header.  Once found, continues
// to read key/value pairs, converting the value as necessary and assigning it to the appropriate member of the
// sys_settings struct.  Processing ends when a new heading is found, or when the end of file marker is read.
//

void check_ini_habitat_A (char* key_ptr)
	{
	uint8_t		index;						// used for habitat heat settings
	uint16_t	temp16;						// temp variable for holding uint16_t sized variables
	uint8_t		temp8;						// temp variable for holding uint8_t sized variables

	char*		value_ptr;						// pointers to the key and value items

	value_ptr = strchr (key_ptr, '=');		// find the assignment operator; assign its address to value_ptr
	if (NULL == value_ptr)
		{
		settings.err_msg ((char *)"(char *)not key/value pair");		// should never get here
		total_errs++;						// make sure that we don't write to fram
		return;
		}
	
	*value_ptr++ = '\0';					// null terminate the key and bump the pointer to point at the value

	settings.err_cnt = 0;							// reset the counter

	if (8 < strlen (value_ptr))
		{
		settings.err_msg ((char *)"value string too long");
		return;
		}
	
	if (strstr (key_ptr, "day_a_"))	// daytime temperature target A ...
		{
		if (*value_ptr)
			{
			index = settings.key_decode (key_ptr + strlen ("day_a_"));
			if (0xFF == index)
				settings.err_msg ((char *)"invalid drawer/compartment");
			else
				{
				temp16 = settings.fahrenheit_string_to_raw13 (value_ptr);	// convert to binary raw13 format

				if (TEMP_ERROR == temp16)							// if can't be converted
					settings.err_msg ((char *)"invalid temperature");
				else
					strcpy (habitat_A_heat_settings [index].day_temp, value_ptr);
				}
			}
		else
			warn_msg();
		}
	else if (strstr (key_ptr, "night_a_"))	// nighttime temperature target A ...
		{
		if (*value_ptr)
			{
			index = settings.key_decode (key_ptr + strlen ("night_a_"));
			if (0xFF == index)
				settings.err_msg ((char *)"invalid drawer/compartment");
			else
				{
				temp16 = settings.fahrenheit_string_to_raw13 (value_ptr);	// convert to binary raw13 format

				if (TEMP_ERROR == temp16)							// if can't be converted
					settings.err_msg ((char *)"invalid temperature");
				else
					strcpy (habitat_A_heat_settings [index].night_temp, value_ptr);
				}
			}
		else
			warn_msg();
		}
	else if (strstr (key_ptr, "hlamp_a_"))						// heat lamp A ...
		{
		if (*value_ptr)
			{
			index = settings.key_decode (key_ptr + strlen ("hlamp_a_"));
			if (0xFF == index)
				settings.err_msg ((char *)"invalid drawer/compartment");
			else
				{
				temp8 = (uint8_t)settings.str_to_int (value_ptr);
				if (INVALID_NUM == temp8 || ((25 != temp8) && (40 != temp8) && (50 != temp8)))
					settings.err_msg ((char *)"invalid wattage");
				else
					strcpy (habitat_A_heat_settings [index].watts, value_ptr);
				}
			}
		else
			warn_msg();
		}
//	else if (strstr (key_ptr, "ot_ignore_a_"))						// over-temp ignore A ...
//		{
//		if (*value_ptr)
//			{
//			index = settings.key_decode (key_ptr + strlen ("ot_ignore_a_"));
//			if (0xFF == index)
//				settings.err_msg ((char *)"invalid drawer/compartment");
//			else
//				{
//				if (stricmp (value_ptr, "yes") && stricmp (value_ptr, "no"))
//					settings.err_msg ((char *)"invalid over-temp ignore setting");
//				else
//					strcpy (overtemp_ignore_A [index], value_ptr);
//				}
//			}
//		else
//			warn_msg();
//		}
	else
		settings.err_msg ((char *)"unrecognized setting");

	*(value_ptr-1) = '=';					// restore the '='
	total_errs += settings.err_cnt;					// accumulate for reporting later
	}


//---------------------------< C H E C K _ I N I _ H A B I T A T _ B >----------------------------------------
//
// reads the ini file from fram line-at-a-time looking for the [habitat B] section header.  Once found, continues
// to read key/value pairs, converting the value as necessary and assigning it to the appropriate member of the
// sys_settings struct.  Processing ends when a new heading is found, or when the end of file marker is read.
//

void check_ini_habitat_B (char* key_ptr)
	{
	uint8_t		index;						// used for habitat heat settings
	uint16_t	temp16;						// temp variable for holding uint16_t sized variables
	uint8_t		temp8;						// temp variable for holding uint8_t sized variables

	char*		value_ptr;					// pointers to the key and value items

	value_ptr = strchr (key_ptr, '=');		// find the assignment operator; assign its address to value_ptr
	if (NULL == value_ptr)
		{
		settings.err_msg ((char *)"(char *)not key/value pair");		// should never get here
		total_errs++;						// make sure that we don't write to fram
		return;
		}
	
	*value_ptr++ = '\0';					// null terminate the key and bump the pointer to point at the value

	settings.err_cnt = 0;							// reset the counter

	if (8 < strlen (value_ptr))
		{
		settings.err_msg ((char *)"value string too long");
		return;
		}
	
	if (strstr (key_ptr, "day_b_"))	// daytime temperature target B ...
		{
		if (*value_ptr)
			{
			index = settings.key_decode (key_ptr + strlen ("day_b_"));
			if (0xFF == index)
				settings.err_msg ((char *)"invalid drawer/compartment");
			else
				{
				temp16 = settings.fahrenheit_string_to_raw13 (value_ptr);	// convert to binary raw13 format

				if (TEMP_ERROR == temp16)							// if can't be converted
					settings.err_msg ((char *)"invalid temperature");
				else
					strcpy (habitat_B_heat_settings [index].day_temp, value_ptr);
				}
			}
		else
			warn_msg();
		}
	else if (strstr (key_ptr, "night_b_"))	// nighttime temperature target B ...
		{
		if (*value_ptr)
			{
			index = settings.key_decode (key_ptr + strlen ("night_b_"));
			if (0xFF == index)
				settings.err_msg ((char *)"invalid drawer/compartment");
			else
				{
				temp16 = settings.fahrenheit_string_to_raw13 (value_ptr);	// convert to binary raw13 format

				if (TEMP_ERROR == temp16)							// if can't be converted
					settings.err_msg ((char *)"invalid temperature");
				else
					strcpy (habitat_B_heat_settings [index].night_temp, value_ptr);
				}
			}
		else
			warn_msg();
		}
	else if (strstr (key_ptr, "hlamp_b_"))	// heat lamp B ...
		{
		if (*value_ptr)
			{
			index = settings.key_decode (key_ptr + strlen ("hlamp_b_"));
			if (0xFF == index)
				settings.err_msg ((char *)"invalid drawer/compartment");
			else
				{
				temp8 = (uint8_t)settings.str_to_int (value_ptr);
				if (INVALID_NUM == temp8 || ((25 != temp8) && (40 != temp8) && (50 != temp8)))
					settings.err_msg ((char *)"invalid wattage");
				else
					strcpy (habitat_B_heat_settings [index].watts, value_ptr);
				}
			}
		else
			warn_msg();
		}
//	else if (strstr (key_ptr, "ot_ignore_b_"))		// over temp ignore A ...
//		{
//		if (*value_ptr)
//			{
//			index = settings.key_decode (key_ptr + strlen ("ot_ignore_b_"));
//			if (0xFF == index)
//				settings.err_msg ((char *)"invalid drawer/compartment");
//			else
//				{
//				if (stricmp (value_ptr, "yes") && stricmp (value_ptr, "no"))
//					settings.err_msg ((char *)"invalid over-temp ignore setting");
//				else
//					strcpy (overtemp_ignore_B [index], value_ptr);
//				}
//			}
//		else
//			warn_msg();
//		}
	else
		settings.err_msg ((char *)"unrecognized setting");

	*(value_ptr-1) = '=';					// restore the '='
	total_errs += settings.err_cnt;					// accumulate for reporting later
	}


//---------------------------< C H E C K _ I N I _ H A B I T A T _ E C >--------------------------------------
//
// reads the ini file from fram line-at-a-time looking for the [habitat EC] section header.  Once found, continues
// to read key/value pairs, converting the value as necessary and assigning it to the appropriate member of the
// sys_settings struct.  Processing ends when a new heading is found, or when the end of file marker is read.
//

void check_ini_habitat_EC (char* key_ptr)
	{
	uint16_t	temp16;						// temp variable for holding uint16_t sized variables
	uint8_t		temp8;						// temp variable for holding uint8_t sized variables

	char*		value_ptr;					// pointers to the key and value items

	value_ptr = strchr (key_ptr, '=');		// find the assignment operator; assign its address to value_ptr
	if (NULL == value_ptr)
		{
		settings.err_msg ((char *)"(char *)not key/value pair");		// should never get here
		total_errs++;						// make sure that we don't write to fram
		return;
		}
	
	*value_ptr++ = '\0';					// null terminate the key and bump the pointer to point at the value

	settings.err_cnt = 0;							// reset the counter

	if (8 < strlen (value_ptr))
		{
		settings.err_msg ((char *)"value string too long");
		return;
		}
	
	if (!strcmp (key_ptr, "day_ec_top"))	// daytime temperature target EC top ...
		{
		if (*value_ptr)
			{
			temp16 = settings.fahrenheit_string_to_raw13 (value_ptr);
			
			if (TEMP_ERROR == temp16)
				settings.err_msg ((char *)"invalid temperature");
			else
				strcpy (habitat_EC_heat_settings [1].day_temp, value_ptr);
			}
		else
			warn_msg();
		}
	else if (!strcmp (key_ptr, "night_ec_top"))	// nighttime temperature target EC top ...
		{
		if (*value_ptr)
			{
			temp16 = settings.fahrenheit_string_to_raw13 (value_ptr);
			
			if (TEMP_ERROR == temp16)
				settings.err_msg ((char *)"invalid temperature");
			else
				strcpy (habitat_EC_heat_settings [1].night_temp, value_ptr);
			}
		else
			warn_msg();
		}
	else if (!strcmp (key_ptr, "hlamp_ec_top"))	// heat lamp B ...
		{
		if (*value_ptr)
			{
			temp8 = (uint8_t)settings.str_to_int (value_ptr);
			if (INVALID_NUM == temp8 || ((25 != temp8) && (40 != temp8) && (50 != temp8)))
				settings.err_msg ((char *)"invalid wattage");
			else
				strcpy (habitat_EC_heat_settings [1].watts, value_ptr);
			}
		else
			warn_msg();
		}
//	else if (strstr (key_ptr, "ot_ignore_ec_top"))		// over temp ignore EC top ...
//		{
//		if (*value_ptr)
//			{
//			if (stricmp (value_ptr, "yes") && stricmp (value_ptr, "no"))
//				settings.err_msg ((char *)"invalid over-temp ignore setting");
//			else
//				strcpy (overtemp_ignore_EC [1], value_ptr);
//			}
//		else
//			warn_msg();
//		}
	else if (!strcmp (key_ptr, "day_ec_bot"))	// daytime temperature target EC bottom ...
		{
		if (*value_ptr)
			{
			temp16 = settings.fahrenheit_string_to_raw13 (value_ptr);
			
			if (TEMP_ERROR == temp16)
				settings.err_msg ((char *)"invalid temperature");
			else
				strcpy (habitat_EC_heat_settings [2].day_temp, value_ptr);
			}
		else
			warn_msg();
		}
	else if (!strcmp (key_ptr, "night_ec_bot"))	// nighttime temperature target EC bottom ...
		{
		if (*value_ptr)
			{
			temp16 = settings.fahrenheit_string_to_raw13 (value_ptr);
			
			if (TEMP_ERROR == temp16)
				settings.err_msg ((char *)"invalid temperature");
			else
				strcpy (habitat_EC_heat_settings [2].night_temp, value_ptr);
			}
		else
			warn_msg();
		}
	else if (!strcmp (key_ptr, "hlamp_ec_bot"))	// heat lamp B ...
		{
		if (*value_ptr)
			{
			temp8 = (uint8_t)settings.str_to_int (value_ptr);
			if (INVALID_NUM == temp8 || ((25 != temp8) && (40 != temp8) && (50 != temp8)))
				settings.err_msg ((char *)"invalid wattage");
			else
				strcpy (habitat_EC_heat_settings [2].watts, value_ptr);
			}
		else
			warn_msg();
		}
//	else if (strstr (key_ptr, "ot_ignore_ec_bot"))		// over temp ignore EC bot ...
//		{
//		if (*value_ptr)
//			{
//			if (stricmp (value_ptr, "yes") && stricmp (value_ptr, "no"))
//				settings.err_msg ((char *)"invalid over-temp ignore setting");
//			else
//				strcpy (overtemp_ignore_EC [2], value_ptr);
//			}
//		else
//			warn_msg();
//		}
	else
		settings.err_msg ((char *)"unrecognized setting");

	*(value_ptr-1) = '=';					// restore the '='
	total_errs += settings.err_cnt;					// accumulate for reporting later
	}


//---------------------------< C H E C K _ I N I _ U S E R S >------------------------------------------------
//
// reads the ini file from fram line-at-a-time looking for the [users] section header.  Once found, continues
// to read key/value pairs, converting the value as necessary and assigning it to the appropriate member of the
// sys_settings struct.  Processing ends when a new heading is found, or when the end of file marker is read.
//

void check_ini_users (char*	key_ptr)
	{
	uint8_t		index;						// index into user array
	char*		value_ptr;					// pointer to the value items

	value_ptr = strchr (key_ptr, '=');		// find the assignment operator; assign its address to value_ptr
	if (NULL == value_ptr)
		{
		settings.err_msg ((char *)"(char *)not key/value pair");		// should never get here
		total_errs++;						// make sure that we don't write to fram
		return;
		}
	
	*value_ptr++ = '\0';					// null terminate the key and bump the pointer to point at the value
	
	settings.err_cnt = 0;							// reset the counter
	
	if (16 < strlen (value_ptr))
		settings.err_msg ((char *)"value string too long");
	
	else if (strstr (key_ptr, "name_"))
		{
		if (*value_ptr)
			{
			index = (uint8_t)settings.str_to_int (&key_ptr[5]);
			if ((INVALID_NUM == index) || (USERS_MAX_NUM < index))
				settings.err_msg ((char *)"invalid name index");
			else
				strcpy (user [index].name, value_ptr);
			}
		}
	else if (strstr (key_ptr, "pin_"))
		{
		if (*value_ptr)
			{
			index = (uint8_t)settings.str_to_int (&key_ptr[4]);
			if ((INVALID_NUM == index) || (USERS_MAX_NUM < index))
				settings.err_msg ((char *)"invalid pin index");
			else
				{
				if (is_good_pin (value_ptr))
					{
					if (!is_unique_pin (index, value_ptr))
						settings.err_msg ((char *)"pin value not unique");
					else
						strcpy (user [index].pin, value_ptr);	// is valid and unique
					}
				else
					settings.err_msg ((char *)"invalid pin value");
				}
			}
		}
	else if (strcmp (key_ptr, "rights_"))
		{
		if (*value_ptr)
			{
			index = (uint8_t)settings.str_to_int (&key_ptr[7]);
			if ((INVALID_NUM == index) || (USERS_MAX_NUM < index))
				settings.err_msg ((char *)"invalid rights index");
			else if (*value_ptr)
				{	// TODO: should factory rights be part of the ini?
				if (stricmp(value_ptr, "factory") &&
					stricmp(value_ptr, "it tech") &&
					stricmp(value_ptr, "service") &&
					stricmp(value_ptr, "leader") &&
					stricmp(value_ptr, "associate"))
						settings.err_msg ((char *)"invalid rights value");
				else
					strcpy (user [index].rights, value_ptr);
				}
			}
		}
	else
		settings.err_msg ((char *)"unrecognized setting");

	*(value_ptr-1) = '=';					// restore the '='
	total_errs += settings.err_cnt;					// accumulate for reporting later
	}


//---------------------------< C H E C K _ M I N _ R E Q _ U S E R S >----------------------------------------
//
// there shall be no less than 1 leader user, 1 service user, and 1 it tech user defined in the .ini file
// except when the user list contains a factory user
//

void check_min_req_users (void)
	{
	uint8_t	i;
	boolean	leader = false;
	boolean	service = false;
	boolean it_tech = false;
	
	for (i=1; i <= USERS_MAX_NUM; i++)
		{
		if ('l' == user [i].rights[0])				// only need to look at first letter
			leader = true;
		if ('s' == user [i].rights[0])
			service = true;
		if ('i' == user [i].rights[0])
			it_tech = true;
		if ('f' == user [i].rights[0])				// in the factory doesn't matter if there are leader, service, it tech users
			{
			warn_cnt++;								// tally number of warnings
			Serial.printf ("\n[users] list contains user with factory rights: user: %d\n", i);	// warn because should not ship from factory with factory users in ini file
			return;
			}
		}
	if (!(leader && service && it_tech))
		{
		settings.err_msg ((char *)"one each of leader, service, and it tech users required");
		total_errs += settings.err_cnt;
		}
	}


//---------------------------< Z E R O _ P A D _ S E T T I N G >----------------------------------------------
//
// Place the end of line marker ('\n') at the end of the setting.  Return the number of bytes that the setting
// occupies in memory.  This function assumes that setting pointer points to a buffer with a properly formatted
// setting followed by '\0' bytes and that the buffer is sufficient in length to contain the setting k/v pair,
// the requisite number of '\0' bytes and the line terminator.  This function does not check formatting.
//
// Upon completion, the setting is
//		<key>=<value><null bytes><eol>
//	where <null bytes> is 0 or more bytes such that the number of <value> bytes + number of <null bytes> = limt
//  and where limit can be 8 or 16 (ip, server ip, mask, name_n, and rights_n keys)
//

uint8_t zero_pad_setting (char* setting_ptr)
	{
	char* 		value_ptr = strchr (setting_ptr, '=') + 1;		// point to the value in a k/v pair
	char*		padding_ptr;
	char*		key_ptr;
	uint8_t		value_len = strlen (value_ptr);					// get the length of the value pointer
	uint8_t		setting_len = strlen (setting_ptr);				// get the length of the whole setting
	uint8_t		limit = 8;						// for most settings
	
	key_ptr = strstr (setting_ptr, "ip");		// also finds 'server ip'
	if (key_ptr && (key_ptr < value_ptr))		// if found must be in the key, not the value
		limit = 16;								// for these keys, value length limit is 16 characters
	key_ptr = strstr (setting_ptr, "mask");
	if (key_ptr && (key_ptr < value_ptr))
		limit = 16;
	key_ptr = strstr (setting_ptr, "name");
	if (key_ptr && (key_ptr < value_ptr))
		limit = 16;
	key_ptr = strstr (setting_ptr, "rights");
	if (key_ptr && (key_ptr < value_ptr))
		limit = 16;

	padding_ptr = value_ptr + value_len + (limit - value_len);	// point to location of the line terminator
	*padding_ptr = EOL_MARKER;						// write the line terminator

	setting_len += ((limit - value_len) + 1);	// calculate the setting length

	return setting_len;							// return setting length
	}


//---------------------------< L I N E _ L E N >--------------------------------------------------------------
//
// like strlen() except that it is looking for '\n' or '\r' instead of '\0'.
//

size_t line_len (char* line_ptr)
	{
	size_t length = 0;
	
	while ((EOL_MARKER != *line_ptr) && ('\r' != *line_ptr) && (EOF_MARKER != *line_ptr))	// not carriage return or line feed or EOF marker
		{
		length ++;			// bump the length
		line_ptr ++;		// bump the pointer
		}
	length ++;				// include end-of-line marker in the length
	return length;			// return the length
	}


//---------------------------< A D D _ L I N E >--------------------------------------------------------------
//
// 
//

char* add_line (kv_pair pair, char* array_ptr)
	{
	uint8_t i;
	uint8_t	ret_val;
	char*	ln_ptr;
	char*	key_ptr = (char*)pair.key;
	char*	val_ptr = pair.value;
	
	memset (ln_buf, '\0', 128);				// set the buffer to all '\0'

	ln_ptr = ln_buf;						// point to the line buffer
	while (*key_ptr)						// copy the key into the line buffer
		*ln_ptr++ = *key_ptr++;
	*ln_ptr++ = '=';						// add the assignment operator
	while (*val_ptr)						// copy the value (if there is one) into the line buffer
		*ln_ptr++ = *val_ptr++;
	*ln_ptr = '\0';							// null terminate

	ret_val = zero_pad_setting (ln_buf);	// zero pad; ln_buf is no longer null terminated
	
	ln_ptr = ln_buf;						// reset the pointer
	for (i=0; i<ret_val; i++)
		*array_ptr++ = *ln_ptr++;			// copy the setting to out_buf
	return array_ptr;						// return a pointer to next available space in out_buf
	}


//---------------------------< W R I T E _ S E T T I N G S _ T O _ O U T _ B U F >----------------------------
//
// writes a complete settings file using defaults where explicit values are not provided in the .ini file.
// This allows the .ini file to be 'incomplete' (not recommended)
//

void write_settings_to_out_buf (char* out_buf_ptr)
	{
	uint8_t i;
	char* ln_ptr;
	
//---------- [system]
	strcpy (ln_buf, (char *)"[system]\n");
	ln_ptr = ln_buf;							// reset the pointer
	while (*ln_ptr)
		*out_buf_ptr++ = *ln_ptr++;				// write the heading to out_buf
	
	for (i=1; i<=11; i++)
		out_buf_ptr = add_line (kv_system [i], out_buf_ptr);

//---------- [habitat A]
	strcpy (ln_buf, (char *)"[habitat A]\n");
	ln_ptr = ln_buf;							// reset the pointer
	while (*ln_ptr)
		*out_buf_ptr++ = *ln_ptr++;				// write the heading to out_buf
	
	for (i=1; i<=36; i++)
		out_buf_ptr = add_line (kv_habitat_A [i], out_buf_ptr);

//---------- [habitat B]
	strcpy (ln_buf, (char *)"[habitat B]\n");
	ln_ptr = ln_buf;							// reset the pointer
	while (*ln_ptr)
		*out_buf_ptr++ = *ln_ptr++;				// write the heading to out_buf
	
	for (i=1; i<=36; i++)
		out_buf_ptr = add_line (kv_habitat_B [i], out_buf_ptr);

//---------- [habitat EC]
	strcpy (ln_buf, (char *)"[habitat EC]\n");
	ln_ptr = ln_buf;							// reset the pointer
	while (*ln_ptr)
		*out_buf_ptr++ = *ln_ptr++;				// write the heading to out_buf
	
	for (i=1; i<=6; i++)
		out_buf_ptr = add_line (kv_habitat_EC [i], out_buf_ptr);

//---------- [users]
	strcpy (ln_buf, (char *)"[users]\n");
	ln_ptr = ln_buf;							// reset the pointer
	while (*ln_ptr)
		*out_buf_ptr++ = *ln_ptr++;				// write the heading to out_buf
	
	for (i=1; i<=(USERS_MAX_NUM*3); i++)
		out_buf_ptr = add_line (kv_users [i], out_buf_ptr);
	}

