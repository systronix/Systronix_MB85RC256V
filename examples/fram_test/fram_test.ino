// this is a crude test of the major functions in Systronix MB85RC256V fram library.  Mostly it proves that
// Teensy can talk to fram on i2c port0

#include <Systronix_MB85RC256V.h>

Systronix_MB85RC256V fram;
Systronix_i2c_common i2c_common;

uint16_t manuf_id;
uint16_t prod_id;
//---------------------------< S E T U P >--------------------------------------------------------------------

void setup()
	{
	uint8_t i;
	const uint8_t byte_pattern = 0x0F;
	const uint16_t int16_pattern = 0x00FF;
	const uint16_t int32_pattern = 0x0000FFFF;
	uint8_t ret_val;						// a place to hold returned byte-size whatever
	uint16_t address;						// local copy of fram address


	Serial.begin(115200);     // use max baud rate
	while((!Serial) && (millis()<10000));    // wait until serial monitor is open or timeout
	Serial.println("fram test");

	if (SUCCESS != fram.setup (0x50))						// uses default wire settings
		{
		Serial.printf ("fram.setup (0x50) failed\ntest halted");
		while (1);
		}

	Serial.printf ("\tfram using %s @ base 0x%.2X\n", fram.wire_name, fram.base_get());

	fram.begin ();							// join i2c as master

	if (SUCCESS != fram.init())
		{
		Serial.printf ("fram.init() failed\ntest halted");
		while (1);
		}

	fram.get_device_id (&manuf_id, &prod_id);
	Serial.printf ("initialized\n\tfram manuf. ID: 0x%.4X\n\tfram prod. ID: 0x%.4X\n", manuf_id, prod_id);

// test address setting and getting
	ret_val = fram.set_addr16 (0);			// attempt to set min address
	if (SUCCESS != ret_val)
		{
		Serial.printf ("fram.set_addr16 (0) failed; min address\ntest halted");
		while (1);
		}

	address = fram.get_addr16 ();
	if (0 != address)
		{
		Serial.printf ("fram.get_addr16() failed; expected: 0x0000; got: 0x%.4X\ntest halted", address);
		while (1);
		}

	ret_val = fram.set_addr16 (0x7FFF);		// attempt to set max address
	if (SUCCESS != ret_val)
		{
		Serial.printf ("fram.set_addr16 (0x7FFF) failed; max address\ntest halted");
		while (1);
		}

	address = fram.get_addr16 ();
	if (0x7FFF != address)
		{
		Serial.printf ("fram.get_addr16() failed; expected: 0x7FFF; got: 0x%.4X\ntest halted", address);
		while (1);
		}

	ret_val = fram.set_addr16 (0x8000);		// attempt to set address out of range
	if (SUCCESS == ret_val)
		{
		Serial.printf ("fram.set_addr16 (0x8000) failed; out of range\ntest halted");
		while (1);
		}

	address = fram.get_addr16 ();
	if (0x8000 & address)		// if address is out of range; address should not have changed
		{
		Serial.printf ("fram.get_addr16() failed; expected: 0x07FF; got: 0x%.4X\ntest halted", address);
		while (1);
		}

	Serial.printf ("address setting tests pass\n");

//---------- < P I N G >----------

	if (SUCCESS != fram.ping_eeprom())		// TODO: delete fram.ping_eeprom()? fram not an eeprom
		{
		Serial.printf ("fram.ping_eeprom() failed\ntest halted");
		while (1);
		}

	Serial.printf ("ping test: pass\n");


//----------< R E A D / W R I T E   B Y T E >----------
// use byte_read() and current_address_read() to test protected inc_addr16()

	fram.set_addr16 (0);					// set min address
	ret_val = fram.byte_read();
	if (SUCCESS != ret_val)
		{
		Serial.printf ("fram.byte_read() failed; returned: 0x%.2X\ntest halted", ret_val);
		while (1);
		}

	address = fram.get_addr16 ();
	if (0x0001 != address)
		{
		Serial.printf ("fram.get_addr16() failed after byte_read(); expected: 0x0001; got: 0x%.4X\ntest halted", address);
		while (1);
		}

// write a pattern of pattern four bytes, one byte at a time

	Serial.printf ("write byte:\n");
	for (i=0; i<4; i++)
		{
		fram.set_addr16 (i);				// set address for each write (fram does not inc internal addr counter for byte writes)
		fram.control.wr_byte = byte_pattern << i;
		ret_val = fram.byte_write();
		if (SUCCESS != ret_val)
			{
			Serial.printf ("fram.byte_write() failed; returned: 0x%.2X\ntest halted", ret_val);
			while (1);
			}
		Serial.printf ("\t[%d]: 0x%.2X\n", i, fram.control.wr_byte);
		}

// read and validate the 4-byte pattern
	fram.set_addr16 (0);					// set min address
	Serial.printf ("read byte:\n");
	for (i=0; i<4; i++)
		{
		ret_val = fram.byte_read();
		if (SUCCESS != ret_val)
			{
			Serial.printf ("fram.byte_read() failed; returned: 0x%.2X\ntest halted", ret_val);
			while (1);
			}

		if (fram.control.rd_byte != (byte_pattern << i))
			{
			Serial.printf ("fram.byte_write() / fram.byte_read() sequence[%d] failed; read: 0x%.2X; expected: 0x%.2X\ntest halted", i, fram.control.rd_byte, (byte_pattern << i));
			while (1);
			}
		Serial.printf ("\t[%d]: 0x%.2X\n", i, fram.control.rd_byte);
		}

	Serial.printf ("byte write / read test: pass\n");

//----------< R E A D / W R I T E   I N T 1 6 >----------
// use int16_read() and page_read() to test protected adv_addr16()

	fram.set_addr16 (0);					// set min address
	ret_val = fram.int16_read();
	if (SUCCESS != ret_val)
		{
		Serial.printf ("fram.int16_read() failed; returned: 0x%.2X\ntest halted", ret_val);
		while (1);
		}

	address = fram.get_addr16 ();
	if (0x0002 != address)
		{
		Serial.printf ("fram.get_addr16() failed after int16_read(); expected: 0x0002; got: 0x%.4X\ntest halted", address);
		while (1);
		}

// write a pattern of pattern four int16s, one int16 at a time

	Serial.printf ("write int16:\n");
	fram.set_addr16 (0);					// set min address; page write does advance the internal counter
	for (i=0; i<4; i++)
		{
		fram.control.wr_int16 = int16_pattern << i;
		ret_val = fram.int16_write();
		if (SUCCESS != ret_val)
			{
			Serial.printf ("fram.int16_write() failed; returned: 0x%.2X\ntest halted", ret_val);
			while (1);
			}
		Serial.printf ("\t[%d]: 0x%.4X\n", i<<1, fram.control.wr_int16);
		}

// read and validate the 4-byte pattern
	fram.set_addr16 (0);					// set min address
	Serial.printf ("read int16:\n");
	for (i=0; i<4; i++)
		{
		ret_val = fram.int16_read();
		if (SUCCESS != ret_val)
			{
			Serial.printf ("fram.int16_read() failed; returned: 0x%.2X\ntest halted", ret_val);
			while (1);
			}

		if (fram.control.rd_int16 != (int16_pattern << i))
			{
			Serial.printf ("fram.int16_write() / fram.int16_read() sequence[%d] failed; read: 0x%.4X; expected: 0x%.4X\ntest halted", i<<1, fram.control.rd_int16, (int16_pattern << i));
			while (1);
			}
		Serial.printf ("\t[%d]: 0x%.4X\n", i<<1, fram.control.rd_int16);
		}

	Serial.printf ("int16 write / read test: pass\n");

//----------< R E A D / W R I T E   I N T 3 2 >----------
// use int32_read() and page_read() to test protected adv_addr32()

	fram.set_addr16 (0);					// set min address
	ret_val = fram.int32_read();
	if (SUCCESS != ret_val)
		{
		Serial.printf ("fram.int32_read() failed; returned: 0x%.2X\ntest halted", ret_val);
		while (1);
		}

	address = fram.get_addr16 ();
	if (0x0004 != address)
		{
		Serial.printf ("fram.get_addr16() failed after int32_read(); expected: 0x0004; got: 0x%.4X\ntest halted", address);
		while (1);
		}

// write a pattern of pattern four int32s, one int32 at a time

	Serial.printf ("write int32:\n");
	fram.set_addr16 (0);					// set min address; page write does advance the internal counter
	for (i=0; i<4; i++)
		{
		fram.control.wr_int32 = int32_pattern << i;
		ret_val = fram.int32_write();
		if (SUCCESS != ret_val)
			{
			Serial.printf ("fram.int32_write() failed; returned: 0x%.2X\ntest halted", ret_val);
			while (1);
			}
		Serial.printf ("\t[%d]: 0x%.8X\n", i<<2, fram.control.wr_int32);
		}

// read and validate the 4-byte pattern
	fram.set_addr16 (0);					// set min address
	Serial.printf ("read int32:\n");
	for (i=0; i<4; i++)
		{
		ret_val = fram.int32_read();
		if (SUCCESS != ret_val)
			{
			Serial.printf ("fram.int32_read() failed; returned: 0x%.2X\ntest halted", ret_val);
			while (1);
			}

		if (fram.control.rd_int32 != (int32_pattern << i))
			{
			Serial.printf ("fram.int32_write() / fram.int32_read() sequence[%d] failed; read: 0x%.8X; expected: 0x%.8X\ntest halted", i<<2, fram.control.rd_int32, (int32_pattern << i));
			while (1);
			}
		Serial.printf ("\t[%d]: 0x%.8X\n", i<<2, fram.control.rd_int32);
		}

	Serial.printf ("int16 write / read test: pass\n");
	Serial.printf ("done");
	}

void loop()
{

}