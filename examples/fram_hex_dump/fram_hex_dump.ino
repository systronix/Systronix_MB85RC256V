//
// this code dumps 256 byte pages from fram beginning at address 0.  TODOprompt for a starting address
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

	Serial.print ("NAP ini loader [SD]: ");
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
	utils.fram_hex_dump (0);
	Serial.println("hex dump stopped; reset to restart");		// give up and enter an endless
	while(1);								// loop
	}


//---------------------------< L O O P >----------------------------------------------------------------------
//
//
//

void loop()
	{
	}
