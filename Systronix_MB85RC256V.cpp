#include <Arduino.h>
#include "Systronix_MB85RC256V.h"


//---------------------------< S E T U P >--------------------------------------------------------------------

uint8_t Systronix_MB85RC256V::setup (uint8_t base)
	{
	if ((FRAM_BASE_MIN > base) || (FRAM_BASE_MAX < base))
		{
		tally_errors (SILLY_PROGRAMMER);
		return FAIL;
		}

	_base = base;
	return SUCCESS;
	}


//---------------------------< B E G I N >--------------------------------------------------------------------
//
// TODO: add support for Wire1, Wire2, ... alternate pins, etc
//

void Systronix_MB85RC256V::begin (void)
	{
	Wire.begin();				// initialize I2C as master
	}


//---------------------------< B A S E _ G E T >--------------------------------------------------------------
//
//	return the I2C base address for this instance
//

uint8_t Systronix_MB85RC256V::base_get(void)
	{
	return _base;
	}


//---------------------------< I N I T >----------------------------------------------------------------------
//
// determines if there is a MB85RC256V at _base address by attempting to fetch the device manufacturer and
// product identifiers.
//

uint8_t Systronix_MB85RC256V::init (void)
	{
	uint16_t	prodID;
	uint16_t	manufID;

	if (SUCCESS == get_device_id (&manufID, &prodID) && (0x000A == manufID) && (0x0510 == prodID))
		{
		control.exists = true;
		return SUCCESS;
		}
	else
		{
		control.exists = false;		// only place in this file where this can be set
		return FAIL;
		}
	}


//---------------------------< T A L L Y _ E R R O R S >------------------------------------------------------
//
// Here we tally errors.  This does not answer the 'what to do in the event of these errors' question; it just
// counts them.
//

void Systronix_MB85RC256V::tally_errors (uint8_t value)
	{
	if (error.total_error_count < UINT64_MAX)
		error.total_error_count++; 	// every time here incr total error count

	error.error_val = value;

	switch (value)
		{
		case WR_INCOMPLETE:					// Wire.write failed to write all of the data to tx_buffer
			error.incomplete_write_count++;
			break;
		case 1:								// i2c_t3 and Wire: data too long from endTransmission() (rx/tx buffers are 259 bytes - slave addr + 2 cmd bytes + 256 data)
			error.data_len_error_count++;
			break;
#if defined I2C_T3_H
		case I2C_TIMEOUT:
			error.timeout_count++;			// 4 from i2c_t3; timeout from call to status() (read)
#else
		case 4:
			error.other_error_count++;		// i2c_t3 and Wire: from endTransmission() "other error"
#endif
			break;
		case 2:								// i2c_t3 and Wire: from endTransmission()
		case I2C_ADDR_NAK:					// 5 from i2c_t3
			error.rcv_addr_nack_count++;
			break;
		case 3:								// i2c_t3 and Wire: from endTransmission()
		case I2C_DATA_NAK:					// 6 from i2c_t3
			error.rcv_data_nack_count++;
			break;
		case I2C_ARB_LOST:					// 7 from i2c_t3; arbitration lost from call to status() (read)
			error.arbitration_lost_count++;
			break;
		case I2C_BUF_OVF:
			error.buffer_overflow_count++;
			break;
		case I2C_SLAVE_TX:
		case I2C_SLAVE_RX:
			error.other_error_count++;		// 9 & 10 from i2c_t3; these are not errors, I think
			break;
		case SILLY_PROGRAMMER:				// 11
			error.silly_programmer_error++;
			break;
		default:
			error.unknown_error_count++;
			break;
		}
	}


//---------------------------< S E T _ F R A M _ A D D R 1 6 >------------------------------------------------
//
// byte order is important.  In Teensy memory, a uint16_t is stored least-significant byte in the lower of two
// addresses.  The fram_addr union allows access to a single fram address as a struct of two bytes (high and
// low), as an array of two bytes [0] and [1], or as a uint16_t.
//
// Given the address 0x1234, this function stores it in the fram_addr union as:
//		fram.control.addr_as_u16:			0x3412 (use get_fram_addr16() to retrieve a properly ordered address)
//		fram.control.addr_as_struct.low:	0x34
//		fram.control.addr_as_struct.high:	0x12
//		fram.control.addr_as_array[0]:		0x12
//		fram.control.addr_as_array[1]:		0x34
//

