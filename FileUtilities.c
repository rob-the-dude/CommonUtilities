/*
 *	FileUtilities.c
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

#include "FileUtilities.h"

#include "CommonUtilities.h"
#include "DebugUtilities.h"

#if ( TARGET_OS_FREERTOS && !TARGET_OS_FREERTOS_SIM ) || TARGET_OS_NONE

	// these are not supported on FreeRTOS or bare-metal builds

#else

#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>

#if TARGET_OS_NETBSD
	#include <uuid.h>
#else
	#include <uuid/uuid.h>
#endif

#if __APPLE__
#include <TargetConditionals.h>
#endif


#ifndef kMAX_FILE_SIZE_TO_READ
#define kMAX_FILE_SIZE_TO_READ		64*1024
#endif

int		CreateDirectoryRecursively( const char *path_to_dir, bool includeLastElement )
{
	int result = -1;
	int err;
	struct stat sb;
	char * path = strdup( path_to_dir );
	char * pos;


	mode_t old_mask = umask( ~(S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP ) );

	require( path != NULL, exit );

	if ( !includeLastElement )
	{
		pos = strrchr( path, '/' );
		require( pos != NULL, exit );
		*pos = 0;
	}

	// just in case it already exists
	err = stat( path, &sb );
	if ( ( err == 0 ) && ( ( sb.st_mode & S_IFDIR ) == S_IFDIR ) ) { result = 0; goto exit; }

	// allow creating directories in working directory...
#if 0
	require( path[0] == '/', exit );
#endif
	
	pos = &path[1];
	char * next_component = NULL;

	while ( true )
	{
		next_component = (char*)strchr( pos, '/' );
		if ( next_component != NULL )
		{
			next_component[0] = 0;
		}

		// found last one
		err = mkdir( path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP );
		if ( ( err != 0 ) && ( errno == EEXIST ) )
		{
			err = stat( path, &sb );
			require( err == 0, exit );
			
			require( ( ( sb.st_mode & S_IFDIR ) == S_IFDIR ), exit );
			err = 0;
		}
		require( err == 0, exit );

		if ( next_component != NULL )
		{
			next_component[0] = '/';
			pos = next_component;
			pos++;
		}
		else
		{
			result = 0;
			break;
		}
	}

exit:

	ForgetMem( &path );

	umask( old_mask );

	return result;
}


char*	ReadDataFromFile( const char *path, size_t *outFileSize )
{
	char * result = NULL;
	char * data = NULL;
	int fd = -1;
	struct stat sb;
	int err;
	ssize_t num;

	fd = open( path, O_RDONLY );
	require_action_quiet( fd >= 0, exit, dlog( kDebugLevelError, "ReadDataFromFile: %s (error = %d)\n", path, errno ) );

	err = fstat( fd, &sb );
	require_noerr( err, exit );

	require( sb.st_size <= kMAX_FILE_SIZE_TO_READ, exit );
	data = (char*)malloc( sb.st_size + 1 );
	require( data != NULL, exit );

	num = read( fd, data, sb.st_size );
	require( num == sb.st_size, exit );

	if ( outFileSize != NULL )
	{
		*outFileSize = num;
	}
	data[num] = 0;
	result = data;
	data = NULL;

exit:

	ForgetFD( &fd );
	ForgetMem( &data );

	return result;
}

int		WriteDataToFile( const char* path, const void * data, size_t len )
{
	int fd = -1;
	int err = -1;
	ssize_t n;

	mode_t old_mask = umask( ~(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) );

	fd = open( path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );
	require( fd >= 0, exit );

	n = write( fd, data, len );
	require( (size_t)n == len, exit );

	err = 0;

exit:

	ForgetFD( &fd );

	umask( old_mask );


	return err;
}

const char*		GetCurrentUserHomeDirectory( void )
{
	const char * result = NULL;

#if TARGET_OS_NETBSD || TARGET_OS_LINUX

	dlog( kDebugLevelMax, "GetCurrentUserHomeDirectory: returning '/root'\n" );

	result =  "/root";

	goto exit;

#elif TARGET_IPHONE_SIMULATOR

	const char *home_dir;
	home_dir = getenv( "COMMON_UTILS_HOMEDIR" );
	if ( home_dir == NULL )
	{
		dlog( kDebugLevelError, "WARNING: COMMON_UTILS_HOMEDIR should be set for iOS simulator builds\n" );
	}

	result = home_dir;

#elif TARGET_OS_MAC || TARGET_OS_EMBEDDED

	static char * cachedCurrentUserHomeDirectory = NULL;

	if ( cachedCurrentUserHomeDirectory != NULL )
	{
		result = cachedCurrentUserHomeDirectory;
		goto exit;
	}

	#if __APPLE__
		// chances are, we are running in the simulator here, and GENERALLY speaking, we don't want to use 'root',
		// even if we might need root privileges for some things...
		// so IF we are root, then we log this (once) and return the REAL user's home directory.  you can over-ride
		// this behavior by setting the "COMMON_UTILS_ALLOW_ROOT_HOMEDIR=YES" environment variable

		if ( getuid() == 0 )
		{
			const char * user = getenv( "USER" );

			if ( user != NULL )
			{
				if ( strcmp( user, "root" ) == 0 )
				{
					if ( getenv( "SUDO_USER" ) != NULL )
					{
						user = getenv( "SUDO_USER" );
					}
				}
				
				if ( strcmp( user, "root" ) != 0 )
				{
					struct passwd *pwd = getpwnam( user );
					require( pwd != NULL, exit );

					if ( pwd != NULL )
					{
						static bool warnFirstTime = true;

						if ( warnFirstTime )
						{
							dlog( kDebugLevelError, "Using '%s' instead of 'root' for home directory (see FileUtilities.c, line %d for more info)\n", user, __LINE__ );
							warnFirstTime = false;
						}

						cachedCurrentUserHomeDirectory = strdup( pwd->pw_dir );
						result = cachedCurrentUserHomeDirectory;
						goto exit;
					}
					else
					{
						dlog( kDebugLevelError, "Unexpected: getpwnam failed for 'USER/SUDO_USER' environment variable (see FileUtilities.c, line %d for more info)\n", __LINE__ );
					}
				}
			}
			else
			{
				dlog( kDebugLevelError, "Unexpected: 'USER' environment variable not set (see FileUtilities.c, line %d for more info)\n", __LINE__ );
			}
		}

	#endif

	struct passwd *pwd = getpwuid( getuid() );
	require( pwd != NULL, exit );
	cachedCurrentUserHomeDirectory = strdup( pwd->pw_dir );
	result = cachedCurrentUserHomeDirectory;

#elif TARGET_OS_FREERTOS

	#warning "GetCurrentUserHomeDirectory not implemented for FreeRTOS"
	dlog( kDebugLevelMax, "WARNING: GetCurrentUserHomeDirectory called on FreeRTOS" );

#else

	#error

#endif

exit:

	return result;
}

#if TARGET_OS_UNIXLIKE

#if TARGET_OS_LINUX

#define DIR_NAME_LEN(ddee, x)	\
	( ddee->d_name[x] == 0 )

#else

#define DIR_NAME_LEN(ddee, x)	\
	( ddee->d_namlen == x )

#endif

int	ForEachFileInDirectory( const char *pathToDirectory, ForEachFileInDirectory_Callback callback, void * callbackContext )
{
	int result = -1;
	DIR * d = NULL;
	struct dirent *de;
	bool cont;

	d = opendir( pathToDirectory );
	require( d != NULL, exit );

	while ( true )
	{
		errno = 0;
		de = readdir( d );
		require( ( de != NULL ) || ( errno == 0 ), exit );
		require_break_quiet( de != NULL );
		require_continue_quiet( !( ( DIR_NAME_LEN(de, 1) ) && ( de->d_name[0] == '.' ) ) );
		require_continue_quiet( !( ( DIR_NAME_LEN(de, 2) ) && ( de->d_name[0] == '.' ) && ( de->d_name[0] == '.' ) ) );

		cont = callback( callbackContext, pathToDirectory, de );
		require_break_quiet( cont );
	}

	result = 0;

exit:

	ForgetDIR( &d );

	return result;
}

#endif

#endif

