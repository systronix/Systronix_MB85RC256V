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

SALT_FETs FETs;
Systronix_MB85RC256V fram;
SALT_settings settings;


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
uint16_t	err_cnt = 0;
uint16_t	total_errs = 0;
uint16_t	warn_cnt = 0;


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


//---------------------------< H E X _ P R I N T _ C O R E >--------------------------------------------------
//
//
//

void hex_print_core (uint32_t val_32, uint8_t len)
	{
	switch (len)
		{
		case 4:								// uint32_t
			for (uint32_t i=0x10000000; i>0x1000; i>>=4)		// add leading zeros as necessary
				{
				if (i > val_32)
					Serial.print ("0");							// then fall into uint16_t
				}
		case 2:								// uint16_t
			for (uint32_t i=0x1000; i>0x10; i>>=4)
				{
				if (i > val_32)
					Serial.print ("0");							// and so downwards
				}
		case 1:								// uint8_t
			if (0x10 > val_32)
				Serial.print ("0");
			break;
		default:
			Serial.print ("hex_print_core() unknown len");
			return;
		}
	Serial.print (val_32, HEX);
	}


//---------------------------< H E X _ P R I N T   ( 1   B Y T E ) >------------------------------------------

void hex_print (uint8_t val)
	{
	hex_print_core ((uint32_t)val, 1);
	}


//---------------------------< H E X _ P R I N T   ( 2   B Y T E ) >------------------------------------------

void hex_print (uint16_t val)
	{
	hex_print_core ((uint32_t)val, 2);
	}


//---------------------------< H E X _ P R I N T   ( 4   B Y T E ) >------------------------------------------

