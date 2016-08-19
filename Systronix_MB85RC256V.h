#ifndef MB85RC256V_H_
#define	MB85RC256V_H_

#include<Arduino.h>

//---------------------------< D E F I N E S >----------------------------------------------------------------
//
// Include the lowest level I2C library
//

#if defined (__MK20DX256__) || defined (__MK20DX128__) 	// Teensy 3.1 or 3.2 || Teensy 3.0
#include <i2c_t3.h>		
#else
#include <Wire.h>	// for AVR I2C library
#endif

#define RSVD_SLAVE_ID	(0xF8)

#define	SUCCESS	0
#define	FAIL	0xFF
#define	DENIED	0xFE
#define	ABSENT	0xFD

#define	I2C_BUF_OVF	8		// mimic the same-named i2c_t3 i2c_status enum



//---------------------------< C L A S S >--------------------------------------------------------------------
//
//
//

class Systronix_MB85RC256V
	{
	protected:
		uint8_t		_base;							// base address, eight possible values
		
		void		adv_addr16 (void);				// advance control.addr.u16 by control.rd_wr_len
		void		inc_addr16 (void);				// increment control.addr.u16 by 1
		void		tally_errors (uint8_t);			// maintains the i2c_t3 error counters

	public:
													// i2c_t3 error counters
		uint32_t	data_len_error_count;			// data too long
		uint32_t	rcv_addr_nack_count;			// slave did not ack address
		uint32_t	rcv_data_nack_count;			// slave did not ack data
		uint32_t	other_error_count;				// arbitration lost or timeout
	
		union fram_addr
			{
			struct
				{
				uint8_t		high;
				uint8_t		low;
				} as_struct;
			uint16_t		as_u16;
			uint8_t			as_array[2];
			};
			
		struct
			{
			boolean				exists;				// set false after an unsuccessful i2c transaction
			union fram_addr		addr;
			uint8_t				wr_byte;			// a place to put single read and write bytes
			uint8_t				rd_byte;
			uint8_t*			wr_buf_ptr;			// pointers to read / write buffers; buffers must be appropriately sized
			uint8_t*			rd_buf_ptr;
			size_t				rd_wr_len;			// number of bytes to read/write with page_read()/page_write()
			size_t				bytes_written;		// number bytes written by Wire.write(); 0 = fail
			size_t				bytes_received;		// number of bytes read by Wire.requestFrom()
			uint8_t				ret_val;			// SUCCESS or error return value of the last transaction
			} control;

		uint16_t	prodID;
		uint16_t	manufID;

		void setup (uint8_t base);					// constructor
		void begin (void);							// joins I2C as master
		uint8_t init (void);						// determines if the device at _base is correct and communicating

		uint8_t set_addr16 (uint16_t);
		
		uint8_t byte_write (void);						// write 1 byte to address
		uint8_t page_write (void);						// write n number of bytes beginning at address
		uint8_t current_address_read (void);			// get the byte at the current position of the device's address pointer
		uint8_t byte_read (void);						// read 1 byte from address
		uint8_t default_byte_read (void);				// read 1 byte from fram's current address pointer
		uint8_t page_read (void);						// read n number of bytes beginning at address
		uint8_t get_device_id (void);					// get the device id
//		uint8_t get_device_id (uint16_t *, uint16_t *);		// get the device id
	private:
	};
	
extern Systronix_MB85RC256V fram;

#endif	// MB85RC256V_H_