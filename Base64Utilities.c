/*
 *	Base64Utilities.c
 *
 *	Copyright (C) 2019-2024 Rob Newberry <robthedude@mac.com>
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

#include "Base64Utilities.h"

#include "CommonUtilities.h"
#include "DebugUtilities.h"

#include <stdarg.h>
#include <string.h>

static char sEncodeTable[] =
	{
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
		'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
		'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
		'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
		'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '0', '1', '2', '3',
		'4', '5', '6', '7', '8', '9', '+', '/'
	};
static bool	sDecodeTableInitialized = false;
static char sDecodeTable[256] = { 0 };
static int sModTable[] = { 0, 2, 1 };

static void BuildDecodeTable( void )
{
	require_quiet( sDecodeTableInitialized == false, exit );

	size_t i;
	for ( i = 0; i < NELEMENTS( sEncodeTable ); i++ )
	{
		sDecodeTable[ (uint8_t)sEncodeTable[i] ] = i;
	}

	sDecodeTableInitialized = true;

exit:
	;
}

char *Base64Encode( const void * inData, size_t size, size_t *outEncodedSize )
{
	const uint8_t *data = (const uint8_t*)inData;
	char * result = NULL;
	size_t	neededLength, i, j;
	char * encodedData = NULL;

	neededLength = 4 * ( ( size + 2 ) / 3 );
	encodedData = malloc( neededLength + 1 );
	require( encodedData != NULL, exit );

	for ( i = 0, j = 0; i < size; )
	{
		uint32_t	a, b, c, t;

		a = i < size ? data[i] : 0;
		i++;
		b = i < size ? data[i] : 0;
		i++;
		c = i < size ? data[i] : 0;
		i++;

		t = ( a << 16 ) + ( b << 8 ) + c;
		encodedData[j] = sEncodeTable[ ( t >> 18 ) & 0x3F ];
		j++;
		encodedData[j] = sEncodeTable[ ( t >> 12 ) & 0x3F ];
		j++;
		encodedData[j] = sEncodeTable[ ( t >> 6 ) & 0x3F ];
		j++;
		encodedData[j] = sEncodeTable[ ( t ) & 0x3F ];
		j++;
    }

    for ( i = 0; i < sModTable[ size % 3 ]; i++ )
    {
        encodedData[ neededLength - 1 - i ] = '=';
	}
	encodedData[ neededLength ] = 0;

	result = encodedData;
	encodedData = NULL;
	*outEncodedSize = neededLength;

exit:

	ForgetMem( &encodedData );

    return result;
}

void* Base64Decode( const char *data, size_t *outDecodedSize )
{
	void * result = NULL;
	size_t len, decoded_len, i, j;
	uint8_t * decoded_data = NULL;

	len = strlen( data );
	require( ( len % 4 ) == 0, exit );

	decoded_len = ( len / 4 ) * 3;
	if ( ( len > 1 ) && ( data[ len - 1 ] == '=' ) ) decoded_len--;
	if ( ( len > 2 ) && ( data[ len - 2 ] == '=' ) ) decoded_len--;

	decoded_data = malloc( decoded_len );
	require( decoded_data != NULL, exit );

	BuildDecodeTable();

	for ( i = 0, j = 0; i < len; )
	{
		uint32_t	a, b, c, d, t;

		a = ( data[i] == '=' ) ? 0 : sDecodeTable[ (uint8_t)data[ i ] ];
		i++;
		b = ( data[i] == '=' ) ? 0 : sDecodeTable[ (uint8_t)data[ i ] ];
		i++;
		c = ( data[i] == '=' ) ? 0 : sDecodeTable[ (uint8_t)data[ i ] ];
		i++;
		d = ( data[i] == '=' ) ? 0 : sDecodeTable[ (uint8_t)data[ i ] ];
		i++;

		t = ( a << 18 ) + ( b << 12 ) + ( c << 6 ) + ( d << 0 );

		if ( j < decoded_len )
		{
			decoded_data[j] = ( t >> 16 ) & 0xFF;
			j++;
		}

		if ( j < decoded_len )
		{
			decoded_data[j] = ( t >> 8 ) & 0xFF;
			j++;
		}

		if ( j < decoded_len )
		{
			decoded_data[j] = ( t ) & 0xFF;
			j++;
		}
	}

	*outDecodedSize = decoded_len;
	result = decoded_data;
	decoded_data = NULL;

exit:

	ForgetMem( &decoded_data );

	return result;
}

#if TEST_B64
static void	TestB64Vector( const char *in, const char *expected )
{
	char * encodedData = NULL;
	size_t encodedSize;
	void * decodedData = NULL;
	size_t decodedSize;

	encodedData = Base64Encode( in, strlen( in ), &encodedSize );
	require( encodedData != NULL, exit );
	require( encodedSize == strlen( expected ), exit );
	require( strcmp( expected, encodedData ) == 0, exit );

	decodedData = Base64Decode( encodedData, &decodedSize );
	require( decodedData != NULL, exit );
	require( decodedSize == strlen( in ), exit );
	require( memcmp( decodedData, in, strlen( in ) ) == 0, exit );

exit:

	ForgetMem( &encodedData );
	ForgetMem( &decodedData );
}

void TestB64( void )
{
	// vectors from RFC4648
	TestB64Vector( "", "" );
	TestB64Vector( "f", "Zg==" );
	TestB64Vector( "fo", "Zm8=" );
	TestB64Vector( "foo", "Zm9v" );
	TestB64Vector( "foob", "Zm9vYg==" );
	TestB64Vector( "fooba", "Zm9vYmE=" );
	TestB64Vector( "foobar", "Zm9vYmFy" );
}
#endif