uint8_t Systronix_MB85RC256V::set_addr16 (uint16_t addr)
	{
	if (addr & 0x8000)
		{
		tally_errors (SILLY_PROGRAMMER);
		return DENIED;								// memory address out of bounds
		}

	control.addr.as_u16 = __builtin_bswap16 (addr);	// byte swap and set the address
	return SUCCESS;
	}


//---------------------------< G E T _ F R A M _ A D D R 1 6 >------------------------------------------------
//
// byte order is important.  In Teensy memory, a uint16_t is stored least-significant byte in the lower of two
// addresses.  The fram_addr union allows access to a single fram address as a struct of two bytes (high and
// low), as an array of two bytes [0] and [1], or as a uint16_t.
//
// Returns fram address as a uint16_t in proper byte order
//
// See set_addr16() for additional explanation.
//

uint16_t Systronix_MB85RC256V::get_addr16 (void)
	{
	return __builtin_bswap16 (control.addr.as_u16);
	}


//---------------------------< I N C _ A D D R 1 6 >----------------------------------------------------------
//
// byte order is important.  In Teensy memory, a uint16_t is stored least-significant byte in the lower of two
// addresses.  The fram_addr union allows access to a single fram address as a struct of two bytes (high and
// low), as an array of two bytes [0] and [1], or as a uint16_t.
//
// This function simplifies keeping track of the current fram address pointer when using current_address_read()
// which uses the fram's internal address pointer.  Increments the address by one and makes sure that the address
// properly wraps from 0x7FFF to 0x0000 instead of going to 0x8000.
//
// See set_addr16() for additional explanation.
//

void Systronix_MB85RC256V::inc_addr16 (void)
	{
	uint16_t addr = __builtin_bswap16 (control.addr.as_u16);
	control.addr.as_u16 = __builtin_bswap16 ((++addr & 0x7FFF));
	}


//---------------------------< A D V _ A D D R 1 6 >----------------------------------------------------------
//
// This function advances the current fram address pointer by rd_wr_len when using page_read() or page_write()
// to track the fram's internal address pointer.  Advances the address and makes sure that the address
// properly wraps from 0x7FFF to 0x0000 instead of going to 0x8000.
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
//		write: a byte; txBufferLength = 4; return zero if overflow
//		endTransmission: does the write; returns:
//			0=success
//			1=data too long (as a result of Wire.write() causing an overflow)
//			2=recv addr NACK
//			3=recv data NACK
//			4=other error
//
// To use this function:
//		1. use set_addr16 (addr) to set the address in the control.addr union
//		2. write the byte to be transmitted into control.wr_byte
//		3. call this function
//

