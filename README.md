# raspberry_clock
Driver for a 4x7 segment display clock (ht16K33v110) with optional light sensor (TSL2561)

This clock.c is driving a 4x7 segment display (ht16K33v110) to display the time.
The dimming is set based on a light sensor value from TSL2561 (CS package!). If no light sensor available the dimming is done by calulated sun-set and sun-down.
The display and the light sensor are connected to a raspberry pi 2 I2C outputs.

Inputs to call:
 - input 1: verbose settings 
			0: no output
			1: full output
			2: reduced output (only communication to the display)
 - input 2: save hh:min, lux value into a file
			0: no save
			1: save
 - input 3: enables the display light measurement function (the display displays an average value, not the time, and the dimming changes in every minute)
			any >0 integer can be used as enable
Neither of the input are mandatory, but only verosity can be defined solely.
e.g.: 
	./clock - no output to standard out or to file
	./clock 0 1 - output to file, but not to standard output
	./clock 1 - output to standard output, but not to file