void hex_print (uint32_t val)
	{
	hex_print_core (val, 4);
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
	while ((EOF_MARKER != *array) && ('\r' != *array) && ('\n' != *array))
		*dest++ = *array++;						// copy array to destination until we find \n or \0

	if (('\r' == *array) || ('\n' == *array))	// if we stopped copying because \r or \n
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

	*array_ptr++ = '\n';						// add end-of-line marker
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
		settings.err_msg ((char *)"(char *)not key/value pair");		// should never get here
		total_errs++;						// make sure that we don't write to fram
		return;
		}
	
	*value_ptr++ = '\0';					// null terminate the key and bump the pointer to point at the value
	
	err_cnt = 0;							// reset the counter
	
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
		}
	else if (!strcmp (key_ptr, "lights_on"))
		{
		if (*value_ptr)
			{
			temp32 = settings.string_to_time_stamp (temp_array);		// convert hh:mm to seconds
			
			if (TIME_ERROR == temp32)						// if can't be converted
				settings.err_msg ((char *)"invalid time");
			}
		else
			warn_msg ();
		}
	else if (!strcmp (key_ptr, "lights_off"))
		{
		temp32 = settings.string_to_time_stamp (temp_array);		// convert hh:mm to seconds
		
		if (TIME_ERROR == temp32)						// if can't be converted
			settings.err_msg ((char *)"invalid time");
		}
	else if (!strcmp (key_ptr, "fan_temp_1"))
		{
		temp16 = settings.fahrenheit_string_to_raw13 (temp_array);	// convert to binary raw13 format

		if (TEMP_ERROR == temp16)							// if can't be converted
			settings.err_msg ((char *)"invalid temperature");
		}
	else if (!strcmp (key_ptr, "fan_temp_2"))
		{
		temp16 = settings.fahrenheit_string_to_raw13 (temp_array);	// convert to binary raw13 format
		
		if (TEMP_ERROR == temp16)							// if can't be converted
			settings.err_msg ((char *)"invalid temperature");
		}
	else if (!strcmp (key_ptr, "fan_temp_3"))
		{
		temp16 = settings.fahrenheit_string_to_raw13 (temp_array);	// convert to binary raw13 format
		
		if (TEMP_ERROR == temp16)							// if can't be converted
			settings.err_msg ((char *)"invalid temperature");
		}
	else if (!strcmp (key_ptr, "ip"))
		{
		if (!settings.is_dot_decimal (temp_array))
			settings.err_msg ((char *)"invalid ip");
		}
	else if (!strcmp (key_ptr, "mask"))
		{
		if (!settings.is_dot_decimal (temp_array))
			settings.err_msg ((char *)"invalid mask");
		}
	else if (!strcmp (key_ptr, "server_ip"))
		{
		if (!settings.is_dot_decimal (temp_array))
			settings.err_msg ((char *)"invalid server ip");
		}
	else if (!strcmp (key_ptr, "tz"))
		{
		if (stricmp (value_ptr, "pst") && stricmp (value_ptr, "mst") &&
			stricmp (value_ptr, "cst") && stricmp (value_ptr, "est") &&
			stricmp (value_ptr, "akst") && stricmp (value_ptr, "hast"))
				settings.err_msg ((char *)"unsupported time zone");
		}
	else if (!strcmp (key_ptr, "dst"))
		{
		if (stricmp (value_ptr, "no") && stricmp (value_ptr, "yes"))				// default value
			settings.err_msg ((char *)"invalid dst setting");
		}
	else
		settings.err_msg ((char *)"unrecognized setting");

	*(value_ptr-1) = '=';					// restore the '='
	total_errs += err_cnt;					// accumulate for reporting later
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

	err_cnt = 0;							// reset the counter

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
				}
			}
		else
			warn_msg();
		}
	else if (strstr (key_ptr, "ot_ignore_a_"))						// over-temp ignore A ...
		{
		if (*value_ptr)
			{
			index = settings.key_decode (key_ptr + strlen ("ot_ignore_a_"));
			if (0xFF == index)
				settings.err_msg ((char *)"invalid drawer/compartment");
			else
				{
				if (stricmp (value_ptr, "yes") && stricmp (value_ptr, "no"))
					settings.err_msg ((char *)"invalid over-temp ignore setting");
				}
			}
		else
			warn_msg();
		}
	else
		settings.err_msg ((char *)"unrecognized setting");

	*(value_ptr-1) = '=';					// restore the '='
	total_errs += err_cnt;					// accumulate for reporting later
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

	err_cnt = 0;							// reset the counter

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
				}
			}
		else
			warn_msg();
		}
	else if (strstr (key_ptr, "ot_ignore_b_"))		// over temp ignore A ...
		{
		if (*value_ptr)
			{
			index = settings.key_decode (key_ptr + strlen ("ot_ignore_b_"));
			if (0xFF == index)
				settings.err_msg ((char *)"invalid drawer/compartment");
			else
				{
				if (stricmp (value_ptr, "yes") && stricmp (value_ptr, "no"))
					settings.err_msg ((char *)"invalid over-temp ignore setting");
				}
			}
		else
			warn_msg();
		}
	else
		settings.err_msg ((char *)"unrecognized setting");

	*(value_ptr-1) = '=';					// restore the '='
	total_errs += err_cnt;					// accumulate for reporting later
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

	err_cnt = 0;							// reset the counter

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
			}
		else
			warn_msg();
		}
	else if (strstr (key_ptr, "ot_ignore_ec_top"))		// over temp ignore EC top ...
		{
		if (*value_ptr)
			{
			if (stricmp (value_ptr, "yes") && stricmp (value_ptr, "no"))
				settings.err_msg ((char *)"invalid over-temp ignore setting");
			}
		else
			warn_msg();
		}
	else if (!strcmp (key_ptr, "day_ec_bot"))	// daytime temperature target EC bottom ...
		{
		if (*value_ptr)
			{
			temp16 = settings.fahrenheit_string_to_raw13 (value_ptr);
			
			if (TEMP_ERROR == temp16)
				settings.err_msg ((char *)"invalid temperature");
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
			}
		else
			warn_msg();
		}
	else if (strstr (key_ptr, "ot_ignore_ec_bot"))		// over temp ignore EC bot ...
		{
		if (*value_ptr)
			{
			if (stricmp (value_ptr, "yes") && stricmp (value_ptr, "no"))
				settings.err_msg ((char *)"invalid over-temp ignore setting");
			}
		else
			warn_msg();
		}
	else
		settings.err_msg ((char *)"unrecognized setting");

	*(value_ptr-1) = '=';					// restore the '='
	total_errs += err_cnt;					// accumulate for reporting later
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
	int32_t		pin;						// pin converted from text

	char*	value_ptr;						// pointers to the key and value items

	value_ptr = strchr (key_ptr, '=');		// find the assignment operator; assign its address to value_ptr
	if (NULL == value_ptr)
		{
		settings.err_msg ((char *)"(char *)not key/value pair");		// should never get here
		total_errs++;						// make sure that we don't write to fram
		return;
		}
	
	*value_ptr++ = '\0';					// null terminate the key and bump the pointer to point at the value
	
	err_cnt = 0;							// reset the counter
	
	if (16 < strlen (value_ptr))
		{
		settings.err_msg ((char *)"value string too long");
		return;
		}
	
	if (strstr (key_ptr, "name_"))
		{
		index = (uint8_t)settings.str_to_int (&key_ptr[5]);
		if ((INVALID_NUM == index) || (10 < index))
			settings.err_msg ((char *)"invalid name index");
		else if (!*value_ptr)
			warn_msg ();
		}
	else if (strstr (key_ptr, "pin_"))
		{
		index = (uint8_t)settings.str_to_int (&key_ptr[4]);
		if ((INVALID_NUM == index) || (10 < index))
			settings.err_msg ((char *)"invalid pin index");
		else if (*value_ptr)
			{
			pin = settings.str_to_int (value_ptr);	// convert to a number to see if string is all digits
			if ((INVALID_NUM == pin) || (5 != strlen (value_ptr)))
				settings.err_msg ((char *)"invalid pin value");
			}
		else
			warn_msg ();
		}
	else if (strcmp (key_ptr, "rights_"))
		{
		index = (uint8_t)settings.str_to_int (&key_ptr[7]);
		if ((INVALID_NUM == index) || (10 < index))
			settings.err_msg ((char *)"invalid rights index");
		else if (*value_ptr)
			{	// TODO: should manuf, dev, and service rights be part of the ini?
			if (stricmp(value_ptr, "manufacturer") &&
				stricmp(value_ptr, "developer") &&
				stricmp(value_ptr, "service") &&
				stricmp(value_ptr, "manager") &&
				stricmp(value_ptr, "employee"))
					settings.err_msg ((char *)"invalid rights value");
			}
		else
			warn_msg ();
		}
	else
		settings.err_msg ((char *)"unrecognized setting");

	*(value_ptr-1) = '=';					// restore the '='
	total_errs += err_cnt;					// accumulate for reporting later
	}


