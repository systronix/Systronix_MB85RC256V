#include <Arduino.h>
#include "Systronix_MB85RC256V.h"


//---------------------------< S E T U P >--------------------------------------------------------------------

void Systronix_MB85RC256V::setup (uint8_t base)
	{
	_base = base;
	}


//---------------------------< B E G I N >--------------------------------------------------------------------

void Systronix_MB85RC256V::begin (void)
	{
	Wire.begin();				// initialize I2C as master
	}


//---------------------------< I N I T >----------------------------------------------------------------------
//
// determines if there is a MB85RC256V at _base address by attempting to fetch the device manufacturer and
// product identifiers.
//

uint8_t Systronix_MB85RC256V::init (void)
	{
	if (SUCCESS == get_device_id () && (0x000A == manufID) && (0x0510 == prodID))
		{
		control.exists = true;
		return SUCCESS;
		}
	else
		{
		control.exists = false;
		return FAIL;
		}
	}


//---------------------------< T A L L Y _ E R R O R S >------------------------------------------------------
//
// Here we tally errors.  This does not answer the 'what to do in the event of these errors' question; it just
// counts them.  If the device does not ack the address portion of a transaction or if we get a timeout error,
// exists is set to false.  We assume here that the timeout error is really an indication that the automatic
// reset feature of the i2c_t3 library failed to reset the device in which case, the device no longer 'exists'
// for whatever reason.
//

void Systronix_MB85RC256V::tally_errors (uint8_t error)
	{
	switch (error)
		{
		case 1:					// data too long from endTransmission() (rx/tx buffers are 259 bytes - slave addr + 2 cmd bytes + 256 data)
		case 8:					// buffer overflow from call to status() (read - transaction never started)
			data_len_error_count ++;
			break;
		case 2:					// slave did not ack address (write)
		case 5:					// from call to status() (read)
			rcv_addr_nack_count ++;
			control.exists = false;
			break;
		case 3:					// slave did not ack data (write)
		case 6:					// from call to status() (read)
			rcv_data_nack_count ++;
			break;
		case 4:					// arbitration lost (write) or timeout (read/write)
		case 7:					// arbitration lost from call to status() (read)
			other_error_count ++;
			control.exists=false;
		}
	}


//---------------------------< S E T _ F R A M _ A D D R 1 6 >------------------------------------------------
//
// byte order is important.  In Teensy memory, a uint16_t is stored least-significant byte in the lower of two
// addresses.  The fram_addr union allows access to a single fram address as a struct of two bytes (high and
// low), as an array of two bytes [0] and [1], or as a uint16_t.
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
// This function advances the current fram address pointer by rd_wr_len when using page_read() or page_write()
// to track the fram's internal address pointer.
//

void Systronix_MB85RC256V::adv_addr16 (void)
	{
	uint16_t addr = __builtin_bswap16 (control.addr.as_u16);
	control.addr.as_u16 = __builtin_bswap16 ((addr + control.rd_wr_len) & 0x7FFF);
	}


//---------------------------< B Y T E _ W R I T E >----------------------------------------------------------
//
// i2c_t3 error returns
//		beginTransmission: none, void function sets txBufferLength to 1
//		write: two address bytes; txBufferLength = 3; return 0 if txBuffer overflow (tx buffer is 259 bytes)
//		write: a byte; txBufferLength = 3; return zero if overflow
//		endTransmission: does the write; returns:
//			0=success
//			1=data too long (as a result of Wire.write() causing an overflow)
//			2=recv addr NACK
//			3=recv data NACK
//			4=other error
//

uint8_t Systronix_MB85RC256V::byte_write (void)
	{
	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	Wire.beginTransmission(_base);							// init tx buff for xmit to slave at _base address
	Wire.write (control.addr.as_array, 2);					// put the memory address in the tx buffer
	control.bytes_written = Wire.write (control.wr_byte);
	control.ret_val = Wire.endTransmission();				// xmit memory address

	if (SUCCESS == control.ret_val)
		return SUCCESS;
	tally_errors (control.ret_val);							// increment the appropriate counter
	return FAIL;											// calling function decides what to do with the error
	}


//---------------------------< P A G E _ W R I T E >----------------------------------------------------------

