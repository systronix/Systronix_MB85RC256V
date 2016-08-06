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

//#define		BY_LINE			// characters line-by-line
//#define		HOMEBREW		// ignored if BY_LINE not defined

#ifndef		BY_LINE			// if not defined
#define		BY_CHAR			// then fetch character-by-character
#endif

#define		SYSTEM	0xDF	// TODO: move these to SALT.h?
#define		USERS	0xEF


//---------------------------< P A G E   S C O P E   V A R I A B L E S >--------------------------------------

char		rx_buf [8192];
char		out_buf [8192];
char		ln_buf [256];
uint16_t	err_cnt = 0;
uint16_t	total_errs = 0;
uint16_t	warn_cnt = 0;


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
			if (!stricmp (key_ptr, "[system]"))
				return SYSTEM;
			else if (!stricmp (key_ptr, "[habitat A]"))
				return HABITAT_A;
			else if (!stricmp (key_ptr, "[habitat B]"))
				return HABITAT_B;
			else if (!stricmp (key_ptr, "[habitat EC]"))
				return HABITAT_EC;
			else if (!stricmp (key_ptr, "[users]"))
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


//---------------------------< G E T _ S E R I A L _ L I N E >------------------------------------------------
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


//---------------------------< G E T _ S E R I A L _ C H A R S >----------------------------------------------
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
				
				Serial.print (".");
				continue;
				}
			else
				{
				char_cnt++;						// tally
				*rx_ptr++ = c;					// save and bump the pointer
				*rx_ptr = '\0';					// null terminate whether we need to or not
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

	
//---------------------------< S E T U P >--------------------------------------------------------------------

void setup()
	{
	uint16_t	rcvd_count;
	uint16_t	crc = 0xFFFF;
	uint32_t	millis_prev, millis_now;
	uint8_t		waiting_counter = 0;
	uint8_t		ret_val;
	uint8_t		heading = 0;

	char* rx_ptr = rx_buf;						// point to start of rx_buf
	char* out_ptr = out_buf;					// point to start of out_buf

	pinMode(PERIPH_RST, OUTPUT);
	digitalWrite(PERIPH_RST, LOW);				// resets asserted
	digitalWrite(PERIPH_RST, HIGH);				// resets released
	FETs.setup (I2C_FET);						// constructor for SALT_FETs, and PCA9557
	FETs.begin ();
	FETs.init ();								// lights, fans, and alarms all off

	Serial.begin(115200);						// usb; could be any value
	while((!Serial) && (millis()<10000));		// wait until serial monitor is open or timeout

#ifdef		BY_LINE
#ifdef		HOMEBREW	
	Serial.print ("8k (fetch by line - HB): initialization file loader: ");
#else
	Serial.print ("8k (fetch by line): initialization file loader: ");
#endif
#endif
#ifdef		BY_CHAR
	Serial.print ("8k (fetch by character): initialization file loader: ");
#endif
	Serial.print ("build time: ");				// assemble
	Serial.print (__TIME__);					// the
	Serial.print (" ");							// startup
	Serial.print (__DATE__);					// message
	
	fram.setup (0x50);
	fram.begin ();								// join i2c as master

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
	millis_prev = millis();
#ifdef		BY_CHAR
	rcvd_count = serial_get_chars (rx_buf);
#endif

#ifdef		BY_LINE
	while (1)
		{
#ifdef		HOMEBREW
		rcvd_count = serial_get_line (ln_buf);
#else
		rcvd_count = Serial.readBytesUntil ('\n', ln_buf, 256);		// 1 second timeout
#endif
		if (0 == rcvd_count)
			break;								// timed out
//		if (1 == rcvd_count)
//			continue;							// newline; we don't save newlines in fram
//		if (strstr (ln_buf, "#"))
//			continue;							// comment; we don't save comments in fram
		settings.line_num++;								// if here we got a line
												// TODO: trim leading and trailing white space?
		strcpy (rx_ptr, ln_buf);
		rx_ptr += strlen (ln_buf);				// advance the rx_ptr
		Serial.print (".");
		}
#endif

	millis_now = millis();
	Serial.print ("\r\nrecieved ");
#ifdef		BY_CHAR
	Serial.print (rcvd_count);
	Serial.print (" characters in ");
#endif
#ifdef		BY_LINE
	Serial.print (line_cnt);
	Serial.print (" lines in ");
#endif
	Serial.print (millis_now-millis_prev);
	Serial.println ("ms");

	settings.line_num = 0;
	Serial.println ("\r\nchecking");
	millis_prev = millis();

	rx_ptr = rx_buf;
	while (rx_ptr)
		{
		rx_ptr = array_get_line (ln_buf, rx_ptr);
		settings.line_num ++;

		if (('\r' == *ln_buf) || ('\n' == *ln_buf))	// do these here so we have source line numbers for error messages
			continue;								// cr or lf; we don't save newlines in fram
		if (strstr (ln_buf, "#"))
			continue;								// comment; we don't save comments in fram

		ret_val = normalize_kv_pair (ln_buf);

		if (ret_val)
			{
			if (INI_ERROR == ret_val)				// not a heading, missing assignment operator
				settings.err_msg ((char *)"not key/value pair");
			else								// found a new heading
				{
				heading = ret_val;				// so remember it
				strcpy (out_ptr, ln_buf);		// save it in the output buffer
				out_ptr += strlen (ln_buf);		// advance the pointer
				*out_ptr++ = '\n';				// add the end-of-line marker
				*out_ptr = '\0';				// and null terminate
				}
			continue;
			}

		if (SYSTEM == heading)					// validate the various settings according to their headings
			check_ini_system (ln_buf);
		else if (HABITAT_A == heading)
			check_ini_habitat_A (ln_buf);
		else if (HABITAT_B == heading)
			check_ini_habitat_B (ln_buf);
		else if (HABITAT_EC == heading)
			check_ini_habitat_EC (ln_buf);
		else if (USERS == heading)
			check_ini_users (ln_buf);
		
		strcpy (out_ptr, ln_buf);		// write the setting to the output buffer
		out_ptr += strlen(ln_buf);		// advance the pointer
		*out_ptr++ = '\n';				// add the end-of-line marker
		*out_ptr = '\0';				// and null terminate
		}

	millis_now = millis();
	Serial.print ("\r\nchecked ");
	Serial.print (settings.line_num);
	Serial.print (" lines in ");
	Serial.print (millis_now-millis_prev);
	Serial.print ("ms; ");
	if (total_errs)
		{
		Serial.print (total_errs);
		Serial.print (" error(s); ");
		Serial.print (warn_cnt);
		Serial.println (" warning(s); ");
		Serial.println ("configuration not written.");
		}
	else
		{
		Serial.print ("0 error(s); ");
		Serial.print (warn_cnt);
		Serial.println (" warning(s).");
		}

	if (warn_cnt && !total_errs)
		{
		if (!get_user_yes_no ((char*)"ignore warnings and write settings to fram?", false))		// default answer no
			total_errs = warn_cnt;					// spoof to prevent writing fram
		}

	if (!total_errs)
		{
		settings.line_num = 0;
		Serial.println ("\r\nwriting");
		millis_prev = millis();

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
	
		fram.set_addr16 (FRAM_SETTINGS_START);		// set starting address where we will begin writing
		
		out_ptr = out_buf;							// point to the buffer
		while (out_ptr)
			{
			out_ptr = array_get_line (ln_buf, out_ptr);			// get a line
			settings.line_num ++;										// tally
			fram.control.wr_buf_ptr = (uint8_t*)ln_buf;
			fram.control.rd_wr_len = strlen ((char*)ln_buf);
			fram.page_write();									// write it
			Serial.print (".");
			}
		
		fram.control.wr_byte = '\x04';				// EOF marker
		fram.byte_write();

		millis_now = millis();

		Serial.print ("\r\nwrote ");
		Serial.print (settings.line_num);
		Serial.print (" lines to fram in ");
		Serial.print (millis_now-millis_prev);
		Serial.println ("ms");

		Serial.print("\r\n\r\nfram write complete\r\n\r\n");

		if (get_user_yes_no ((char*)"dump settings from fram?", true))	// default answer yes
			settings.dump_settings ();							// dump the settings to the monitor
		}
	Serial.println("\n\ndone");
	}

void loop()
	{
	}
