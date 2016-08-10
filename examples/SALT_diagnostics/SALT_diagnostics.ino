#include <Systronix_MB85RC256V.h>
//#include <SALT_settings.h>
#include <SALT_FETs.h>
#include <SALT.h>

SALT_FETs FETs;
Systronix_MB85RC256V fram;
//SALT_settings settings;


//---------------------------< D E F I N E S >----------------------------------------------------------------

#define		FRAM_SIZE		0x8000		// 0x0000-0x7FFF
#define		END_OF_FILE		0xFFFF

#define		TIMING			0xFFFFFFFF	// elapsed time holds this value while timing
#define		START			1
#define		STOP			0


//---------------------------< P A G E   S C O P E   V A R I A B L E S >--------------------------------------

time_t		start_time = 0;
time_t		end_time = 0;
time_t		elapsed_time = 0;

uint8_t		test_buf [512];
uint8_t*	test_ptr;
uint8_t		rd_buf [512];
uint8_t		state = 0;		// user interface variable

uint16_t	err_cnt = 0;
uint16_t	total_errs = 0;
uint16_t	warn_cnt = 0;


//---------------------------< H E X _ P R I N T _ C O R E >--------------------------------------------------
//
// overcomes a weakness in arduino's Serial.print (<value>, HEX) function.  This function adds the missing
// leading zeros.
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


//---------------------------< G E T _ U S E R _ Y E S _ N O >------------------------------------------------
//
//
//

