'SALT_INI_File_Issues'
# What works and what doesn't in sending ini file to Teensy/SALT

## 1K rcv_ini_file Aug 03 21:55, this would have the SALT_settings bug
comments	Delays		PC			Win Ver		RT2/3	Results
--------	-------		-------		--------	-----	-------
long 804	0/100		Yoga S11	10 Home		3		works, no detail. Strings of '.' then dump of FRAM
it's possible this is a read of data left in FRAM, here is the output. Should we see a "writing" message?
receiving ......................................................................
........................................................
fram write complete

## 8K Aug 05 10:44, this would have the SALT_settings bug
comments	Delays		PC			Win Ver		RT2/3	Results
--------	-------		-------		--------	-----	-------
short 0805	0/100		Yoga S1		10	Home	2		work
long 0804	0/100										rcvd 173 lines in 21329 ms, wrote 126 in 262ms								
long 0804	0/0											rcvd 173 lines in 1296 ms, wrote 126 in 262ms
long 0804	0/100		Yoga S11	10 Pro		3		Bruce D, CA fails with long/short/no comments
short 0805	0/100										fails
none 0805	0/100										fails

## 8K Aug 5 21:48 Recompile to include Scott's fix to SALT_settings, 8K fetch by char
comments	Delays		PC			Win Ver		RT2/3	Results
--------	-------		-------		--------	-----	-------
long 0804	0/100		Yoga S1		10	Home	2	rcvd 4273 chars 21199 ms, wrote 126 lines 263 ms
			0/0											rcvd 4273 chars 1258 ms, wrote 126 lines 263 ms
			-2/0									3	rcvd 4273 chars 1097 ms, wrote 126 lines 263 ms	