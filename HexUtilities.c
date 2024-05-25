/*
 *	HexUtilities.c
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

#include "HexUtilities.h"

#include "CommonUtilities.h"
#include "DebugUtilities.h"

#include <ctype.h>

static char		hex_encoding[] = {
	'0',
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'A',
	'B',
	'C',
	'D',
	'E',
	'F' };

int			HexEncodeByte( uint8_t val, char *bytes )
{
	bytes[0] = hex_encoding[ ( val >> 4 ) & 0x0F ];
	bytes[1] = hex_encoding[ ( val & 0x0F ) ];
	return 0;
}

char*		HexEncodeByteString( uint8_t val, char *bytes )
{
	bytes[0] = hex_encoding[ ( val >> 4 ) & 0x0F ];
	bytes[1] = hex_encoding[ ( val & 0x0F ) ];
	bytes[2] = 0;
	return bytes;
}

int			HexDecodeByte( const char bytes[2], uint8_t *val )
{
	int result = -1;
	uint8_t v;
	char b;

	require_quiet(
			( ( bytes[0] >= '0' ) && ( bytes[0] <= '9' ) ) ||
			( ( bytes[0] >= 'A' ) && ( bytes[0] <= 'F' ) ) ||
			( ( bytes[0] >= 'a' ) && ( bytes[0] <= 'f' ) ),
			exit );
	require_quiet(
			( ( bytes[1] >= '0' ) && ( bytes[1] <= '9' ) ) ||
			( ( bytes[1] >= 'A' ) && ( bytes[1] <= 'F' ) ) ||
			( ( bytes[1] >= 'a' ) && ( bytes[1] <= 'f' ) ),
			exit );

	b = tolower( (int)bytes[0] );
	v = ( ( b >= '0' ) && ( b <= '9' ) ) ? b - '0' : 0xA + ( b - 'a' );
	v <<= 4;
	b = tolower( (int)bytes[1] );
	v += ( ( b >= '0' ) && ( b <= '9' ) ) ? b - '0' : 0xA + ( b - 'a' );

	result = 0;
	*val = v;

exit:

	return result;
}

int			ParseHexUInt64( const char *str, uint64_t *val )
{
	int result = -1;
	uint64_t v = 0;
	uint8_t bits = 0;
	char ch;
	size_t i;
	
	require( true, exit );
	
	for ( i = 0; i < sizeof( uint64_t ) * 2; i++ )
	{
		if (
			( ( str[i] >= '0' ) && ( str[i] <= '9' ) ) ||
			( ( str[i] >= 'A' ) && ( str[i] <= 'F' ) ) ||
			( ( str[i] >= 'a' ) && ( str[i] <= 'f' ) )
			)
		{
			ch = tolower( (int)str[i] );
			
			bits = ( ( ch >= '0' ) && ( ch <= '9' ) ) ? ch - '0' : 0xA + ( ch - 'a' );
			
			v <<= 4;
			v |= bits;
		}
		else
		{
			break;
		}
	}
	
	*val = v;
	result = 0;
	
exit:

	return result;
}




char*		HexEncode( const void *bytes, size_t amount )
{
	size_t		i;
	uint8_t		v;
	char* result;
	char* pos;
	const uint8_t* inBytes = (const uint8_t*)bytes;

	result = (char*)malloc( amount * 2 + 1 );
	require( result != NULL, exit );

	pos = result;
	for ( i = 0; i < amount; i++ )
	{
		v = inBytes[i];
		pos[0] = hex_encoding[ ( v >> 4 ) & 0x0F ];
		pos[1] = hex_encoding[ ( v & 0x0F ) ];
		pos += 2;
	}
	pos[0] = 0;

exit:

	return result;
}

int	HexDecodeBuffer( const char* inString, size_t inStringLength, void *outvBuffer, size_t inMaxLength, size_t *outActualLength )
{
	int 	result = -1;
	size_t	i;
	size_t	actualLength;
	char	ch;
	uint8_t	val;
	require( ( inStringLength % 2 ) == 0, exit );
	require( ( inStringLength <= inMaxLength * 2 ), exit );

	uint8_t *outBuffer = (uint8_t*)outvBuffer;

	i = 0;
	actualLength = 0;
	while ( i < inStringLength )
	{
		ch = inString[0];
		require( ( ( ( ch >= '0' ) && ( ch <= '9' ) ) || ( ( ch >= 'A' ) && ( ch <= 'F' ) ) ), exit );

		val = ( ( ch >= '0' ) && ( ch <= '9' ) ) ? ch - '0' : 0x0A + ( ch - 'A' );
		val <<= 4;

		ch = inString[1];
		require( ( ( ( ch >= '0' ) && ( ch <= '9' ) ) || ( ( ch >= 'A' ) && ( ch <= 'F' ) ) ), exit );
		val |= ( ( ch >= '0' ) && ( ch <= '9' ) ) ? ch - '0' : 0x0A + ( ch - 'A' );

		i+= 2;
		inString += 2;
		outBuffer[0] = val;
		outBuffer++;
		actualLength++;
	}

	if ( outActualLength != NULL )
		*outActualLength = actualLength;
	result = 0;

exit:

	return result;
}

void*	HexDecode( const char* inString, size_t inStringLength, size_t *outActualLength )
{
	void * result = NULL;
	size_t 	maxLength;
	size_t	i;
	size_t	actualLength;
	char	ch;
	uint8_t	val;
	require( ( inStringLength % 2 ) == 0, exit );

	maxLength = inStringLength / 2;
	void* outvBuffer = malloc( maxLength );
	require( outvBuffer != NULL, exit );

	uint8_t* outBuffer = (uint8_t*)outvBuffer;

	i = 0;
	actualLength = 0;
	while ( i < inStringLength )
	{
		ch = inString[0];
		require( ( ( ( ch >= '0' ) && ( ch <= '9' ) ) || ( ( ch >= 'A' ) && ( ch <= 'F' ) ) ), exit );

		val = ( ( ch >= '0' ) && ( ch <= '9' ) ) ? ch - '0' : 0x0A + ( ch - 'A' );
		val <<= 4;

		ch = inString[1];
		require( ( ( ( ch >= '0' ) && ( ch <= '9' ) ) || ( ( ch >= 'A' ) && ( ch <= 'F' ) ) ), exit );
		val |= ( ( ch >= '0' ) && ( ch <= '9' ) ) ? ch - '0' : 0x0A + ( ch - 'A' );

		i+= 2;
		inString += 2;
		outBuffer[0] = val;
		outBuffer++;
		actualLength++;
	}

	if ( outActualLength != NULL )
		*outActualLength = actualLength;

	result = outvBuffer;
	outBuffer = NULL;

exit:

	return result;
}


#if INCLUDE_HEX_UTIL_UNIT_TESTS
#include <string.h>
void TestHexUtilities( void )
{
	int 	err;
	
	char	test_buffer[] = "F1E2D3C4B5A69788796A5B4C3D2E1F00FF0FF0";
	uint8_t	test_bytes[] = { 0xF1, 0xE2, 0xD3, 0xC4, 0xB5, 0xA6, 0x97, 0x88, 0x79, 0x6A, 0x5B, 0x4C, 0x3D, 0x2E, 0x1F, 0x00, 0xFF, 0x0F, 0xF0 };

	char	*buffer = NULL;
	uint8_t bytes[ sizeof( test_bytes ) ];
	size_t	actual_length;

	buffer = HexEncode( test_bytes, sizeof( test_bytes ) );
	check( buffer != NULL );

	check( strcmp( buffer, test_buffer ) == 0 );

	err = HexDecodeBuffer( buffer, strlen( buffer ), bytes, sizeof( bytes ), &actual_length );
	check( err == 0 );
	check( memcmp( bytes, test_bytes, sizeof( bytes ) ) == 0 );
	check( actual_length = sizeof( bytes ) );


	ForgetMem( &buffer );
}
#endif

