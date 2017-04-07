# Systronix_MB85RC256V
Arduino library for Fujitsu MB85RC256V 256 Kbit (32 KByte) I2C FRAM

## example files
### rcv_ini_file.ino
this code is obsolete

### ini_loader.ino
this code reads a habitat .ini file from the usb serial port, validates the file's settings, and writes the file to fram.  It then calculates a crc and stores that in fram.

Additionally, this code allows inspection of the saved file and of the log portion of the fram.  It is also possible to initialize the log memory.

### SALT_diagnostics.ino
initial hack at a diagnostic tool.  Currently this code just tests the FRAM.

##control struct
interface to and from the functions in this file are through a struct.  This allows the individual functions to return simple SUCCESS or FAIL status.

The struct has members to hold memory address; single byte written and read; pointers to read and write buffers; counts of the number of bytes to write or read; and status values.

To write a single byte to fram, set the address by calling set_addr16().  Alternately, individual bytes of an address may be set by wrting to the struct members control.addr.as_struct.high and control.addr.as_struct.low.  Next set the byte by writing the byte value to control.wr_byte.  Now call byte_wrte().

Similarly, to read a byte, set the address and call byte_read().  The byte read from fram is returned in control.rd_byte.


## function list

### byte_read()
reads a byte from fram
control.address.as_u16 gets address; use set_addr16()
control.rd_byte gets the data read from fram

returns:
  SUCCESS when the byte was successfully read
  ABSENT when the device failed the detection test in init()
  FAIL when the i2c_t3 library reports an error; the returned error value is stored in control.retval
  
### byte_write()
writes a byte to fram
control.address.as_u16 gets address; use set_addr16()
control. wr_byte get the data to write

returns:
  SUCCESS when the byte was successfully written
  ABSENT when the device failed the detection test in init()
  FAIL when the i2c_t3 library reports an error; the returned error value is stored in control.retval
  addr.addr.as_u16 hold the next available address
  control.ret_val holds the return value provided by the i2c_t3 library
  

### current_address_read
