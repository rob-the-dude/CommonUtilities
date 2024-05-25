/*
 *	DebugUtilities.h
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

#ifndef __DEBUG_UTILITIES_H__
#define __DEBUG_UTILITIES_H__

#include "CommonUtilities.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#include <errno.h>

#define kDebugLevelMax			0xFFFF
#define kDebugLevelError		0x5000
#define kDebugLevelVerbose		0x3000
#define kDebugLevelTrace		0x2000
#define kDebugLevelChatty		0x1000


#if DEBUG

#ifdef __cplusplus
extern "C" {
#endif

extern int	gDebugLevel;

void dlog_set_level( int level );

void dlog_include_timestamps( int onOrOff );
void dlog_include_procname( const char *pname );
void dlog_set_file( FILE * f );
void dlog_set_fd( int fd );

int dlog( int level, const char *fmt, ... );
int dlog_out( bool addNL, const char * fmt, va_list args );
int dlog_print_strings( int level, const char *prefix, const char *suffix, char * const strings[], size_t count );

void dlog_dump_hex_options( int debugLevel, bool dupLineHandling, const char *label, const void * buffer, size_t len );

void dlog_dump_hex( int level, const void* buffer, size_t length );

#if defined(TARGET_OS_NONE) && TARGET_OS_NONE
	#define debug_fail( x )		do { ; } while(0)
#else
	void debug_fail( int print_it );
#endif

#if __APPLE__
	bool debug_running_in_debugger( void );
#else
	#define debug_running_in_debugger()		( false )
#endif

void dlog_debugger( const char *file, int line );

void debug_fatal( const char *file, int line, char * expr );

#ifdef __cplusplus
} // extern "C"
#endif


#else

#define	dlog_set_level( level )
#define dlog_set_file( file )

#define dlog_include_timestamps( timestamps )
#define dlog_include_procname( ignored )

#define dlog( level, ... )

#define dlog_dump_hex( level, addr, len )
#define dlog_dump_hex_options( level, dupHandling, indent, addr, len )

#define debug_fail( x )

#define debug_running_in_debugger()		0

#endif

// many of these things are defined by Mac OS X already -- suckiness


#ifdef check_compile_time
	#undef check_compile_time
#endif
#define check_compile_time( expr )					extern int compile_time_check[ (expr) ? 1 : -1 ];

#ifdef check_compile_time_code
	#undef check_compile_time_code
#endif
#define check_compile_time_code( expr )    			switch( 0 ) { case 0: case expr:; }

#if 0
#define check_compile_time( expr )			\
											#if ( ( expr ) ) \
												/* al good */ \
											#else \
												#error \
											#endif
#endif


#ifdef check
	#undef check
#endif

#if defined(TARGET_OS_NONE) && TARGET_OS_NONE
	//#define dbg_check( x )								if ( !(x) ) { dlog( kDebugLevelMax, "!fail (%s:%d)\n", __FILE__, __LINE__ ); }
	#define check( x )									if ( !(x) ) { dlog( kDebugLevelMax, "!fail (%s:%d)\n", __FILE__, __LINE__ ); debug_fail(0); }
	#define check_fatal( x )
#else
	//#define dbg_check( x )								if ( !(x) ) { dlog( kDebugLevelMax, "debug check failed(%s)\r\n\t%s, line: %d\r\n", # x, __FILE__, __LINE__ ); }
	#define check( x )									if ( !(x) ) { dlog( kDebugLevelMax, "debug check failed(%s)\r\n\t%s, line: %d\r\n", # x, __FILE__, __LINE__ ); debug_fail(0); }
	#define check_fatal( x )							if ( !(x) ) { debug_fatal( __FILE__, __LINE__, #x ); }
#endif

#define check_errno( x )							if ( !(x) ) { dlog( kDebugLevelMax, "debug check fail(%s)\r\n\t%s, line: %d\r\n\terrno = %d (%s)\n", # x, __FILE__, __LINE__, errno, strerror( errno ) ); }

#ifdef check_noerr
	#undef check_noerr
#endif
#define check_noerr( x )							do { check( x == 0 ); } while(0)

#ifdef require
	#undef require
#endif
#define require( x, label )							if ( !(x) ) { check(x); goto label; }

#ifdef require_quiet
	#undef require_quiet
#endif
#define require_quiet( x, label )					if ( !(x) ) { goto label; }

#ifdef require_action
	#undef require_action
#endif
#define require_action( x, label, action )			if ( !(x) ) { check(x); action; goto label; }

#ifdef require_action_quiet
	#undef require_action_quiet
#endif
#define require_action_quiet( x, label, action )	if ( !(x) ) { action; goto label; }

#ifdef require_noerr
	#undef require_noerr
#endif
#define require_noerr( err, label )					if ( err != 0 ) { dlog( kDebugLevelMax, "error: %d\r\n\t%s, line: %d\r\n", err, __FILE__, __LINE__ ); debug_fail( 0 ); goto label; }

#ifdef require_noerr_quiet
	#undef require_noerr_quiet
#endif
#define require_noerr_quiet( err, label )			if ( err != 0 ) { goto label; }

#ifdef require_continue
	#undef require_continue
#endif
#define require_continue( x )						if ( !(x) ) { check(x); continue; }

#ifdef require_continue_action
	#undef require_continue_action
#endif
#define require_continue_action( x, action )		if ( !(x) ) { check(x); action; continue; }

#ifdef require_continue_quiet
	#undef require_continue_quiet
#endif
#define require_continue_quiet( x )					if ( !(x) ) { continue; }

#ifdef require_continue_action_quiet
	#undef require_continue_action_quiet
#endif
#define require_continue_action_quiet( x, action )	if ( !(x) ) { action; continue; }


#ifdef require_break
	#undef require_break
#endif
#define require_break( x )							if ( !(x) ) { check(x); break; }

#ifdef require_break_action
	#undef require_break_action
#endif
#define require_break_action( x, action )		if ( !(x) ) { check(x); action; break; }

#ifdef require_break_quiet
	#undef require_break_quiet
#endif
#define require_break_quiet( x )					if ( !(x) ) { break; }

#ifdef require_break_action_quiet
	#undef require_break_action_quiet
#endif
#define require_break_action_quiet( x, action )	if ( !(x) ) { action; break; }




#endif /* __DEBUG_UTILITIES_H__ */
