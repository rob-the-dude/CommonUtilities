/*
 *	ArgumentUtilities.c
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

#include "ArgumentUtilities.h"

#include "CommonUtilities.h"
#include "DebugUtilities.h"

#include <string.h>
#include <stdlib.h>


int	FindOptionWithValue( int argc, const char *argv[], char option, FindOptionParameterType type, void * value )
{
	int result = -1;
	int i;
	
	require_quiet( argc > 0, exit );
	require_quiet( argv != NULL, exit );

	for ( i = 0; i < argc; i++ )
	{
		require( argv[i] != NULL, exit );
		
		if ( ( argv[i][0] != '-' ) || ( argv[i][1] != option ) || ( argv[i][2] != 0 ) )
			continue;
		
		i++;
		require( i < argc, exit );
		require( argv[i] != NULL, exit );
		
		switch ( type )
		{
			case kFIND_OPT_U16:
				*(uint16_t*)value = atoi( argv[i] );
				break;
				
			case kFIND_OPT_U32:
				*(uint32_t*)value = (uint32_t)strtoul( argv[i], NULL, 0 );
				break;
				
			case kFIND_OPT_U64:
				*(uint64_t*)value = strtoll( argv[i], NULL, 0 );
				break;

			case kFIND_OPT_STR_DUP:
				(*(char**)value) = strdup( argv[i] );
				break;
				
			case kFIND_OPT_STR_REF:
				(*(char**)value) = (char*)argv[i];
				break;

			case kFIND_OPT_INDEX:
				*(int*)value = i - 1;
				break;
				
			default:
				require( 0, exit );
				break;
		}
		
		result = 0;
		break;
	}

exit:

	return result;
}

int	FindOption( int argc, const char *argv[], char option )
{
	int result = -1;
	int i;

	require_quiet( argc > 0, exit );
	require_quiet( argv != NULL, exit );

	for ( i = 0; i < argc; i++ )
	{
		require( argv[i] != NULL, exit );

		if ( ( argv[i][0] != '-' ) || ( argv[i][1] != option ) || ( argv[i][2] != 0 ) )
			continue;

		result = 0;
		break;

	}

exit:

	return result;
}

int FindArgument( int argc, const char *argv[], const char *option, int *position )
{
	int result = -1;
	int i;
	
	require( argc > 0, exit );
	require( argv != NULL, exit );
	require( option != NULL, exit );
	
	for ( i = 0; i < argc; i++ )
	{
		require( argv[ i ] != NULL, exit );
		if ( strcmp( argv[i], option ) == 0 )
		{
			result = true;
			if ( position != NULL )
			{
				(*position) = i;
			}
			result = 0;
			break;
		}
	}

exit:

	return result;
}
