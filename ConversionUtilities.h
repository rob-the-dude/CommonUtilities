/*
 *	ConversionUtilities.h
 *
 *	Copyright (C) 2016-2024 Rob Newberry <robthedude@mac.com>
 *
 *	Redistribution and use in source and binary forms, with or without modification,
 *	are permitted provided that the following conditions are met:
 *
 *	1.	Redistributions of source code must retain the above copyright notice,
 *		this list of conditions and the following disclaimer.
 *
 *	2.	Redistributions in binary form must reproduce the above copyright notice,
 * 		this list of conditions and the following disclaimer in the documentation
 *		and/or other materials provided with the distribution.
 *
 *	3.	Neither the name of the copyright holder nor the names of its contributors
 *		may be used to endorse or promote products derived from this software
 *		without specific prior written permission.
 *
 *	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
 *	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *	THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __CONVERSION_UTILITIES_H__
#define __CONVERSION_UTILITIES_H__

#ifdef __cplusplus
extern "C" {
#endif

#define	celsius_to_fahrenheit( c ) \
	( ( ( ( ( (float)c ) * 9.0 ) / 5.0 ) + 32.0 ) )

#define CelsiusToFahrenheit(c)		celsius_to_fahrenheit(c)


#define fahrenheit_to_celsius( f ) \
	( ( ( ( ( (float)f ) - 32.0 ) * 5.0 ) / 9.0 ) )

#define FahrenheitToCelsius(f)		fahrenheit_to_celsius( f )


double RoundToDecimalPlaces( double v, unsigned int places );


#endif


#ifdef __cplusplus
} // extern "C"
#endif

#endif // __CONVERSION_UTILITIES_H__