uint8_t Systronix_MB85RC256V::page_write (void)
	{
	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	Wire.beginTransmission(_base);							// init tx buff for xmit to slave at _base address
	Wire.write (control.addr.as_array, 2);					// put the memory address in the tx buffer
	control.bytes_written = Wire.write (control.wr_buf_ptr, control.rd_wr_len);	// copy source to wire tx buffer data
	if (I2C_BUF_OVF == Wire.status())						// did we try to write too many bytes to the i2c tx buf?
		{
		tally_errors (I2C_BUF_OVF);							// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}
		
	control.ret_val = Wire.endTransmission();				// xmit address followed by data
	
	if (SUCCESS == control.ret_val)
		{
		adv_addr16 ();										// advance our copy of the address
		return SUCCESS;
		}
	tally_errors (control.ret_val);							// increment the appropriate counter
	return FAIL;											// calling function decides what to do with the error
	}


//---------------------------< C U R R E N T _ A D D R E S S _ R E A D >--------------------------------------
//
// read a byte from the fram's current address pointer; pointer is bumped to the next location after the read.
// we presume that the fram's address pointer was previously set with byte_read().  This function attempts to
// track the fram's internal pointer by incrementing control.addr.as_u16.
//

uint8_t Systronix_MB85RC256V::current_address_read (void)
	{
	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	control.bytes_received = Wire.requestFrom(_base, 1, I2C_STOP);
	if (control.bytes_received)								// if a byte was received
		{
		control.rd_byte = Wire.readByte();					// get the byte
		inc_addr16 ();										// bump our copy of the address
		return SUCCESS;
		}

	control.ret_val = Wire.status();						// to get error value
	tally_errors (control.ret_val);							// increment the appropriate counter
	return FAIL;											// calling function decides what to do with the error
	}


//---------------------------< B Y T E _ R E A D >------------------------------------------------------------

uint8_t Systronix_MB85RC256V::byte_read (void)
	{
	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	Wire.beginTransmission(_base);							// init tx buff for xmit to slave at _base address
	Wire.write (control.addr.as_array, 2);					// put the memory address in the tx buffer
	control.ret_val = Wire.endTransmission();				// xmit memory address
	
	if (control.ret_val)
		{
		tally_errors (control.ret_val);						// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}
	
	return current_address_read ();							// use current_address_read() to fetch the byte
	}


//---------------------------< P A G E _ R E A D >------------------------------------------------------------
//
// Reads control.rd_wr_len bytes from fram beginning at control.addr.  The number of bytes that can be read in
// a single operation is limited by the I2C_RX_BUFFER_LENGTH #define in i2c_t3.h.  Setting control.rd_wr_len to
// 256 is a convenient max.
//

uint8_t Systronix_MB85RC256V::page_read (void)
	{
	size_t 		i;
	uint8_t*	ptr = control.rd_buf_ptr;					// a copy so we don't disturb the original
	
	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	Wire.beginTransmission(_base);							// init tx buff for xmit to slave at _base address
	Wire.write (control.addr.as_array, 2);					// put the memory address in the tx buffer
	control.ret_val = Wire.endTransmission (I2C_NOSTOP);	// xmit memory address

	if (control.ret_val)
		{
		tally_errors (control.ret_val);						// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}

	control.bytes_received = Wire.requestFrom(_base, control.rd_wr_len, I2C_STOP);	// read the bytes
	if (control.bytes_received == control.rd_wr_len)
		{
		adv_addr16 ();										// advance our copy of the address

		for (i=0;i<control.rd_wr_len; i++)					// copy wire rx buffer data to destination
			*ptr++ = Wire.readByte();
		return SUCCESS;
		}

	control.ret_val = Wire.status();						// to get error value
	tally_errors (control.ret_val);							// increment the appropriate counter
	return FAIL;											// calling function decides what to do with the error
	}


//---------------------------< G E T _ D E V I C E _ I D >----------------------------------------------------
//
// Original code stolen from Adafruit.  Modified to use this library's control struct so that in the event of
// a failure, diagnostic information is available to external functions.  
//
	
uint8_t Systronix_MB85RC256V::get_device_id (void)
	{
	uint8_t a[3] = { 0, 0, 0 };

	Wire.beginTransmission(RSVD_SLAVE_ID >> 1);				// (0xF8>>1)=0xFC; Wire shifts left to 0xF8
	Wire.write(_base << 1);
	control.ret_val = Wire.endTransmission(false);
	
	if (SUCCESS == control.ret_val)
		{
		control.rd_wr_len = 3;								// set the number of bytes to read
		control.bytes_received = Wire.requestFrom(RSVD_SLAVE_ID >> 1, control.rd_wr_len, I2C_STOP);	// r/w bit for read makes 0xF9
		if (control.rd_wr_len == control.bytes_received)
			{
			a[0] = Wire.read();
			a[1] = Wire.read();
			a[2] = Wire.read();
			
			// Shift values to separate manuf and prod IDs; see p.10 of
			// http://www.fujitsu.com/downloads/MICRO/fsa/pdf/products/memory/fram/MB85RC256V-DS501-00017-3v0-E.pdf
			manufID = (a[0] << 4) + (a[1]  >> 4);			// for MB85RC256V: 0x000A = fujitsu
			prodID = ((a[1] & 0x0F) << 8) + a[2];			// 0x0510 (5 is the density; 10 is proprietary
			return SUCCESS;
			}
		}
	control.ret_val = Wire.status();						// to get error value
	tally_errors (control.ret_val);							// increment the appropriate counter
	return FAIL;
	}
