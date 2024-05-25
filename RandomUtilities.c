/*
 *	RandomUtilities.c
 *
 *	Copyright (C) 2012-2024 Rob Newberry <robthedude@mac.com>
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

#include "CommonUtilities.h"
#include "DebugUtilities.h"

#include "RandomUtilities.h"


#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>


int GenerateRandomData( void* buffer, size_t amount )
{
	int result = -1;
	int fd = -1;
	ssize_t num_bytes = 0;

#if TARGET_OS_FREERTOS
	#error
	// platform layer (or something like OpenThread) needs to provide an API for getting random dataa
#elif TARGET_OS_ZEPHYR
	#error
	// Zephyr has a mechanism for getting random data...
#else

	fd = open( "/dev/urandom", O_RDONLY );
	require( fd >= 0, exit );

	num_bytes = read( fd, buffer, amount );
	require( (size_t)num_bytes == amount, exit );

#endif
	
	result = 0;

exit:

	ForgetFD( &fd );
	return result;
}

char	RandomDigit( void )
{
	char	result;

	// GROSSLY INEFFICIENT -- we open/close /dev/random every time.
	//		consider using a static buffer and filling it every so often
	uint8_t		random_byte;
	GenerateRandomData( &random_byte, sizeof( random_byte ) );

	result = '0' + ( random_byte % 10 );

	return result;
}

char	RandomCharacter( void )
{
	char	result;

	// GROSSLY INEFFICIENT -- we open/close /dev/random every time.
	//		consider using a static buffer and filling it every so often
	while ( 1 )
	{
		uint8_t		random_byte;
		GenerateRandomData( &random_byte, sizeof( random_byte ) );

		if ( isalnum( random_byte ) )
		{
			result = random_byte;
			break;
		}
	}

	return result;
}

uint32_t RandomNumber( uint32_t minBound, uint32_t maxBound )
{
	uint32_t t;
	uint64_t v;
	uint64_t range = maxBound - minBound;
	
	GenerateRandomData( &t, sizeof( t ) );
	
	// range h
	v = t;
	v *= range;
	v /= UINT32_MAX;
	v += minBound;
	
	t = (uint32_t)v;
	
	return t;
}