uint8_t Systronix_MB85RC256V::byte_write (void)
	{
	uint8_t ret_val;
	
	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;

	Wire.beginTransmission(_base);							// init tx buff for xmit to slave at _base address
	control.bytes_written = Wire.write (control.addr.as_array, 2);	// put the memory address in the tx buffer
	control.bytes_written += Wire.write (control.wr_byte);			// add data byte to the tx buffer
	if (3 != control.bytes_written)
		{
		tally_errors (WR_INCOMPLETE);						// only here 0 is error value since we expected to write more than 0 bytes
		return FAIL;
		}

	ret_val = Wire.endTransmission();				// xmit memory address and data byte
	if (SUCCESS != ret_val)
		{
		tally_errors (ret_val);						// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}

	if (error.successful_count < UINT64_MAX)
		error.successful_count++;

	return SUCCESS;
	}


//---------------------------< I N T 1 6 _ W R I T E >--------------------------------------------------------
//
// writes the two bytes of an int16_t or uint16_t to fram beginning at address in control.addr; ls byte is first.
//
// To use this function:
//		1. use set_addr16 (addr) to set the address in the control.addr union
//		2. write the 16-bit value to be transmitted into control.wr_int16 (there is no control.wr_uint16)
//		3. call this function
//

uint8_t Systronix_MB85RC256V::int16_write (void)
	{
	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	control.wr_buf_ptr = (uint8_t*)(&control.wr_int16);		// point to the int16 member of the control struct
	control.rd_wr_len = sizeof(uint16_t);					// set the write length
	return page_write ();									// do the write and done
	}


//---------------------------< I N T 3 2 _ W R I T E >--------------------------------------------------------
//
// writes the four bytes of an int32_t or uint32_t to fram beginning at address in control.addr; ls byte is first.
//
// To use this function:
//		1. use set_addr16 (addr) to set the address in the control.addr union
//		2. write the 32-bit value to be transmitted into control.wr_int32 (there is no control.wr_uint32)
//		3. call this function
//

uint8_t Systronix_MB85RC256V::int32_write (void)
	{
	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	control.wr_buf_ptr = (uint8_t*)(&control.wr_int32);		// point to the int32 member of the control struct
	control.rd_wr_len = sizeof(uint32_t);					// set the write length
	return page_write ();									// do the write and done
	}


//---------------------------< P A G E _ W R I T E >----------------------------------------------------------
//
// writes an array of control.rd_wr_len number of bytes to fram beginning at address in control.addr
//
// To use this function:
//		1. use set_addr16 (addr) to set the address in the control.addr union
//		2. set control.wr_buf_ptr to point at the array of bytes to be transmitted
//		3. set control.rd_wr_len to the number of bytes to be transmitted
//		4. call this function
//

uint8_t Systronix_MB85RC256V::page_write (void)
	{
	uint8_t ret_val;

	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	Wire.beginTransmission(_base);							// init tx buff for xmit to slave at _base address
	control.bytes_written = Wire.write (control.addr.as_array, 2);					// put the memory address in the tx buffer
	control.bytes_written += Wire.write (control.wr_buf_ptr, control.rd_wr_len);	// copy source to wire tx buffer data
	if (control.bytes_written < (2 + control.rd_wr_len))	// did we try to write too many bytes to the i2c_t3 tx buf?
		{
		tally_errors (WR_INCOMPLETE);						// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}
		
	ret_val = Wire.endTransmission();						// xmit memory address followed by data
	if (SUCCESS != ret_val)
		{
		tally_errors (ret_val);								// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}
	adv_addr16 ();											// advance our copy of the address

	if (error.successful_count < UINT64_MAX)
		error.successful_count++;

	return SUCCESS;
	}


//---------------------------< C U R R E N T _ A D D R E S S _ R E A D >--------------------------------------
//
// Read a byte from the fram's current address pointer; the fram's address pointer is bumped to the next
// location after the read.  We presume that the fram's address pointer was previously set with byte_read().
// This function attempts to track the fram's internal pointer by incrementing control.addr.
//
// To use this function:
//		1. perform some operation that correctly sets the fram's internal address pointer
//		2. call this function
//		3. retrieve the byte read from control.rd_byte
//

uint8_t Systronix_MB85RC256V::current_address_read (void)
	{
	uint8_t ret_val;

	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	control.bytes_received = Wire.requestFrom(_base, 1, I2C_STOP);
	if (1 != control.bytes_received)						// if we got more than or less than 1 byte
		{
		ret_val = Wire.status();					// to get error value
		tally_errors (ret_val);						// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}

	control.rd_byte = Wire.readByte();						// get the byte
	inc_addr16 ();											// bump our copy of the address

	if (error.successful_count < UINT64_MAX)
		error.successful_count++;

	return SUCCESS;
	}


//---------------------------< B Y T E _ R E A D >------------------------------------------------------------
//
// Read a byte from a specified address.
//
// To use this function:
//		1. use set_addr16 (addr) to set the address in the control.addr union
//		2. call this function
//		3. retrieve the byte read from control.rd_byte
//

uint8_t Systronix_MB85RC256V::byte_read (void)
	{
	uint8_t ret_val;

	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	Wire.beginTransmission(_base);							// init tx buff for xmit to slave at _base address
	control.bytes_written = Wire.write (control.addr.as_array, 2);	// put the memory address in the tx buffer
	if (2 != control.bytes_written)							// did we get correct number of bytes into the i2c_t3 tx buf?
		{
		tally_errors (WR_INCOMPLETE);						// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}

	ret_val = Wire.endTransmission();				// xmit memory address
	
	if (SUCCESS != ret_val)
		{
		tally_errors (ret_val);						// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}
	
	return current_address_read ();							// use current_address_read() to fetch the byte
	}


//---------------------------< I N T 1 6 _ R E A D >----------------------------------------------------------
//
// reads the two bytes of an int16_t or uint16_t from fram beginning at address in control.addr; ls byte is first.
//
// To use this function:
//		1. use set_addr16 (addr) to set the address in the control.addr union
//		2. call this function
//		3. retrieve the 16-bit value from control.rd_int16
//

uint8_t Systronix_MB85RC256V::int16_read (void)
	{
	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	control.rd_buf_ptr = (uint8_t*)(&control.rd_int16);
	control.rd_wr_len = sizeof(uint16_t);					// set the read length
	return page_read ();									// do the read and done
	}


//---------------------------< I N T 3 2 _ R E A D >----------------------------------------------------------
//
// reads the four bytes of an int32_t or uint32_t from fram beginning at address in control.addr; ls byte is first.
//
// To use this function:
//		1. use set_addr16 (addr) to set the address in the control.addr union
//		2. call this function
//		3. retrieve the 32-bit value from control.rd_int32
//

uint8_t Systronix_MB85RC256V::int32_read (void)
	{
	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	control.rd_buf_ptr = (uint8_t*)(&control.rd_int32);
	control.rd_wr_len = sizeof(uint32_t);					// set the read length
	return page_read ();									// do the read and done
	}


//---------------------------< P A G E _ R E A D >------------------------------------------------------------
//
// Reads control.rd_wr_len bytes from fram beginning at control.addr.  The number of bytes that can be read in
// a single operation is limited by the I2C_RX_BUFFER_LENGTH #define in i2c_t3.h.  Setting control.rd_wr_len to
// 256 is a convenient max.
//
// To use this function:
//		1. use set_addr16 (addr) to set the address in the control.addr union
//		2. set control.rd_wr_len to the number of bytes to be read
//		3. set control.rd_buf_ptr to point to the place where the data are to be stored
//		4. call this function
//

uint8_t Systronix_MB85RC256V::page_read (void)
	{
	uint8_t		ret_val;
	size_t 		i;
	uint8_t*	ptr = control.rd_buf_ptr;					// a copy so we don't disturb the original
	
	if (!control.exists)									// exit immediately if device does not exist
		return ABSENT;
	
	Wire.beginTransmission(_base);							// init tx buff for xmit to slave at _base address
	control.bytes_written = Wire.write (control.addr.as_array, 2);	// put the memory address in the tx buffer
	if (2 != control.bytes_written)							// did we get correct number of bytes into the i2c_t3 tx buf?
		{
		tally_errors (WR_INCOMPLETE);						// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}

	ret_val = Wire.endTransmission (I2C_NOSTOP);			// xmit memory address

	if (SUCCESS != ret_val)
		{
		tally_errors (ret_val);								// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}

	control.bytes_received = Wire.requestFrom(_base, control.rd_wr_len, I2C_STOP);	// read the bytes
	if (control.bytes_received != control.rd_wr_len)
		{
		ret_val = Wire.status();							// to get error value
		tally_errors (ret_val);								// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}

	for (i=0;i<control.rd_wr_len; i++)						// copy wire rx buffer data to destination
		*ptr++ = Wire.readByte();

	adv_addr16 ();											// advance our copy of the address

	if (error.successful_count < UINT64_MAX)
		error.successful_count++;

	return SUCCESS;
	}


//---------------------------< G E T _ D E V I C E _ I D >----------------------------------------------------
//
// Original code stolen from Adafruit.  Modified to use this library's control struct so that in the event of
// a failure, diagnostic information is available to external functions.  
//
	
uint8_t Systronix_MB85RC256V::get_device_id (uint16_t* manuf_id_ptr, uint16_t* prod_id_ptr)
	{
	uint8_t ret_val;
	uint8_t a[3] = { 0, 0, 0 };

	Wire.beginTransmission(RSVD_SLAVE_ID >> 1);				// (0xF8>>1)=0xFC; Wire shifts left to 0xF8
	Wire.write(_base << 1);
	ret_val = Wire.endTransmission(I2C_NOSTOP);
	
	if (SUCCESS != ret_val)
		{
		tally_errors (ret_val);						// increment the appropriate counter
		return FAIL;										// calling function decides what to do with the error
		}

	control.rd_wr_len = 3;									// set the number of bytes to read
	control.bytes_received = Wire.requestFrom(RSVD_SLAVE_ID >> 1, control.rd_wr_len, I2C_STOP);	// r/w bit for read makes 0xF9
	if (control.rd_wr_len != control.bytes_received)
		{
		ret_val = Wire.status();							// to get error value
		tally_errors (ret_val);								// increment the appropriate counter
		return FAIL;
		}

	a[0] = Wire.readByte();
	a[1] = Wire.readByte();
	a[2] = Wire.readByte();

	// Shift values to separate manuf and prod IDs; see p.10 of
	// http://www.fujitsu.com/downloads/MICRO/fsa/pdf/products/memory/fram/MB85RC256V-DS501-00017-3v0-E.pdf
	*manuf_id_ptr = (a[0] << 4) + (a[1]  >> 4);				// for MB85RC256V: 0x000A = fujitsu
	*prod_id_ptr = ((a[1] & 0x0F) << 8) + a[2];				// 0x0510 (5 is the density; 10 is proprietary

	if (error.successful_count < UINT64_MAX)
		error.successful_count++;

	return SUCCESS;
	}
