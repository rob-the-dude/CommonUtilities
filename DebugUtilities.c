/*
 *	DebugUtilities.c
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

#define _GNU_SOURCE	1	// to pick up asprintf

#include "CommonUtilities.h"
#include "DebugUtilities.h"

#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#if DEBUG

#ifndef DEBUG_INCLUDE_TIME_STAMPS

	#define DEBUG_INCLUDE_TIME_STAMPS	1

#endif

#else

	#define DEBUG_INCLUDE_TIME_STAMPS	0

#endif


int	gDebugLevel = kDebugLevelError;
bool gDebugIncludeTimeStamps = DEBUG_INCLUDE_TIME_STAMPS;
#if TARGET_OS_UNIXLIKE
char * gDebugIncludeProcName = NULL;
#endif

FILE*	gDebugLogFile = NULL;

#if TARGET_OS_FREERTOS || TARGET_OS_FREERTOS_SIM

#include <FreeRTOS.h>
#include <semphr.h>

static SemaphoreHandle_t	debugLogLock = NULL;

#endif

#if TARGET_OS_ZEPHYR

K_MUTEX_DEFINE(debugLogLock);

#endif

static void dlog_init_internal( void )
{
#if TARGET_OS_FREERTOS || TARGET_OS_FREERTOS_SIM
	if ( debugLogLock == NULL )
	{
		debugLogLock = xSemaphoreCreateMutex();
	}
#endif

}

#if defined(TARGET_OS_NETBSD) && TARGET_OS_NETBSD
// may not be necessary any more? I think they fixed it based on my email to them
#include "SocketUtilities.h"
#endif

static void dlog_force_sync( int fd )
{
#if defined(TARGET_OS_NETBSD) && TARGET_OS_NETBSD
	// vdprintf doesn't work on non-blocking descriptors that aren't regular files?

	SetBlocking( fd, 1 );
#endif
}

void dlog_set_level( int level )
{
	gDebugLevel = level;

	dlog_init_internal();
}

static int dlog_internal( const char *procname, const char *timestamp, bool add_nl, const char * fmt, va_list args )
{
	int result = 0;
	
	require( true, exit );

#if defined(TARGET_OS_NETBSD) && TARGET_OS_NETBSD
	dlog_force_sync( fileno( gDebugLogFile ) );
#endif

#if TARGET_OS_FREERTOS
	require_quiet( debugLogLock != NULL, exit );
#endif

	if ( gDebugLogFile == NULL )
		gDebugLogFile = stderr;

#if TARGET_OS_FREERTOS
	bool semTaken = false;
	if ( debugLogLock )
	{
		xSemaphoreTake( debugLogLock, portMAX_DELAY );
		semTaken = true;
	}
#endif

#if TARGET_OS_ZEPHYR

	k_mutex_lock( &debugLogLock, K_FOREVER );

#endif

	result = fprintf( gDebugLogFile, "[%s]%s", procname != NULL ? procname : "", timestamp != NULL ? timestamp : "" );
	result += vfprintf( gDebugLogFile, fmt, args );
	if ( add_nl )
	{
		result += fprintf( gDebugLogFile, "\n" );
	}

#if TARGET_OS_FREERTOS
	if ( semTaken )
		xSemaphoreGive( debugLogLock );
#endif
#if TARGET_OS_ZEPHYR

	k_mutex_unlock( &debugLogLock );

#endif

#if defined(TARGET_OS_NONE) && TARGET_OS_NONE
	// don't fflush
#else
	fflush( gDebugLogFile );
#endif

exit:

	return result;
}

void dlog_set_file( FILE * f )
{
	// should we close existing gDebugLogFile?  or let user? by default it's set to stderr, so we probably don't want to close it ourselves

	gDebugLogFile = f;
}

int dlog_imp( int debugLevel, bool add_nl, const char *fmt, ... )
{
	int			result;
	va_list		args;
	const char 	*procname = NULL;
	const char	*timestamp = NULL;
	char 		timestamp_buffer[80];

	result = 0;
	require_quiet( debugLevel >= gDebugLevel, exit );

	if ( gDebugLogFile == NULL )
	{
		gDebugLogFile = stderr;
	}

#if TARGET_OS_UNIXLIKE

	if ( gDebugIncludeProcName != NULL )
	{
		procname = gDebugIncludeProcName;
	}

#endif

#if TARGET_OS_FREERTOS

	TaskHandle_t task = xTaskGetCurrentTaskHandle();
	if ( task != NULL )
	{
		procname = pcTaskGetName( task );
	}

#endif

#if TARGET_OS_ZEPHYR
	procname = k_thread_name_get( k_current_get() );
#endif

	if ( gDebugIncludeTimeStamps )
	{
		time_t 		now;
		struct tm 	*tm;

		time( &now );

		tm = localtime( &now );

		snprintf( timestamp_buffer, sizeof( timestamp_buffer ), " %04d-%02d-%02d %02d:%02d:%02d : ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec );
		timestamp = timestamp_buffer;
	}

	va_start( args, fmt );
	result = dlog_internal( procname, timestamp, add_nl, fmt, args );
	va_end( args );
	
exit:
	
	return result;
}

int dlog_print_strings( int debugLevel, const char *prefix, const char *suffix, char * const strings[], size_t count )
{
	size_t i;

	require_quiet( debugLevel >= gDebugLevel, exit );

	dlog_force_sync( fileno( gDebugLogFile ) );

	fprintf( gDebugLogFile, "%s", prefix == NULL ? "" : prefix );
	for ( i = 0; i < count; i++ )
	{
		fprintf( gDebugLogFile, "%s ", strings[i] == NULL ? "NULL" : strings[i] );
	}
	fprintf( gDebugLogFile, "%s", suffix == NULL ? "" : suffix );

exit:

	return 0;
}


static uint8_t line_buffer_hex_offsets[]	= { 1, 4, 7, 10, 13, 16, 19, 22 };
static uint8_t line_buffer_ascii_offsets[] 	= { 26, 27, 28, 29, 31, 32, 33, 34 };

static void dlog_dump_hex_internal_helper( const char *fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	dlog_internal( NULL, NULL, false, fmt, args );
	va_end( args );
}

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

//#define		dlog_byte_to_hex( v, c )	do { ((char*)c)[0] = hex_encoding[ ( v >> 4 ) & 0x0F ]; ((char*)c)[1] = hex_encoding[ v & 0x0F ]; } while(0)
static inline void dlog_byte_to_hex( uint8_t v, char *c )
{
	c[0] = hex_encoding[ ( v >> 4 ) & 0x0F ];
	c[1] = hex_encoding[ v & 0x0F ];
}

void dlog_dump_hex_options( int debugLevel, bool dupLineHandling, const char *indent, const void * buffer, size_t len )
{
	require_quiet( debugLevel >= gDebugLevel, exit );

	if ( gDebugLogFile == NULL )
	{
		gDebugLogFile = stderr;
	}

	int		duplicate_line_count = 0;
	char	line_buffer		[ sizeof( "\t12 34 56 78\t9A BC DE F0\t\tABCD EFGH" ) ];
	char	prev_line_buffer[ sizeof( line_buffer ) ];
	char	line_template[] =         "\t           \t           \t\t         ";
	size_t i;
	const uint8_t *buf = (const uint8_t*)buffer;

	prev_line_buffer[0] = 0;
	line_buffer[0] = 0;

	for ( i = 0; i < len; i++ )
	{
		int val = buf[i] & 0x000000FF;

		if ( ( i % 8 ) == 0 )
		{
			if ( i != 0 )
			{
				if ( dupLineHandling && ( strcmp( line_buffer, prev_line_buffer ) == 0 ) )
				{
					duplicate_line_count++;
				}
				else
				{
					if ( duplicate_line_count > 0 )
					{
						dlog_dump_hex_internal_helper( "\t... repeated %d times\r\n", duplicate_line_count );
						duplicate_line_count = 0;
					}
					dlog_dump_hex_internal_helper( "%s%s\r\n", indent, line_buffer );
					memcpy( prev_line_buffer, line_buffer, sizeof( line_buffer ) );
				}
			}
			memcpy( line_buffer, line_template, sizeof( line_template ) );
		}

		dlog_byte_to_hex( buf[i], &line_buffer[ line_buffer_hex_offsets[ i % 8 ] ] );
		line_buffer[ line_buffer_ascii_offsets[ i % 8 ] ] = isprint( val ) ? val : '.';
	}

	if ( duplicate_line_count > 0 )
	{
		dlog_dump_hex_internal_helper( "\t... repeated %d times\r\n", duplicate_line_count );
	}
	dlog_dump_hex_internal_helper( "%s%s\r\n", indent, line_buffer );

exit:
	;
}

void dlog_dump_hex( int debugLevel, const void * buffer, size_t len )
{
	dlog_dump_hex_options( debugLevel, true, "", buffer, len );
}


static void dlog_byte_to_hex_check( bool testCondition, uint8_t byte, char * pos )
{
	if ( testCondition )
		dlog_byte_to_hex( byte, pos );
	else
		pos[0] = 0;
}

#define kHex32Init	"                       "
uint8_t	hex32_offsets[] = { 0, 3, 6, 9, 12, 15, 18, 21 };

void dlog_dump_hex_simple( int debugLevel, const void * buffer, size_t len );
void dlog_dump_hex_simple( int debugLevel, const void * buffer, size_t len )
{
	char *out = NULL, *temp = NULL;
	const uint8_t *b = (const uint8_t*)buffer;
	char hex32[] = kHex32Init;

	require_quiet( len > 0, exit );

	asprintf( &out, "\t" );
	require( out != NULL, exit );

	while ( len >= 8 )
	{
		dlog_byte_to_hex( b[0], &hex32[hex32_offsets[0]] );
		dlog_byte_to_hex( b[1], &hex32[hex32_offsets[1]] );
		dlog_byte_to_hex( b[2], &hex32[hex32_offsets[2]] );
		dlog_byte_to_hex( b[3], &hex32[hex32_offsets[3]] );
		dlog_byte_to_hex( b[4], &hex32[hex32_offsets[4]] );
		dlog_byte_to_hex( b[5], &hex32[hex32_offsets[5]] );
		dlog_byte_to_hex( b[6], &hex32[hex32_offsets[6]] );
		dlog_byte_to_hex( b[7], &hex32[hex32_offsets[7]] );

		asprintf( &temp, "%s%s ", out, hex32 );
		require( temp != NULL, exit );
		ForgetMem( &out );
		out = temp;
		temp = NULL;

		b += 8;
		len -= 8;
	}

	dlog_byte_to_hex_check( ( len > 0 ), b[0], &hex32[hex32_offsets[0]] );
	dlog_byte_to_hex_check( ( len > 1 ), b[1], &hex32[hex32_offsets[1]] );
	dlog_byte_to_hex_check( ( len > 2 ), b[2], &hex32[hex32_offsets[2]] );
	dlog_byte_to_hex_check( ( len > 3 ), b[3], &hex32[hex32_offsets[3]] );
	dlog_byte_to_hex_check( ( len > 4 ), b[4], &hex32[hex32_offsets[4]] );
	dlog_byte_to_hex_check( ( len > 5 ), b[5], &hex32[hex32_offsets[5]] );
	dlog_byte_to_hex_check( ( len > 6 ), b[6], &hex32[hex32_offsets[6]] );
	dlog_byte_to_hex_check( ( len > 7 ), b[7], &hex32[hex32_offsets[7]] );

	asprintf( &temp, "%s%s", out, hex32 );
	require( temp != NULL, exit );
	ForgetMem( &out );
	out = temp;
	temp = NULL;

	dlog( debugLevel, "%s\n", out );

exit:

	ForgetMem( &out );
}


#if defined(TARGET_OS_NONE) && TARGET_OS_NONE
	// not supported...
#else
int gDebugDropIntoDebugger = 0;

void debug_fail( int print_it )
{
	if ( print_it )
		dlog( kDebugLevelError, "debug_fail:\n" );

	if ( gDebugDropIntoDebugger )
	{
		dlog_debugger( __FILE__, __LINE__ );
	}
}

void debug_fatal( const char *file, int line, char * failedexpr )
{
	dlog( kDebugLevelMax, "debug_fatal:\n"
	 	"\t%s:d\n"
	 	"\t%s\n",
	 	file != NULL ? file : "(NULL - no file specified)\n",
	 	line,
	 	failedexpr != NULL ? failedexpr : "(NULL - no expr provided)\n"
	 	);

	while( 1 )
	{
#if TARGET_OS_FREERTOS
		vTaskDelay( 1000 );
#elif TARGET_OS_ZEPHYR
		k_msleep( 1000 );
#elif TARGET_OS_UNIXLIKE
		sleep( 1 );
#else
		;
#endif
	}
}
#endif

void dlog_include_timestamps( int onOrOff )
{
	gDebugIncludeTimeStamps = onOrOff;
}

void dlog_include_procname( const char *procName )
{
#if TARGET_OS_UNIXLIKE
	if ( procName != NULL )
	{
		gDebugIncludeProcName = strdup( procName );
	}
#else
	(void)procName;
#endif
}

#if __APPLE__
#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>

static bool AmIBeingDebugged(void)
    // Returns true if the current process is being debugged (either
    // running under the debugger or has a debugger attached post facto).
{
    int                 junk;
    int                 mib[4];
    struct kinfo_proc   info;
    size_t              size;

    // Initialize the flags so that, if sysctl fails for some bizarre
    // reason, we get a predictable result.

    info.kp_proc.p_flag = 0;

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    // Call sysctl.

    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    assert(junk == 0);

    // We're being debugged if the P_TRACED flag is set.

    return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
}
#endif


#if __APPLE__

bool debug_running_in_debugger( void )
{
	if ( AmIBeingDebugged() )
		return true;

	return false;
}

#else

	//TO DO: figure out if we're running in debugger on other platforms
	//	actually, this may not be useful.  everywhere else, the 'debug' output
	//	it a serial port.  we MIGHT figure out how to send debug output
	//	to the segger JLink at some point, but honestly, UART seems
	//	better.  so I don't know if we even need this routine on other platforms.
	//	perhaps it would be better to #define it to (false) ?

#endif

void dlog_debugger( const char *file, int line )
{
	const char * pos;
	char	dbg_loc[64];

	pos = strrchr( file, '/' );
	if ( pos != NULL )
	{
		pos++;
	}
	else
	{
		pos = file;
	}

	snprintf( dbg_loc, sizeof( dbg_loc ), "!DBGR: %d, %s\n", line, pos );
	dbg_loc[63] = 0;

#if ( ( __ARM_ARCH == 7 ) && ( __ARM_ARCH_PROFILE == 'M' ) )
	while(1)
	{
	#if DEBUG_CONFIG_ENABLE_ARM_BREAKPOINT
		__asm("bkpt #0");
	#else
		;
	#endif
	}
#else

	while ( 1 )
	{
	#if TARGET_OS_UNIXLIKE
		sleep( 1 );
	#else
			;
	#endif
	}

#endif
}

#if TARGET_OS_FREERTOS || ( defined(TARGET_OS_NONE) && TARGET_OS_NONE )
void dlog_debugger_string( const char * msg )
{
	dlog( kDebugLevelMax, "%s", msg );
	dlog_debugger( __FILE__, __LINE__ );
}


void __assert_func (const char *file,
  int line,
  const char *func,
  const char *failedexpr)
{
	dlog( kDebugLevelMax, "assertion failed:\n"
	 	"\t%s\n"
	 	"\t%d\n"
	 	"\t%s\n"
	 	"\t%s\n",
	 	file != NULL ? file : "(NULL - no file specified)\n",
	 	line,
	 	func != NULL ? func : "(NULL - no function specified)\n",
	 	failedexpr != NULL ? failedexpr : "(NULL - no expr provided)\n"
	 	);

	while( 1 )
	{
#if TARGET_OS_FREERTOS
		vTaskDelay( 1000 );
#elif TARGET_OS_UNIXLIKE
		sleep( 1 );
#else
		;
#endif

	}
}
#endif