//---------------------------< S E R I A L _ G E T _ L I N E >------------------------------------------------
//
// 1360ms
//

uint16_t serial_get_line (char* ln_ptr)
	{
	uint16_t	char_cnt = 0;
	time_t		last_char_time = millis();	// initialize to current time so we don't immediately fall out
	
	while (1)
		{
		while (Serial.available())				// while stuff to get
			{
			*ln_ptr = Serial.read();			// read and save the byte
			if ('\n' == *ln_ptr)
				{
				*ln_ptr ='\0';					// was a newline so null terminate
				return char_cnt;
				}

			char_cnt++;							// tally
			ln_ptr++;							// bump the next character
			if (255 <= char_cnt)
				{
				*ln_ptr = '\0';					// null terminate at ln_buf[255]
				return char_cnt;
				}
			last_char_time = millis();			// reset the timer
			}
		
		while (!Serial.available())
			{
			if (1000 < (millis()-last_char_time))
				return char_cnt;
			}
		}
	}


//---------------------------< S E R I A L _ G E T _ C H A R S >----------------------------------------------
//
// 1260mS
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
			if ('\n' == c)
				{
				char_cnt++;						// tally
				*rx_ptr++ = c;					// save and bump the pointer
				*rx_ptr = EOF_MARKER;				// add end-of-file whether we need to or not
				*(rx_ptr+1) = '\0';				// null terminate whether we need to or not
				
				Serial.print (".");
				continue;
				}
			else if ('\r' == c)
				{
				char_cnt ++;					// tally
				continue;						// but don't save; we only want the newline as a line marker
				}
			else
				{
				char_cnt++;						// tally
				*rx_ptr++ = c;					// save and bump the pointer
				*rx_ptr = EOF_MARKER;				// add end-of-file whether we need to or not
				*(rx_ptr+1) = '\0';				// null terminate whether we need to or not
				}

			if (8191 <= char_cnt)
				{
				*rx_ptr = '\0';					// null terminate at rx_buf[8191]
				return char_cnt;
				}
			}
		
		while (!Serial.available())
			{
			if (1000 < (millis()-last_char_time))
				return char_cnt;
			}
		}
	}

//---------------------------< Z E R O _ P A D _ S E T T I N G >----------------------------------------------
//
// Pad the setting value with '\0' characters.  Return the number of byte that the setting occupies in memory.
//

uint8_t zero_pad_setting (char* setting_ptr)
	{
	char* 		value_ptr = strchr (setting_ptr, '=') + 1;
	char*		padding_ptr;
	uint8_t		value_len = strlen (value_ptr);
	uint8_t		setting_len = strlen (setting_ptr);
	uint8_t		limit = 8;						// for most settings
	
	if (strstr (setting_ptr, "ip") ||			// also finds 'server ip'
		strstr (setting_ptr, "mask") ||
		strstr (setting_ptr, "name") ||
		strstr (setting_ptr, "rights"))
			limit = 16;							// for these keys, value length limit is 16 characters
	
	padding_ptr = value_ptr + value_len;		// point to the line terminator (not the null byte)
	for (uint8_t i=value_len; i<limit; i++)
		{
		*padding_ptr++ = '\0';					// write a fill byte and bump the pointer
		setting_len++;							// keep track of the setting's length
		}

	*padding_ptr++ = '\n';						// write the line terminator
	setting_len++;								// bump the length
	
	return setting_len;
	}


