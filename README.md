# libmsr: a library to handle MSRxxx devices

This was coded instead of using the library kindly provided by the vendor, with the aim of being a robust and easy to use piece of software.

NOTE: This is not related to libmsr hosted on Google Code (see references).

**Project status**: active development. Please do use my library and let me know whether there are any other features you would like to have. Bug reports are also welcome.

**Implementation status**: fully working on a device I have.

**Tested devices**:

* MSR605

**Untested but should be working**:

* MSR206
* MSRE206
* MSR505
* MSR606
* Generally, any MSRxxx device with a similar command set

Please report your experience with these and other devices.

# API Documentation

See `libmsr.h` -- each API is commented. Documentation patches are welcome too.

# Building

The project/solution files supplied are for MSVC 2010. Feel free to contribute files for other MSVC versions or compilers!

# Future plans

* Add comm timeouts so code won't get stuck if a device doesn't respond
* Support more devices
* Test potentially unsupported features like reading nonstandard cards (to obtain them first...)

# References

* http://www.gae.ucm.es/~padilla/extrawork/stripe.html
* https://code.google.com/p/libmsr
* http://sourceforge.net/projects/stripesnoop/
