/*
 *	AsyncIO.h
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

#ifndef __ASYNC_NET_IO_H__
#define __ASYNC_NET_IO_H__

#include "CommonUtilities.h"

#include <stdbool.h>

#if TARGET_OS_UNIXLIKE
#include <unistd.h>
#include <sys/time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


typedef struct OpaqueAsyncIO *AsyncIO;

#define kAIO_NEW_CONNECTION			1
#define kAIO_CONNECTION_CLOSED		2

#define kAIO_READY_FOR_WRITE		3
#define kAIO_CONNECTED				kAIO_READY_FOR_WRITE
#define kAIO_DATA_AVAILABLE			4

#define kAIO_TIMER_FIRED			5

#define kAIO_PROCESS_EXITED			6	// fd in callback is the PID

#define kAIO_SIGNAL_DELIVERED		7	// fd in callback is actually the signal


typedef void ( *AsyncIOEvent )( int eventID, AsyncIO anio, int fd, void * userData );

#if __APPLE__
// for using AsyncIO inside a Cocoa or iPhone application -- but for command line utilities, we don't want to do this
#define kAsyncIOFlags_UseRunLoop		1
#endif
int 	AsyncIO_Initialize( int flags );



AsyncIO		AsyncIO_NewConnectionListener( int fd, AsyncIOEvent eventCallback, void * userData );
AsyncIO		AsyncIO_NewConnection( int fd, AsyncIOEvent eventCallback, void * userData );
int			AsyncIO_Release( AsyncIO aio, bool closeDescriptor );
int			AsyncIO_NotifyOnReadability( AsyncIO aio );
int			AsyncIO_NotifyOnWritability( AsyncIO aio );
#define 	AsyncIO_NotifyWhenConnected( aio )		AsyncIO_NotifyOnWritability( aio )

int 			AsyncIO_Run( bool keepRunning );

typedef struct OpaqueAsyncIOEventContext *AsyncIOEventsContext;
int				AsyncIO_WaitForEvents( AsyncIOEventsContext *outEventsContext, struct timeval *timeout );
int				AsyncIO_ProcessEvents( AsyncIOEventsContext outEventsContext );

#if TARGET_OS_UNIXLIKE
AsyncIO		AsyncIO_NewProcessMonitor( pid_t pid, AsyncIOEvent eventCallback, void * userData );
AsyncIO		AsyncIO_NewSignalMonitor( int signalID, AsyncIOEvent eventCallback, void * userData );
#endif

// we can do one to deliver timers
AsyncIO		AsyncIO_NewTimer( /*uint32_t milliseconds, */AsyncIOEvent eventCallback, void * userData );
int			AsyncIO_EnableTimer( AsyncIO timer, uint32_t milliseconds );
int			AsyncIO_DisableTimer( AsyncIO timer );


#define ForgetAsyncIO( a, c )		do { if ( (*a) != NULL ) { AsyncIO_Release( (*a), c ); (*a) = NULL; } } while(0)


#define kAIO_REDIRECT_INPUT_CLOSED_EVENT		1
#define kAIO_REDIRECT_INPUT_ERROR_EVENT			2
#define kAIO_REDIRECT_OUTPUT_ERROR_EVENT		3
#define kAIO_REDIRECT_DATA_READY				4
#define kAIO_REDIRECT_DATA_WRITTEN				5

typedef struct OpaqueAsyncIORedirect *AsyncRedirectIO;
typedef void (*AsyncRedirectIOEvent)( int eventID, AsyncRedirectIO redir, void * userData );

AsyncRedirectIO AsyncIO_Redirect( int fd_in, int fd_out, AsyncRedirectIOEvent callback, void * callback_data );
void AsyncIO_ReleaseRedirect( AsyncRedirectIO redir, bool closeIn, bool closeOut );

#define ForgetRedirectIO( a, cI, cO )		do { if ( (*a) != NULL ) { AsyncIO_ReleaseRedirect( (*a), cI, cO ); (*a) = NULL; } } while(0)


#ifdef __cplusplus
} // extern "C"
#endif

#endif