//---------------------------< L I N E _ L E N >--------------------------------------------------------------
//
// like strlen() except that it is looking for '\n' or '\r' instead of '\0'.
//

size_t line_len (char* line_ptr)
	{
	size_t length = 0;
	
	while (('\n' != *line_ptr) && ('\r' != *line_ptr) && (EOF_MARKER != *line_ptr))	// not carriage return or line feed or EOF marker
		{
		length ++;			// bump the length
		line_ptr ++;		// bump the pointer
		}
	length ++;				// include end-of-line marker in the length
	return length;			// return the length
	}


//---------------------------< G E T _ U S E R _ Y E S _ N O >------------------------------------------------
//
//
//

boolean get_user_yes_no (char* query, boolean yesno)
	{
	char c;
	Serial.print ("\r\nloader> ");
	Serial.print (query);
	if (yesno)
		Serial.print (" ([y]/n) ");
	else
		Serial.print (" (y/[n]) ");
	
	while (1)
		{
		if (Serial.available())
			{
			c = Serial.read();
			if ((('\x0d' == c) && !yesno) || ('n' == c) || ('N' == c))
				{
				Serial.println ("no");
				return false;
				}
			else if ((('\x0d' == c) && yesno) || ('y' == c) || ('Y' == c))
				{
				Serial.println ("yes");
				return true;
				}
			}
		}
	}


//---------------------------< F R A M _ G E T _ N _ B Y T E S >----------------------------------------------
//
// call this after setting the appropriate values in the fram control struct.  This function reads n number
// of characters into a buffer.
//

void fram_get_n_bytes (uint8_t* buf_ptr, size_t n)
	{
	uint8_t i;
	
	fram.byte_read ();						// get the first byte
	*buf_ptr++ = fram.control.rd_byte;		// save the byte we read
	for (i=0; i<(n-1); i++)					// loop getting the rest of the bytes
		{
		fram.current_address_read ();		// get next byte
		*buf_ptr++ = fram.control.rd_byte;	// save the byte we read
		};
	}


//---------------------------< F R A M _ F I L L >------------------------------------------------------------
//
// fill fram with n copies of 'c' beginning at address and continuing for n number of bytes.
// writes in groups of 256.  For n not an evenly divisible by 256, this code will stop short.
//

void fram_fill (uint8_t c, uint16_t address, size_t n)
	{
	uint8_t	buf[256];							// max length natively supported by the i2c_t3 library
	size_t	i;									// the iterator

	memset (buf, c, 256);						// fill buffer with characters write to fram

	for (i=0; i<n; i+=256)						// page_write() max length is 256 bytes (i2c_t3 limit) so iterate
		{
		fram.set_addr16 (address + i);			// set the address
		fram.control.wr_buf_ptr = buf;			// point to ln_buf
		fram.control.rd_wr_len = 256;			// number of bytes to write
		fram.page_write();						// do it
		}
	}


//---------------------------< H E X _ D U M P _ C O R E >----------------------------------------------------
//
// Render a 'page' (16 rows of 16 columns) of hex data and its character equivalent.  data_ptr MUST point to
// a buffer of at least 256 bytes.  This function always renders 256 items regardless of the size of the buffer.
//

