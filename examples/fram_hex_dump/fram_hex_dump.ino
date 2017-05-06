//
// this code dumps 256 byte pages from fram beginning at address provided by user.  addresses must be in the
// range 0x0000-0x7FFF.  The low byte address is set to 0x00 because pages are dumped in 256-byte chunks.
//

//

#include <Systronix_MB85RC256V.h>
//#include <SALT_settings.h>
#include <SALT_FETs.h>
#include <SALT.h>
#include <SALT_utilities.h>
#include <SALT_JX.h>

Systronix_MB85RC256V fram;
//SALT_settings settings;
SALT_utilities utils;

SALT_FETs FETs;						// to turn of lights fans, alarm
SALT_JX coreJ2;						// to turn off heat pads, lamps, drawer locks
SALT_JX coreJ3;
SALT_JX coreJ4;


//---------------------------< S E T U P >--------------------------------------------------------------------

void setup()
	{
	Serial.begin(115200);						// usb; could be any value
	while((!Serial) && (millis()<10000));		// wait until serial monitor is open or timeout

	Serial1.begin(9600);								// UI habitat A LCD and keypad
	while((!Serial1) && (millis()<10000));				// wait until serial port is open or timeout


	Serial2.begin(9600);								// UI habitat B LCD and keypad
	while((!Serial2) && (millis()<10000));				// wait until serial port is open or timeout

	// These functions have a bug in TD 1.29 see forum post by KurtE...
	// ...https://forum.pjrc.com/threads/32502-Serial2-Alternate-pins-26-and-31
	Serial2.setRX (26);
	Serial2.setTX (31);	

	Serial1.println("r");								// 'r' initialize display so we can have a signon message
	Serial2.println("r");
	delay(50);

	//                0123456789ABCDEF0123456789ABCDEF
	Serial1.printf ("dNAP fram hex    dump utility    \r");
	Serial2.printf ("dNAP fram hex    dump utility    \r");
												// make sure all spi chip selects inactive
	pinMode (FLASH_CS_PIN, INPUT_PULLUP);		// SALT FLASH_CS(L)
	pinMode (T_CS_PIN, INPUT_PULLUP);			// SALT T_CS(L)
	pinMode (ETH_CS_PIN, INPUT_PULLUP);			// SALT ETH_CS(L)
	pinMode (DISP_CS_PIN, INPUT_PULLUP);		// SALT DISP_CS(L)
	
	pinMode (uSD_DETECT, INPUT_PULLUP);			// so we know if a uSD is in the socket

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

//	Serial.begin(115200);						// usb; could be any value
//	while((!Serial) && (millis()<10000));		// wait until serial monitor is open or timeout

	Serial.print ("NAP fram hex dump utility: ");
	Serial.print ("build time: ");				// assemble
	Serial.print (__TIME__);					// the
	Serial.print (" ");							// startup
	Serial.println (__DATE__);					// message

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

uint8_t		c;
uint16_t	address = 0;
uint8_t		character_counter = 0;

void loop()
	{
	Serial.println ("\n********** DO NOT SEND LINE ENDINGS**********");

	Serial.println ("enter 4-digit starting address in HEX format (00xx-7Fxx)");
	Serial.print ("SALT/fram hex dump> ");
	
	while (1)
		{
		if (Serial.available())
			{
			c = Serial.read();
			if (isxdigit(c))
				{
				if (('0' <= c) && ('9' >= c))
					c -= '0';					// convert to binary
				else if (('a' <= c) && ('f' >= c))
					c = (c - 'a') + 10;
				else
					c = (c - 'A') + 10;
				Serial.printf ("%x", c);
				
				address <<= 4;					// make room for new digit
				address |= c;					// tack on the new digit
				character_counter++;
				if (4 <= character_counter)
					{
					while (Serial.available())
						Serial.read();			// swallow extra characters
					break;						// got 4 digits go dump
					}
				}
			}
		}
	character_counter = 0;						
	address >>= 8;								// set low 8 bits to zero because pages are displayed in groups of 256 bytes
	address <<= 8;

	utils.fram_hex_dump (address);
	}
