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

#include "ini_loader.h"

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
				else
					strcpy (overtemp_ignore_A [index], value_ptr);
				}
			}
		else
			warn_msg();
		}
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
				else
					strcpy (overtemp_ignore_B [index], value_ptr);
				}
			}
		else
			warn_msg();
		}
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
	else if (strstr (key_ptr, "ot_ignore_ec_top"))		// over temp ignore EC top ...
		{
		if (*value_ptr)
			{
			if (stricmp (value_ptr, "yes") && stricmp (value_ptr, "no"))
				settings.err_msg ((char *)"invalid over-temp ignore setting");
			else
				strcpy (overtemp_ignore_EC [1], value_ptr);
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
	else if (strstr (key_ptr, "ot_ignore_ec_bot"))		// over temp ignore EC bot ...
		{
		if (*value_ptr)
			{
			if (stricmp (value_ptr, "yes") && stricmp (value_ptr, "no"))
				settings.err_msg ((char *)"invalid over-temp ignore setting");
			else
				strcpy (overtemp_ignore_EC [2], value_ptr);
			}
		else
			warn_msg();
		}
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
			if ((INVALID_NUM == index) || (10 < index))
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
			if ((INVALID_NUM == index) || (10 < index))
				settings.err_msg ((char *)"invalid pin index");
			else
				{
				if (is_good_pin (value_ptr))
					strcpy (user [index].pin, value_ptr);
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
			if ((INVALID_NUM == index) || (10 < index))
				settings.err_msg ((char *)"invalid rights index");
			else if (*value_ptr)
				{	// TODO: should manuf, dev, and service rights be part of the ini?
				if (stricmp(value_ptr, "factory") &&
					stricmp(value_ptr, "service") &&
					stricmp(value_ptr, "manager") &&
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


//---------------------------< H E X _ D U M P _ A R R A Y >--------------------------------------------------
//
// print a hex dump of an internal array in 256 byte chunks.  If the array is smaller than 256 byte, this
// function will print whatever is in memory beyond the last element of the array.
//
// TODO: add an array length argument so that the function will only print pages that cover the array.
//

//void hex_dump_array (uint8_t* array_ptr, size_t len)
//	{
//	uint16_t	index = 0;									// array index
	
//	while (1)
//		{
//		Serial.print ("@ ");
//		Serial.print ((uint32_t)array_ptr);
//		Serial.print (" (0x");
//		Serial.print ((uint32_t)array_ptr, HEX);
//		Serial.print (")");
//		hex_dump_core (index, array_ptr);

//		Serial.println ("");									// insert a blank line
//		array_ptr += 256;										// next 'page' starting address
//		index += 256;											// and index

//		if (len > index)
//			{
//			if (!utils.get_user_yes_no ((char*)"loader", (char*)"another page?", true))	// default answer yes
//				return;
//			}
//		else
//			{
//			if (!utils.get_user_yes_no ((char*)"loader", (char*)"end of array; more?", false))	// default answer no
//				return;
//			}
//		}
//	}


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
	
	for (i=1; i<=48; i++)
		out_buf_ptr = add_line (kv_habitat_A [i], out_buf_ptr);

//---------- [habitat B]
	strcpy (ln_buf, (char *)"[habitat B]\n");
	ln_ptr = ln_buf;							// reset the pointer
	while (*ln_ptr)
		*out_buf_ptr++ = *ln_ptr++;				// write the heading to out_buf
	
	for (i=1; i<=48; i++)
		out_buf_ptr = add_line (kv_habitat_B [i], out_buf_ptr);

//---------- [habitat EC]
	strcpy (ln_buf, (char *)"[habitat EC]\n");
	ln_ptr = ln_buf;							// reset the pointer
	while (*ln_ptr)
		*out_buf_ptr++ = *ln_ptr++;				// write the heading to out_buf
	
	for (i=1; i<=8; i++)
		out_buf_ptr = add_line (kv_habitat_EC [i], out_buf_ptr);

//---------- [users]
	strcpy (ln_buf, (char *)"[users]\n");
	ln_ptr = ln_buf;							// reset the pointer
	while (*ln_ptr)
		*out_buf_ptr++ = *ln_ptr++;				// write the heading to out_buf
	
	for (i=1; i<=30; i++)
		out_buf_ptr = add_line (kv_users [i], out_buf_ptr);
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
		Serial.print ("\r\ncrc value (");
		utils.hex_print (crc);
		Serial.print (") calculated and written to fram in ");
		Serial.print (elapsed_time);					// elapsed time
		Serial.println ("ms");
			
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
	total_errs = 0;										// reset these so they aren't misleading
	warn_cnt = 0;
	}