void hex_dump_core (uint16_t address, uint8_t* data_ptr)
	{
	uint8_t*	text_ptr = data_ptr;					// walks in tandem with data_ptr; prints the text
	
	for (uint8_t i=0; i<16; i++)						// dump 'pages' that are 16 lines of 16 bytes
		{
		Serial.print ("\r\n\r\n");						// open some space
		hex_print (address);
		Serial.print ("    ");							// space between address and data

		
		for (uint8_t j=0; j<8; j++)						// first half
			{
			hex_print (*data_ptr++);
			Serial.print (" ");							// space between bytes
			}
		Serial.print (" -  ");							// mid array separator

		for (uint8_t j=0; j<8; j++)						// second half
			{
			hex_print (*data_ptr++);
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

		address+=16;
		}
	}


//---------------------------< H E X _ D U M P _ S E T T I N G S >--------------------------------------------
//
// TODO: make a generic version of this
//

void hex_dump_settings (uint16_t start_address)
	{
	uint8_t		buf [256];
	
	while (1)
		{
		fram.set_addr16 (start_address);						// first address to dump
		fram_get_n_bytes ((uint8_t*)buf, 256);					// get 256 bytes from fram
		hex_dump_core (start_address, buf);

		Serial.println ("");									// insert a blank line
		if (!get_user_yes_no ((char*)"another page?", true))	// default answer yes
			return;
		start_address += 256;									// next 'page' starting address
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
			if (!get_user_yes_no ((char*)"another page?", true))	// default answer yes
				return;
			}
		else
			{
			if (!get_user_yes_no ((char*)"end of array; more?", false))	// default answer no
				return;
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

	Serial.begin(115200);						// usb; could be any value
	while((!Serial) && (millis()<10000));		// wait until serial monitor is open or timeout

	Serial.print ("NAP ini loader: ");
	Serial.print ("build time: ");				// assemble
	Serial.print (__TIME__);					// the
	Serial.print (" ");							// startup
	Serial.print (__DATE__);					// message
	
	fram.setup (0x50);
	fram.begin ();								// join i2c as master
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
	char*		ln_ptr;							// pointer to ln_buf

	memset (out_buf, EOF_MARKER, 8192);			// fill out_buf with end-of-file markers

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

		if (('\r' == *ln_buf) || ('\n' == *ln_buf))	// do these here so we have source line numbers for error messages
			continue;								// cr or lf; we don't save newlines in fram
		if (strstr (ln_buf, "#"))
			continue;								// comment; we don't save comments in fram

		ret_val = normalize_kv_pair (ln_buf);		// trim, spaces to underscores; returns ptr to mull terminated string

		if (ret_val)								// if an error or a heading (otherwise returns SUCCESS)
			{
			if (INI_ERROR == ret_val)				// not a heading, missing assignment operator
				settings.err_msg ((char *)"+not key/value pair");
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
		
		ret_val = zero_pad_setting (ln_buf);		// do the zero padding; returned string is not and cannot be null terminated

		ln_ptr = ln_buf;							// reset the pointer
		for (uint8_t i=0; i<ret_val; i++)
			{
			*out_ptr++ = *ln_ptr++;					// copy the setting to out_buf
			}
		*out_ptr = EOF_MARKER;							// add the end-of-file marker
		}

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
		if (!get_user_yes_no ((char*)"ignore warnings and write settings to fram?", false))		// default answer no
			{
			total_errs = warn_cnt;					// spoof to prevent writing fram
			Serial.println("\n\nabandoned");
			}
		}

	if (!total_errs)								// if there have been errors, no writing fram
		{
		Serial.println ("\r\nerasing fram settings");
		stopwatch (START);						// reset
		fram_fill (EOF_MARKER, FRAM_SETTINGS_START, 8192);
		elapsed_time = stopwatch (STOP);			// capture the time

		Serial.print ("\r\nerased ");
		Serial.print (8192);						// number of bytes
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

		fram.set_addr16 (0);						// set fram control block starting address
		fram.byte_read();
		if (0x00 != fram.control.rd_byte)			// if address zero not blank, erase the control block
			{
			memset (ln_buf, '\0', 16);
			fram.set_addr16 (0);					// set fram control block starting address
			fram.control.wr_buf_ptr = (uint8_t*)ln_buf;
			fram.control.rd_wr_len = 16;
			fram.page_write();						// erase the control block
			}

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
			fram.control.wr_byte = (uint8_t)crc;		// write the low byte
			fram.byte_write();
			fram.set_addr16 (FRAM_CRC_HI);				// set address for high byte of crc
			fram.control.wr_byte = (uint8_t)(crc>>8);	// write the high byte
			fram.byte_write();
			}
		else
			{
			Serial.println("\r\ncrc match failure. loader stopped; reset to restart");	// give up and enter and endless
			while(1);									// loop
			}
	
		elapsed_time = stopwatch (STOP);
		Serial.print ("\r\ncrc value (");
		hex_print (crc);
		Serial.print (") calculated and written to fram in ");
		Serial.print (elapsed_time);				// elapsed time
		Serial.println ("ms");
			
		Serial.print("\r\n\r\nfram write complete\r\n\r\n");

		if (get_user_yes_no ((char*)"dump settings from fram?", true))	// default answer yes
			{
//			settings.dump_settings ();				// dump the settings to the monitor
			hex_dump_settings (0);
			}
		
		Serial.println("\n\ndone");
		}
	
	if (!get_user_yes_no ((char*)"load another file", false))	// default answer no
		{
		Serial.println("\r\nloader stopped; reset to restart");				// give up and enter and endless
		while(1);									// loop
		}
	memset (rx_buf, '\0', 8192);					// clear rx_buf to zeros
	total_errs = 0;									// reset these so they aren't misleading
	warn_cnt = 0;
	}
