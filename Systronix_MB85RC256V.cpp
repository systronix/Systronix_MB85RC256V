#include <Arduino.h>
#include "Systronix_MB85RC256V.h"

//---------------------------< S E T U P >--------------------------------------------------------------------

void Systronix_MB85RC256V::setup (uint8_t base)
	{
	_base = base;
//	BaseAddr = base;
	}


//---------------------------< B E G I N >--------------------------------------------------------------------

void Systronix_MB85RC256V::begin (void)
	{
	Wire.begin();				// initialize I2C as master
	}


//---------------------------< I N I T >----------------------------------------------------------------------

void Systronix_MB85RC256V::init (void)
	{
	}


//---------------------------< S E T _ F R A M _ A D D R 1 6 >------------------------------------------------
//
// byte order is important.  In Teensy memory, a uint16_t is stored least-significant byte in the lower of two
// addresses.  The fram_addr union allows access to a single fram address as a struct of two bytes (high and
// low), as an array of two bytes [0] and [1], or as a uint16_t.
//
// This function simplifies keeping track of the current fram address pointer when using default_read_byte()
// which uses the fram's internal address pointer
//

uint8_t Systronix_MB85RC256V::set_addr16 (uint16_t addr)
	{
	if (addr & 0x8000)
		return DENIED;								// memory address out of bounds
	
	control.addr.as_u16 = __builtin_bswap16 (addr);	// byte swap and set the address
	return SUCCESS;
	}


//---------------------------< I N C _ A D D R 1 6 >----------------------------------------------------------
//
// byte order is important.  In Teensy memory, a uint16_t is stored least-significant byte in the lower of two
// addresses.  The fram_addr union allows access to a single fram address as a struct of two bytes (high and
// low), as an array of two bytes [0] and [1], or as a uint16_t.
//
// This function simplifies keeping track of the current fram address pointer when using current_address_read()
// which uses the fram's internal address pointer
//

void Systronix_MB85RC256V::inc_addr16 (void)
	{
	uint16_t addr = __builtin_bswap16 (control.addr.as_u16);
	control.addr.as_u16 = __builtin_bswap16 ((++addr & 0x7FFF));
	}


//---------------------------< A D V _ A D D R 1 6 >----------------------------------------------------------
//
// This function advances the current fram address pointer by rd_wr_en when using page_read() or page_write()
// to track the fram's internal address pointer.
//

void Systronix_MB85RC256V::adv_addr16 (void)
	{
	uint16_t addr = __builtin_bswap16 (control.addr.as_u16);
	control.addr.as_u16 = __builtin_bswap16 ((addr + control.rd_wr_len) & 0x7FFF);
	}


//---------------------------< B Y T E _ W R I T E >----------------------------------------------------------

void Systronix_MB85RC256V::byte_write (void)
	{
	Wire.beginTransmission(_base);			// init tx buff for xmit to slave at _base address
	Wire.write (control.addr.as_array, 2);		// put the memory address in the tx buffer
	Wire.write (control.wr_byte);
	Wire.endTransmission();					// xmit memory address
	}


//---------------------------< P A G E _ W R I T E >----------------------------------------------------------

void Systronix_MB85RC256V::page_write (void)
	{
	Wire.beginTransmission(_base);			// init tx buff for xmit to slave at _base address
	Wire.write (control.addr.as_array, 2);	// put the memory address in the tx buffer
	Wire.write (control.wr_buf_ptr, control.rd_wr_len);		// copy source to wire tx buffer data
	Wire.endTransmission();					// xmit address followed by data
	
	adv_addr16 ();							// advance our copy of the address
	}


//---------------------------< C U R R E N T _ A D D R E S S _ R E A D >--------------------------------------
//
// read a byte from the fram's current address pointer; pointer is bumped to the next location after the read.
// we presume that the fram's address pointer was previously set with byte_read().  This function attempts to
// track the fram's internal pointer by incrementing control.addr.as_u16.
//

void Systronix_MB85RC256V::current_address_read (void)
	{
	Wire.requestFrom(_base, 1, I2C_STOP);
	control.rd_byte = Wire.readByte();
	inc_addr16 ();					// bump our copy of the address to perhaps keep track of where the fran's pointer is
	}


//---------------------------< B Y T E _ R E A D >------------------------------------------------------------

void Systronix_MB85RC256V::byte_read (void)
	{
	Wire.beginTransmission(_base);			// init tx buff for xmit to slave at _base address
	Wire.write (control.addr.as_array, 2);	// put the memory address in the tx buffer
	Wire.endTransmission();					// xmit memory address
	
	current_address_read ();
	}


//---------------------------< P A G E _ R E A D >------------------------------------------------------------

void Systronix_MB85RC256V::page_read (void)
	{
	uint8_t i;
	
	Wire.beginTransmission(_base);			// init tx buff for xmit to slave at _base address
	Wire.write (control.addr.as_array, 2);	// put the memory address in the tx buffer
	Wire.endTransmission();					// xmit memory address

	Wire.requestFrom(_base, control.rd_wr_len, I2C_STOP);	// read the bytes
	
	adv_addr16 ();							// advance our copy of the address
	
	for (i=0;i<control.rd_wr_len; i++)		// copy wire rx buffer data to destination
		*control.rd_buf_ptr++ = Wire.readByte();
	}


//---------------------------< D E V I C E _ I D >------------------------------------------------------------

void Systronix_MB85RC256V::device_id (uint16_t *manuf_id_ptr, uint16_t *prod_id_ptr)
	{
	uint8_t a[3];

	Wire.beginTransmission (RSVD_SLAVE_ID >> 1);
	Wire.write (_base << 1);
	Wire.endTransmission (false);

	Wire.requestFrom (RSVD_SLAVE_ID >> 1, 3);
	
	a[0] = Wire.read();
	a[1] = Wire.read();
	a[2] = Wire.read();

	/* Shift values to separate manuf and prod IDs */
	/* See p.10 of http://www.fujitsu.com/downloads/MICRO/fsa/pdf/products/memory/fram/MB85RC256V-DS501-00017-3v0-E.pdf */
	*manuf_id_ptr = (a[0] << 4) + (a[1]  >> 4);
	*prod_id_ptr = ((a[1] & 0x0F) << 8) + a[2];
	}


