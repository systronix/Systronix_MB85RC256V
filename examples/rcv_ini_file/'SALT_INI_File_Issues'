'SALT_INI_File_Issues'
# What works and what doesn't in sending ini file to Teensy/SALT

## Version from Aug 05
rcv_ini_file		comments	Delays		PC			Win Ver		RT2/3	Results
------------		--------	-------		-------		--------	-----	-------
8K Aug 5 10:44		short 0805	0/100		Yoga S1		10	Home	2		work
ditto				long 0804	0/100										rcvd 173 lines in 21329 ms, wrote 126 in 262ms								
ditto				long 0804	0/0											rcvd 173 lines in 1296 ms, wrote 126 in 262ms
ditto				long 0804	0/100		Yoga S11	10 Pro		3		Bruce D, CA fails with long/short/no comments
					short 0805	0/100										fails
					none 0805	0/100										fails

## Recompile to include Scott's fix to SALT_settings, 8K fetch by char Aug 5 21:48
8K Aug 5 21:48		long 0804	0/100		Yoga S1		10	Home	2	rcvd 4273 chars 21199 ms, wrote 126 lines 263 ms
								0/0											rcvd 4273 chars 1258 ms, wrote 126 lines 263 ms
								-2/0									3	rcvd 4273 chars 1097 ms, wrote 126 lines 263 ms	