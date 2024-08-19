This is the source code for the 'wicap' project, a simple data acquisition application for 
the Pi RP2040. It includes a browser-based display, that can show the data in analogue or
digital form (i.e. as a storage oscilloscope or logic analyser).

The analog input uses an AD9226 parallel ADC, at up to 60 megasamples/sec. The software
assumes that only 10 bits are connected, to I/O pins 0 to 9. If using all 12 bits, the zero-volt
reference ANALOG_ZERO in the rpscope Javascript application will have to be changed, also the
ANALOG_SCALE value.

Note that some AD9226 modules have the most-significant bit labelled as D0. Also the analogue 
input usually has 50 ohm impedance, which is incompatible with most oscilloscope probes.

For more information see https://iosoft.blog/wicap

JPB 19/8/24