boolean get_user_yes_no (char* query, boolean yesno)
	{
	char c;
	Serial.print ("\r\nSALT> ");
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
		if (0x100 == (i & 0x100))
			Serial.print (".");				// every 4th read
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
//		hex_print_2_byte (address);
		hex_print (address);
		Serial.print ("    ");							// space between address and data

		
		for (uint8_t j=0; j<8; j++)						// first half
			{
//			hex_print_1_byte (*data_ptr++);
			hex_print (*data_ptr++);
			Serial.print (" ");							// space between bytes
			}
		Serial.print (" -  ");							// mid array separator

		for (uint8_t j=0; j<8; j++)						// second half
			{
//			hex_print_1_byte (*data_ptr++);
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


//---------------------------< H E X _ D U M P _ F R A M >----------------------------------------------------
//
// print a hex dump of fram contents in 256 byte chunks.
//

void hex_dump_fram (uint16_t start_address)
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


//---------------------------< S T O P W A T C H >------------------------------------------------------------
//
// Start and stop an elapsed timer
//

void stopwatch (boolean start_stop)
	{
	static uint32_t state = STOP;
	
	switch (state)
		{
		case STOP:
			if (start_stop)
				{
				start_time = millis();
				elapsed_time = TIMING;
				state = TIMING;
				break;
				}
			break;						// command to stop while stopped
		
		case TIMING:
			if (!start_stop)			// stop command
				{
				end_time = millis();
				elapsed_time = end_time - start_time;
				state = STOP;
				break;
				}
			start_time = millis();		// start command while timing is a restart
		}
	}


//---------------------------< F R A M _ F I L L _ T E S T >--------------------------------------------------
//
//
//

uint8_t fram_fill_test (uint8_t fill_val)
	{
	Serial.print ("\r\nfilling fram with 0x");
	if (0x10 > fill_val)
		Serial.print ("0");
	Serial.print (fill_val, HEX);
	
	stopwatch (START);						// start a timer
	fram_fill (fill_val, 0, FRAM_SIZE);
	stopwatch (STOP);						// stop the timer

	Serial.print ("\r\nwrote ");
	Serial.print (FRAM_SIZE);					// number of bytes
	Serial.print (" bytes in ");
	Serial.print (elapsed_time);				// elapsed time
	Serial.println ("ms");

	//----- verify fill
	Serial.print ("\r\nchecking fram fill    ");
	stopwatch (START);						// start a timer

												// page_read() max length is 256 bytes (i2c_t3 limit) but, that
	for (uint16_t i=0; i<FRAM_SIZE; i+=128)		// doesn't seem to work (never returns) work so read in chunks of 128
		{
		memset (test_buf, ~fill_val, 512);		// preset the test buffer to something other than the test value
		
		fram.set_addr16 (i);					// set the address
		fram.control.rd_buf_ptr = test_buf;		// point to test_buf
		fram.control.rd_wr_len = 128;			// number of bytes to read
		if (SUCCESS != fram.page_read())		// do it
			{
			Serial.print ("page read fail");
			Serial.print (i);
			return FAIL;
			}
		else									// successful read
			{
			if (0x180 == (i & 0x180))
				Serial.print (".");				// every 4th read
			}

		test_ptr = test_buf;					// point to start of buffer
		for (uint16_t j=0; j<130; j++)			// compare bytes in buffer against test value
			{
			if (fill_val == *test_ptr)			// should be 0
				{
				test_ptr ++;					// bump the pointer
				continue;						// do it again
				}
			if (128 == j)						// the 129th byte in the buffer should not be 0
				break;
			else								// one of 0-127 was not zero
				{
				stopwatch (STOP);			// stop the timer
				Serial.print ("\r\nFAILED: expected: ");
//				hex_print_1_byte (fill_val);
				hex_print (fill_val);
				Serial.print (", got: 0x");
//				hex_print_1_byte (*test_ptr);
				hex_print (*test_ptr);
				Serial.print ("; i: ");
				Serial.print (i);
				Serial.print ("; j: ");
				Serial.print (j);
				return FAIL;
				}
			}
		}

	stopwatch (STOP);						// stop the timer

	Serial.print ("\r\nchecked ");
	Serial.print (FRAM_SIZE);					// number of bytes
	Serial.print (" bytes in ");
	Serial.print (elapsed_time);				// elapsed time
	Serial.println ("ms");
	return SUCCESS;
	}



//---------------------------< M A K E _ C O U N T I N G _ P A T T E R N >------------------------------------
//
//  Given a starting 'seed', write an incrementing pattern of 251 bytes into the array pointed to by array_ptr.
//

void make_counting_pattern (uint8_t seed, uint8_t* array_ptr)
	{
	for (uint8_t i=0; i<251; i++)					// fill the test buffer with new test pattern
		*array_ptr++ = seed + i;
	}
	

//---------------------------< W R I T E _C O U N T I N G _ P A T T E R N >-----------------------------------
//
// write groups of 251 bytes as incrementing numbers.  The first groups are:
//	0..250
//	1..251
//	2..252
//	3..253
//	4..254
//	5..255
//	6..0		(counter rolls over here)
//	7..1
//	...
//	254..248
//	255..249
//	0..250
//	...
// Stop writing at the top of memory.  Repeatedly call this function with different seeds to test all possible
// byte values in all addresses.
//

uint8_t write_counting_pattern (uint8_t seed)
	{
	uint16_t	address = 0;
	
	Serial.print ("\r\n    writing           ");
	stopwatch (START);						// start a timer
	fram.control.rd_wr_len = 251;				// initial setting

	while (FRAM_SIZE > address)					// number of blocks to write (131st is a partial
		{
		if ((address + 251) & FRAM_SIZE)		// will we wrap during this write?
			fram.control.rd_wr_len = (FRAM_SIZE - address);		// yes, this is the number of bytes to write
		else
			fram.control.rd_wr_len = 251;		// no, write the whole enchilada

		make_counting_pattern (seed, test_buf);	// fill it with the next pattern
		
		fram.set_addr16 (address);				// set the address
		fram.control.wr_buf_ptr = test_buf;		// point to test_buf
		fram.page_write();						// do it

		if (0x100 == (address & 0x100))
			Serial.print (".");					// every 1k bytes
		seed++;

		address = (address + 251);
		}
	stopwatch (STOP);						// stop the timer

	Serial.print ("\r\n    wrote ");
	if (0 == __builtin_bswap16 (fram.control.addr.as_u16))
		Serial.print (32768);	// number of bytes (
	Serial.print (" pattern bytes in ");
	Serial.print (elapsed_time);				// elapsed time
	Serial.println ("ms");
	return SUCCESS;
	}



//---------------------------< C H E C K _C O U N T I N G _ P A T T E R N >-----------------------------------
//
//
//

uint8_t check_counting_pattern (uint8_t seed)
	{
	uint8_t		i;
	uint8_t*	test_ptr;
	uint8_t*	rd_ptr;
	uint16_t	address = 0;
	
	Serial.print ("    checking          ");
	stopwatch (START);						// start a timer
	fram.control.rd_wr_len = 251;				// initial setting

	while (FRAM_SIZE > address)					// number of blocks to write (131st is a partial
		{
		if ((address + 251) & FRAM_SIZE)		// will we wrap during this write?
			fram.control.rd_wr_len = (FRAM_SIZE - address);		// yes, this is the number of bytes to read
		else
			fram.control.rd_wr_len = 251;		// no, read the whole enchilada

		make_counting_pattern (seed, test_buf);	// fill it with the next pattern
		
		fram.set_addr16 (address);				// set the address
		fram.control.rd_buf_ptr = rd_buf;		// point to test_buf
		fram.page_read();						// do it

		rd_ptr = rd_buf;						// reset to start of received fram data
		test_ptr = test_buf;					// reset to start of test pattern buf
		for (i=0; i<fram.control.rd_wr_len; i++)
			{
			if (*rd_ptr == *test_ptr)
				{
				rd_ptr++;
				test_ptr++;
				continue;
				}

			stopwatch (STOP);			// stop the timer
			Serial.print ("\r\nFAILED: expected: 0x");
//			hex_print_1_byte (*test_ptr);	// test pattern value
			hex_print (*test_ptr);	// test pattern value
			Serial.print (", got: 0x");
//			hex_print_1_byte (*rd_ptr++);		// fram value
			hex_print (*rd_ptr++);		// fram value
			Serial.print ("; i: ");
			Serial.print (i);
			Serial.print ("; address: ");
			Serial.print (fram.control.rd_wr_len);
			hex_dump_array (rd_buf, 251);
			return FAIL;
			}
			
		if (0x100 == (address & 0x100))
			Serial.print (".");					// every 1k bytes
		seed++;

		address = (address + 251);
		}
	stopwatch (STOP);						// stop the timer

	Serial.print ("\r\n    checked ");
	if (0 == __builtin_bswap16 (fram.control.addr.as_u16))
		Serial.print (32768);	// number of bytes (
	Serial.print (" pattern bytes in ");
	Serial.print (elapsed_time);				// elapsed time
	Serial.println ("ms");
	return SUCCESS;
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

	Serial.print ("SALT diagnostics: ");
	Serial.print ("build time: ");				// assemble
	Serial.print (__TIME__);					// the
	Serial.print (" ");							// startup
	Serial.print (__DATE__);					// message
	
	fram.setup (0x50);
	fram.begin ();								// join i2c as master
	
	delay (2000);
	}


//---------------------------< L O O P >----------------------------------------------------------------------
//
//
//

void loop()
	{
	boolean	answer = false;

	//---------- < F R A M >----------
	
	fram.manufID = 0;							// make sure that we read these
	fram.prodID = 0;
	fram.get_device_id ();
	Serial.print ("\r\ndevice id: manufacturer ID: 0x");
//	hex_print_2_byte (fram.manufID);
	hex_print (fram.manufID);
	Serial.print ("; product ID: 0x");
//	hex_print_2_byte (fram.prodID);
	hex_print (fram.prodID);
	Serial.print ("\r\n");

	delay (2000);
	
	fram_fill_test (0);						// fill fram with 0x00
	fram_fill_test (0xAA);
	fram_fill_test (0x55);
	fram_fill_test (0xFF);
	
	if (!state)
		answer = get_user_yes_no ((char*)"do counting pattern test? (takes about 26 minutes)", true);	// 100kHz default answer yes
	else
		answer = get_user_yes_no ((char*)"do counting pattern test? (takes about 8 minutes)", true);	// 400kHz default answer yes
	
	if (answer)
		{
		for (uint16_t i=0; i<256; i++)
			{
			Serial.print ("counting pattern; seed = 0x");
//			hex_print_1_byte (i);
			hex_print (i);

			write_counting_pattern ((uint8_t)i);
			check_counting_pattern ((uint8_t)i);
			Serial.print ("\r\n");
			}
		}

	if (!state)
		{
		state = 1;
		if (!get_user_yes_no ((char*)"repeat tests at 400kHz bus rate?", true))	// default answer yes
			{
			Serial.print ("fram tests stopped.  reset to start again.");
			while (1);
			}
		else
			{
			Wire.setRate (48000000, I2C_RATE_400);
			Serial.println ("i2c bus rate set to 400kHz; restarting\n\n");
			}
		}
	else
		{
		Serial.print ((char*)"fram tests stopped.  reset to start again.");
		while (1);
		}
	}
