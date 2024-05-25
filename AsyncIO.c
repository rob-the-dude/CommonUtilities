/*
 *	AsyncIO.c
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

#include "AsyncIO.h"

#include "CommonUtilities.h"
#include "DebugUtilities.h"
#include "TimeUtilities.h"

#if !TARGET_OS_NONE

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#if TARGET_OS_FREERTOS
	#include <sys/time.h>
	#include <lwip/sockets.h>
	#define ASYNC_NETIO_USE_SELECT		1
	#include <FreeRTOS.h>
	#include <task.h>
#else
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <sys/event.h>
	#include <sys/time.h>
	#include <signal.h>
	#define ASYNC_NETIO_USE_KQUEUE		1
#endif

// we may only want to do this selectively, even on Apple --
//	it's needed to use AsyncIO inside a Cocoa app, which uses it's own runloop
#if __APPLE__
#if !TARGET_OS_FREERTOS
#define ASYNC_NETIO_USE_RUN_LOOP		1
#endif
#endif

#if ASYNC_NETIO_USE_RUN_LOOP
#include <CoreFoundation/CoreFoundation.h>
#endif



#define kAIO_TYPE_LISTENER			1
#define kAIO_TYPE_CONNECTION		2
#define kAIO_TYPE_TIMER				3		// not on RTOS (yet?)
#define kAIO_TYPE_PROC				4		// process monitor
#define kAIO_TYPE_SIGNAL			5		// signal monitor

struct OpaqueAsyncIO
{
	int 						fd;
	int							type;
	AsyncIOEvent				callback;
	void*						userdata;

	// mostly for debugging and cleanup
	bool						notifyOnRead;
	bool						notifyOnWrite;

#if ASYNC_NETIO_USE_SELECT
	uint32_t					next_fire_time;

	struct OpaqueAsyncIO		*next_timer;
#endif
};

AsyncIO anioInProgress = NULL;

#if ASYNC_NETIO_USE_KQUEUE
int	anioKQ = -1;
#endif

#if ASYNC_NETIO_USE_SELECT
fd_set	anioReadSet;
fd_set	anioWriteSet;
AsyncIO enabled_timers = NULL;
AsyncIO aio_next_enabled_timer = NULL;
#endif

#if ASYNC_NETIO_USE_RUN_LOOP
CFFileDescriptorRef anioDescriptorRef = NULL;
CFRunLoopSourceRef anioSourceRef = NULL;

static void AsyncIO_PrimeRunLoop( void );

#define ASYNC_NETIO_PRIME_RUN_LOOP()	\
	do { if ( anioDescriptorRef != NULL ) { AsyncIO_PrimeRunLoop(); } } while(0)

#else

#define ASYNC_NETIO_PRIME_RUN_LOOP()

#endif


int	AsyncIO_Release( AsyncIO	anio, bool closeDescriptor)
{
	int result = -1;
	
	if ( anio == NULL )
	{
		dlog( kDebugLevelError, "AsyncIO_Release: NULL aio?\n" );
	}

	require( anio != NULL, exit );

	if ( anio->type == kAIO_TYPE_TIMER )
	{
//		dlog( kDebugLevelMax, "releasing asyncio timer: %p\n", anio );
	
		AsyncIO_DisableTimer( anio );
	}

#if ASYNC_NETIO_USE_KQUEUE
	int err;
#ifdef EV_SET64
	struct kevent64_s	kv;
#else
	struct kevent	kv;
#endif
	if ( anio->notifyOnRead )
	{
#ifdef EV_SET64
		EV_SET64( &kv, anio->fd, EVFILT_READ, EV_DELETE, 0, 0, (uint64_t)anio, 0, 0 );
		err = kevent64( anioKQ, &kv, 1, NULL, 0, 0, NULL );
#else
		EV_SET( &kv, anio->fd, EVFILT_READ, EV_DELETE, 0, 0, anio );
		err = kevent( anioKQ, &kv, 1, NULL, 0, NULL );
#endif
		if ( err != 0 ) { dlog( kDebugLevelError, "AsyncIO_Release: error removing READ filter (%d)\n", errno); }
	}

	if ( anio->notifyOnWrite )
	{
#ifdef EV_SET64
		EV_SET64( &kv, anio->fd, EVFILT_WRITE, EV_DELETE, 0, 0, (uint64_t)anio, 0, 0 );
		err = kevent64( anioKQ, &kv, 1, NULL, 0, 0, NULL );
#else
		EV_SET( &kv, anio->fd, EVFILT_WRITE, EV_DELETE, 0, 0, anio );
		err = kevent( anioKQ, &kv, 1, NULL, 0, NULL );
#endif
		if ( err != 0 ) { dlog( kDebugLevelError, "AsyncIO_Release: error removing WRITE filter (%d)\n", errno); }
	}
#endif

#if ASYNC_NETIO_USE_SELECT
	FD_CLR( anio->fd, &anioReadSet );
	FD_CLR( anio->fd, &anioWriteSet );
#endif

	if ( closeDescriptor )
	{
		// we close the descriptor and that will remove all events
		ForgetFD( &anio->fd );
	}
	
	// make sure any loop that's in progress doesn't muck with this item
	if ( anioInProgress == anio )
	{
		anioInProgress = NULL;
	}

	ForgetMem( &anio );

	result = 0;

exit:

	return result;
}

AsyncIO		AsyncIO_NewConnectionListener( int fd, AsyncIOEvent eventCallback, void * userData )
{
	struct OpaqueAsyncIO *anio = NULL, *result = NULL;

	anio = calloc( 1, sizeof( struct OpaqueAsyncIO ) );
	require( anio != NULL, exit );

	anio->fd = fd;
	anio->type = kAIO_TYPE_LISTENER;
	anio->callback = eventCallback;
	anio->userdata = userData;

	// make sure socket is in non-blocking mode
	int flags, err;
	flags = fcntl( fd, F_GETFL, 0 );
	require( flags != -1, exit );

	flags |= O_NONBLOCK;
	err = fcntl( fd, F_SETFL, flags );
	require( err == 0, exit );

	// add it to our kqueue
#if ASYNC_NETIO_USE_KQUEUE

#ifdef EV_SET64

	struct kevent64_s	kv;
	EV_SET64( &kv, fd, EVFILT_READ, EV_ADD, 0, 0, (uint64_t)anio, 0, 0 );
	err = kevent64( anioKQ, &kv, 1, NULL, 0, 0, NULL );

#else

	struct kevent	kv;
	EV_SET( &kv, fd, EVFILT_READ, EV_ADD, 0, 0, anio );
	err = kevent( anioKQ, &kv, 1, NULL, 0, NULL );

#endif

	require( err == 0, exit );
#endif
#if ASYNC_NETIO_USE_SELECT
	lwip_socket_set_userdata( fd, anio );
	FD_SET( fd, &anioReadSet );
#endif

	ASYNC_NETIO_PRIME_RUN_LOOP();
	result = anio;
	anio = NULL;

exit:

	ForgetMem( &anio );

	return result;
}

AsyncIO		AsyncIO_NewTimer( AsyncIOEvent eventCallback, void * userData )
{
	struct OpaqueAsyncIO *anio = NULL, *result = NULL;

	anio = calloc( 1, sizeof( struct OpaqueAsyncIO ) );
	require( anio != NULL, exit );

	anio->fd = -1;
	anio->type = kAIO_TYPE_TIMER;
	anio->callback = eventCallback;
	anio->userdata = userData;

	result = anio;
	anio = NULL;

exit:

	ForgetMem( &anio );

	return result;
}

#if ASYNC_NETIO_USE_SELECT
static void	AsyncIO_GetNextTimerFireTime( struct timeval *tv, AsyncIO *out_next_timer )
{
	uint32_t next_fire_time = 0xFFFFFFFF;
	uint32_t now = MillisecondCounter();
	AsyncIO next_timer = NULL;
	AsyncIO tio = enabled_timers;
	while ( tio != NULL )
	{
		if ( tio->next_fire_time < next_fire_time )
		{
			next_timer = tio;
			next_fire_time = tio->next_fire_time;
		}
		
		tio = tio->next_timer;
	}
	
	if ( next_timer != NULL )
	{
		// NOTE: it's possible that a timer's 'next fire time' has already passed, and remaining will be < now
		//		deal with that case here
		if ( now > next_fire_time )
		{
			tv->tv_sec = 0;
			tv->tv_usec = 0;
		}
		else
		{
			uint32_t remaining = next_fire_time - now;
			tv->tv_sec = remaining / 1000;
			tv->tv_usec = ( remaining % 1000 ) * 1000;
		}

		*out_next_timer = next_timer;
	}
}
#endif

int				AsyncIO_EnableTimer( AsyncIO timer, uint32_t milliseconds )
{
	int result = -1;

	require( timer != NULL, exit );

#if ASYNC_NETIO_USE_KQUEUE
	int err;

#ifdef EV_SET64
	struct kevent64_s	kv;
	EV_SET64( &kv, (uint64_t)timer, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, milliseconds, (uint64_t)timer, 0, 0 );
	err = kevent64( anioKQ, &kv, 1, NULL, 0, 0, NULL );
#else
	struct kevent	kv;
	EV_SET( &kv, timer, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, milliseconds, timer );
	err = kevent( anioKQ, &kv, 1, NULL, 0, NULL );
#endif
	require_action_quiet( err == 0, exit, dlog( kDebugLevelError, "AsyncIO_EnableTimer( %p, %u): kevent failure (errno = %d)\n", timer, (unsigned int)milliseconds, errno ) );

	ASYNC_NETIO_PRIME_RUN_LOOP();
#endif

#if ASYNC_NETIO_USE_SELECT
	timer->next_fire_time = MillisecondCounter() + milliseconds;
	
	timer->next_timer = enabled_timers;
	enabled_timers = timer;
#endif

	result = 0;

exit:

	return result;
}

int		AsyncIO_DisableTimer( AsyncIO timer )
{
	int result = -1;

#if ASYNC_NETIO_USE_KQUEUE
	int err;
#ifdef EV_SET64
	struct kevent64_s	kv;
	EV_SET64( &kv, (uint64_t)timer, EVFILT_TIMER, EV_DELETE, 0, 0, (uint64_t)timer, 0, 0 );
	err = kevent64( anioKQ, &kv, 1, NULL, 0, 0, NULL );
#else
	struct kevent	kv;
	EV_SET( &kv, timer, EVFILT_TIMER, EV_DELETE, 0, 0, timer );
	err = kevent( anioKQ, &kv, 1, NULL, 0, NULL );
#endif
	require( ( err == 0 ) || ( errno == ENOENT ), exit );

	ASYNC_NETIO_PRIME_RUN_LOOP();
#endif

#if ASYNC_NETIO_USE_SELECT

	if ( timer == aio_next_enabled_timer )
	{
		aio_next_enabled_timer = NULL;
	}

	AsyncIO tio = enabled_timers;
	require( tio != NULL, exit );
	
	if ( tio == timer )
	{
		enabled_timers = tio->next_timer;
	}
	else
	{
		while ( true )
		{
			if ( tio->next_timer == timer )
			{
				tio->next_timer = timer->next_timer;
				break;
			}
			
			tio = tio->next_timer;
			require( tio != NULL, exit );
		}
	}
	
	// housekeeping (not really necessary)
	timer->next_timer = NULL;
#endif

	result = 0;

exit:

	return result;
}

#if ASYNC_NETIO_USE_KQUEUE
AsyncIO		AsyncIO_NewProcessMonitor( pid_t pid, AsyncIOEvent eventCallback, void * userData )
{
	struct OpaqueAsyncIO *anio = NULL, *result = NULL;

	anio = calloc( 1, sizeof( struct OpaqueAsyncIO ) );
	require( anio != NULL, exit );

	anio->fd = -1;
	anio->type = kAIO_TYPE_PROC;
	anio->callback = eventCallback;
	anio->userdata = userData;

	int err;

#ifdef EV_SET64
	struct kevent64_s	kv;
	EV_SET64( &kv, (uint64_t)pid, EVFILT_PROC, EV_ADD | EV_ENABLE | EV_ONESHOT, NOTE_EXIT, 0, (uint64_t)anio, 0, 0 );
	err = kevent64( anioKQ, &kv, 1, NULL, 0, 0, NULL );
#else
	struct kevent	kv;
	EV_SET( &kv, pid, EVFILT_PROC, EV_ADD | EV_ENABLE | EV_ONESHOT, NOTE_EXIT, 0, anio );
	err = kevent( anioKQ, &kv, 1, NULL, 0, NULL );
#endif
	if ( err != 0 )
	{
		dlog( kDebugLevelError, "kevent error %d\n", errno );
	}
	require( err == 0, exit );

	ASYNC_NETIO_PRIME_RUN_LOOP();
	result = anio;
	anio = NULL;

exit:

	ForgetMem( &anio );

	return result;
}

AsyncIO		AsyncIO_NewSignalMonitor( int sig_id, AsyncIOEvent eventCallback, void * userData )
{
	struct OpaqueAsyncIO *anio = NULL, *result = NULL;

	anio = calloc( 1, sizeof( struct OpaqueAsyncIO ) );
	require( anio != NULL, exit );

	anio->fd = -1;
	anio->type = kAIO_TYPE_SIGNAL;
	anio->callback = eventCallback;
	anio->userdata = userData;

	int err;

	// must set signal to be ignored
	signal( sig_id, SIG_IGN );

#ifdef EV_SET64
	struct kevent64_s	kv;
	EV_SET64( &kv, sig_id, EVFILT_SIGNAL, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, (uint64_t)anio, 0, 0 );
	err = kevent64( anioKQ, &kv, 1, NULL, 0, 0, NULL );
#else
	struct kevent	kv;
	EV_SET( &kv, sig_id, EVFILT_SIGNAL, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, anio );
	err = kevent( anioKQ, &kv, 1, NULL, 0, NULL );
#endif
	if ( err != 0 )
	{
		dlog( kDebugLevelError, "kevent error %d\n", errno );
	}
	require( err == 0, exit );

	ASYNC_NETIO_PRIME_RUN_LOOP();
	result = anio;
	anio = NULL;

exit:

	ForgetMem( &anio );

	return result;
}
#endif


AsyncIO		AsyncIO_NewConnection( int fd, AsyncIOEvent eventCallback, void * userData )
{
	struct OpaqueAsyncIO *anio = NULL, *result = NULL;

	anio = calloc( 1, sizeof( struct OpaqueAsyncIO ) );
	require( anio != NULL, exit );

	anio->fd = fd;
	anio->type = kAIO_TYPE_CONNECTION;
	anio->callback = eventCallback;
	anio->userdata = userData;

	// make sure socket is in non-blocking mode
	int flags, err;
	flags = fcntl( fd, F_GETFL, 0 );
	require( flags != -1, exit );

	flags |= O_NONBLOCK;
//#if !TARGET_OS_FREERTOS
//	flags |= O_ASYNC;
//#endif
	err = fcntl( fd, F_SETFL, flags );
	if ( err != 0 ) { dlog( kDebugLevelError, "AsyncIO: fcntl( %d, F_SETFL, 0x%08X): error = %d\r\n", fd, flags, errno ); }
	if ( ( err != 0 ) && ( errno == ENOTTY ) ) { err = 0; }
	require( err == 0, exit );

#if ASYNC_NETIO_USE_SELECT
	lwip_socket_set_userdata( fd, anio );
#endif

	result = anio;
	anio = NULL;

exit:

	ForgetMem( &anio );

	return result;
}

int				AsyncIO_NotifyOnReadability( AsyncIO anio )
{
	int result = -1;

	int err;
#if ASYNC_NETIO_USE_KQUEUE
	require( anioKQ >= 0, exit );

#ifdef EV_SET64

	struct kevent64_s	kv;
	EV_SET64( &kv, anio->fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, (uint64_t)anio, 0, 0 );
	err = kevent64( anioKQ, &kv, 1, NULL, 0, 0, NULL );

#else

	struct kevent	kv;
	EV_SET( &kv, anio->fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, anio );
	err = kevent( anioKQ, &kv, 1, NULL, 0, NULL );

#endif

	if ( err != 0 )
	{
		dlog( kDebugLevelError, "kevent: error %d\n", errno );
	}
	require_quiet( err == 0, exit );
#endif
#if ASYNC_NETIO_USE_SELECT
	err = lwip_socket_set_userdata( anio->fd, anio );
	require( err == 0, exit );
	FD_SET( anio->fd, &anioReadSet );
#endif

	ASYNC_NETIO_PRIME_RUN_LOOP();
	anio->notifyOnRead = true;
	result = 0;

exit:

	return result;
}

int				AsyncIO_NotifyOnWritability( AsyncIO anio )
{
	int result = -1;

	int err;
#if ASYNC_NETIO_USE_KQUEUE

	require( anioKQ >= 0, exit );

#ifdef EV_SET64

	struct kevent64_s	kv;
	EV_SET64( &kv, anio->fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, (uint64_t)anio, 0, 0 );
	err = kevent64( anioKQ, &kv, 1, NULL, 0, 0, NULL );

#else

	struct kevent	kv;
	EV_SET( &kv, anio->fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, anio );
	err = kevent( anioKQ, &kv, 1, NULL, 0, NULL );

#endif
	require( err == 0, exit );
#endif
#if ASYNC_NETIO_USE_SELECT
	err = lwip_socket_set_userdata( anio->fd, anio );
	require( err == 0, exit );
	FD_SET( anio->fd, &anioWriteSet );
#endif

	ASYNC_NETIO_PRIME_RUN_LOOP();
	anio->notifyOnWrite = true;
	result = 0;

exit:

	return result;
}

#if ASYNC_NETIO_USE_RUN_LOOP

static void AsyncIO_PrimeRunLoop( void )
{
	CFFileDescriptorEnableCallBacks( anioDescriptorRef, kCFFileDescriptorReadCallBack );
}

static void AsyncIO_CFFileDescriptorCallBack( CFFileDescriptorRef f, CFOptionFlags callBackTypes, void *info )
{
	//dlog( kDebugLevelChatty, "AsyncIO_CFFileDescriptorCallBack: 0x%08lX\n", callBackTypes );
	require( f == anioDescriptorRef, exit );
	require( info == NULL, exit );
	require( callBackTypes == kCFFileDescriptorReadCallBack, exit );

	int err;
	err = AsyncIO_Run( 0 );
	require( err == 0, exit );
	
	// re-enable read notification
	CFFileDescriptorEnableCallBacks( anioDescriptorRef, kCFFileDescriptorReadCallBack );
	
exit:
	;
}
#endif



int AsyncIO_Initialize( int flags )
{
	int result = -1;

	require( 1, exit );

#if __APPLE__
	int enableRunLoop = 0;
	
	if ( ( flags & kAsyncIOFlags_UseRunLoop ) != 0 )
		enableRunLoop = 1;
#else
	(void)flags;
#endif

#if ASYNC_NETIO_USE_SELECT
	FD_ZERO( &anioReadSet );
	FD_ZERO( &anioWriteSet );
#endif

#if ASYNC_NETIO_USE_KQUEUE

	anioKQ = kqueue();
	require( anioKQ >= 0, exit );

#if ASYNC_NETIO_USE_RUN_LOOP

	if ( enableRunLoop )
	{
		anioDescriptorRef = CFFileDescriptorCreate( kCFAllocatorDefault,
			anioKQ,
			false,
			AsyncIO_CFFileDescriptorCallBack,
			NULL );
		require( anioDescriptorRef != NULL, exit );

		CFFileDescriptorEnableCallBacks( anioDescriptorRef, kCFFileDescriptorReadCallBack );

		// hopefully this doesn't fail...
		anioSourceRef = CFFileDescriptorCreateRunLoopSource( kCFAllocatorDefault, anioDescriptorRef, 0 );
		require( anioSourceRef != NULL, exit );

		CFRunLoopAddSource( CFRunLoopGetMain(), anioSourceRef, kCFRunLoopDefaultMode );

		CFRelease( anioSourceRef );
		
	}

#endif

#endif

	result = 0;

exit:

	return result;
}


#warning "FIX ME: thrash detection is busted"
// not sure what's wrong, but it's detecting stuff incorrectly (not surprised -- just haven't debugged)
#if THRASH_DETECTION
size_t	runs = 0;
bool	tvStartTimeValid = false;
struct	timeval tvStart;
struct	timeval	tvEnd;
#endif

typedef struct OpaqueAsyncIOEventContext
{
	int num;
#if ASYNC_NETIO_USE_SELECT
	int maxFd;
	fd_set	readfds;
	fd_set	writefds;
#endif

#if ASYNC_NETIO_USE_KQUEUE

//#define kMaxAsyncIOEvents		1
#define kMaxAsyncIOEvents		16

#ifdef EV_SET64
	struct kevent64_s	kv[ kMaxAsyncIOEvents ];
#else
	struct kevent		kv[ kMaxAsyncIOEvents ];
#endif
#endif

} OpaqueAsyncIOEventContext;


OpaqueAsyncIOEventContext gAsyncIOContext;

int				AsyncIO_WaitForEvents( AsyncIOEventsContext *outEventsContext, struct timeval *timeout )
{
	int result = -1;
	
	require( outEventsContext != NULL, exit );
	
	AsyncIOEventsContext ctx = &gAsyncIOContext;
	*outEventsContext = ctx;


#if ASYNC_NETIO_USE_SELECT

	// take a look at <https://www.freertos.org/Pend-on-multiple-rtos-objects.html>
	// see if it's possible to have other objects we could wait on, inside the lwip select (we might need to have it call back to us)

#warning "FIX ME: select case never posts kAIO_CONNECTION_CLOSED events for read"
	// we don't handle the EV_EOF case

	int i;
	//struct timeval timeout;
	struct timeval *to;

	FD_ZERO( &ctx->readfds );
	FD_ZERO( &ctx->writefds );
	ctx->maxFd = -1;
	for ( i = 0; i < LWIP_SELECT_MAXNFDS; i++ )
	{
		if ( FD_ISSET( i, &anioReadSet ) )
		{
			ctx->maxFd = i;
			FD_SET( i, &ctx->readfds );
		}
		if ( FD_ISSET( i, &anioWriteSet ) )
		{
			ctx->maxFd = i;
			FD_SET( i, &ctx->writefds );
		}
	}

	// if there's nothing to monitor, we can't do anything here
	//require( maxFd != -1, exit );
	
	struct timeval aio_timeout;
	aio_next_enabled_timer = NULL;
	AsyncIO_GetNextTimerFireTime( &aio_timeout, &aio_next_enabled_timer );
	if ( aio_next_enabled_timer != NULL )
	{
		to = &aio_timeout;
		if ( timeout != NULL )
		{
			if (
				( aio_timeout.tv_sec > timeout->tv_sec ) ||
				( ( aio_timeout.tv_sec == timeout->tv_sec ) && ( aio_timeout.tv_nsec > timeout->tv_nsec ) )
				)
			{
				to = timeout;
			}
		}
	}

	require( ( ctx->maxFd >= 0 ) || ( to != NULL ), exit );

	if ( ctx->maxFd == -1 )
	{
		ctx->num = select( 0, NULL, NULL, NULL, to );
	}
	else
	{
		ctx->num = select( ctx->maxFd+1, &ctx->readfds, &ctx->writefds, NULL, to );
	}
#endif

#if ASYNC_NETIO_USE_KQUEUE
	struct timespec	*to;
	struct timespec timeout_spec;
	
	if ( timeout == NULL )
	{
		to = NULL;
	}
	else
	{
		timeout_spec.tv_sec = timeout->tv_sec;
		timeout_spec.tv_nsec = timeout->tv_usec * 1000;
		to = &timeout_spec;
	}

	errno = 0;
#ifdef EV_SET64
	ctx->num = kevent64( anioKQ, NULL, 0, ctx->kv, kMaxAsyncIOEvents, 0, to );
#else
	ctx->num = kevent( anioKQ, NULL, 0, ctx->kv, kMaxAsyncIOEvents, to );
#endif
#endif

	result = 0;

exit:

	return result;
}

int				AsyncIO_ProcessEvents( AsyncIOEventsContext inEventsContext )
{
	int result = -1;
	AsyncIOEventsContext ctx = inEventsContext;
	
	require( inEventsContext != NULL, exit );

#if ASYNC_NETIO_USE_SELECT

	if ( ctx->num == 0 )
	{
		// timeout reached
		if ( aio_next_enabled_timer != NULL )
		{
			AsyncIO anio = aio_next_enabled_timer;

			// we don't do multishot timers, so we want to remove this one from the list
			AsyncIO_DisableTimer( anio );
			anioInProgress = anio;
			(*(anio->callback))( kAIO_TIMER_FIRED, anio, -1, anio->userdata );
			anioInProgress = NULL;
		}
		else
		{
			// I'm not certain this can happen, but I'm gonna check it just in case
			dlog( kDebugLevelTrace, "AsyncIO: timer fired, but has been disabled/deleted since priming\n" );
		}
	}
	else if ( ctx->num > 0 )
	{
		for ( i = 0; i <= ctx->maxFd; i++ )
		{
			if ( ctx->num == 0 )
				break;

			if ( FD_ISSET( i, &ctx->readfds ) )
			{
				ctx->num--;

				AsyncIO anio;
				lwip_socket_get_userdata( i, (void**)&anio );

				// clear it before call back, because they may request it again DURING the callback
				FD_CLR( i, &anioReadSet );

				anioInProgress = anio;
				if ( anio->type == kAIO_TYPE_LISTENER )
					(*(anio->callback))( kAIO_NEW_CONNECTION, anio, anio->fd, anio->userdata );
				else if ( anio->type == kAIO_TYPE_CONNECTION )
				{
					anio->notifyOnRead = false;
					(*(anio->callback))( kAIO_DATA_AVAILABLE, anio, anio->fd, anio->userdata );
				}
				anioInProgress = NULL;
			}

			if ( FD_ISSET( i, &ctx->writefds ) )
			{
				num--;

				AsyncIO anio;
				lwip_socket_get_userdata( i, (void**)&anio );

				// clear it before call back, because they may request it again DURING the callback
				FD_CLR( i, &anioWriteSet );

				anioInProgress = anio;
				anio->notifyOnWrite = false;
				(*(anio->callback))( kAIO_READY_FOR_WRITE, anio, anio->fd, anio->userdata );
				anioInProgress = NULL;
			}
		}

		check( ctx->num == 0 );
	}
#endif

#if ASYNC_NETIO_USE_KQUEUE

	AsyncIO anio;
	int ident;

	int i;
	for ( i = 0; i < ctx->num; i++ )
	{
		anio = (AsyncIO)ctx->kv[i].udata;
		anioInProgress = anio;

		switch ( ctx->kv[i].filter )
		{
			case EVFILT_READ:
				{
					ident = (int)ctx->kv[i].ident;
					if ( anio->type == kAIO_TYPE_LISTENER )
						(*(anio->callback))( kAIO_NEW_CONNECTION, anio, ident, anio->userdata );
					else if ( anio->type == kAIO_TYPE_CONNECTION )
					{
						anio->notifyOnRead = false;
						(*(anio->callback))( kAIO_DATA_AVAILABLE, anio, ident, anio->userdata );
					}
				}
				break;

			case EVFILT_WRITE:
				{
					ident = (int)ctx->kv[i].ident;
					anio->notifyOnWrite = false;
					(*(anio->callback))( kAIO_READY_FOR_WRITE, anio, ident, anio->userdata );
				}
				break;

			case EVFILT_TIMER:
				{
					(*(anio->callback))(kAIO_TIMER_FIRED, anio, -1, anio->userdata );
				}
				break;

			case EVFILT_PROC:
				{
					ident = (int)ctx->kv[i].ident;
					(*(anio->callback))(kAIO_PROCESS_EXITED, anio, ident, anio->userdata );
				}
				break;

			case EVFILT_SIGNAL:
				{
					ident = (int)ctx->kv[i].ident;
					(*(anio->callback))(kAIO_SIGNAL_DELIVERED, anio, ident, anio->userdata );
				}
		}

		if ( ctx->kv[i].flags & EV_EOF )
		{
			//check( kv.filter == EVFILT_READ );
			if ( ctx->kv[i].filter == EVFILT_READ )
			{
				// adding log message - can't remember if this works...
				dlog( kDebugLevelChatty, "kevent: EV_EOF hit\n" );

				require_continue_quiet( anioInProgress == anio );	// make sure it didn't get freed

				ident = (int)ctx->kv[i].ident;

				// let them know the socket closed
				(*(anio->callback))( kAIO_CONNECTION_CLOSED, anio, ident, anio->userdata );
			}
			else
			{
				// why do we get these?
			}
		}
		
		anioInProgress = NULL;
	}
	#endif

	result = 0;

exit:

	return result;
}




int AsyncIO_Run( bool keepRunning )
{
	int result = -1;

#if THRASH_DETECTION
	runs++;
	if ( !tvStartTimeValid )
	{
		gettimeofday( &tvStart, NULL );
	}

#if 1
	gettimeofday( &tvEnd, NULL );
	if ( tvEnd.tv_sec - tvStart.tv_sec >= 2 )
	{
		if ( ( tvEnd.tv_sec - tvStart.tv_sec == 2 ) && ( runs > 1000 ) )
		{
			dlog( kDebugLevelMax, "AsyncIO_Run: thrashing?\n" );
			sleep( 1 );
		}
		runs = 0;
		gettimeofday( &tvStart, NULL );
	}
#else
	if ( runs > 1000 )
	{
		gettimeofday( &tvEnd, NULL );
		if ( tvEnd.tv_sec - tvStart.tv_sec <= 2 )
		{
			dlog( kDebugLevelMax, "AsyncIO_Run: thrashing?\n" );
			sleep( 1 );
		}
	}
#endif
#endif


	check( anioInProgress == NULL );

#if TARGET_OS_FREERTOS
	UBaseType_t stackFreeNow, stackFree = uxTaskGetStackHighWaterMark( NULL );

	if ( keepRunning )
	{
		dlog( kDebugLevelTrace, "AsyncIO_Run: current stack low water mark is %lu\n", (unsigned long)stackFree );
	}
#endif

#if ASYNC_NETIO_USE_SELECT

	// take a look at <https://www.freertos.org/Pend-on-multiple-rtos-objects.html>
	// see if it's possible to have other objects we could wait on, inside the lwip select (we might need to have it call back to us)

#warning "FIX ME: select case never posts kAIO_CONNECTION_CLOSED events for read"
	// we don't handle the EV_EOF case

	fd_set	readfds;
	fd_set	writefds;
	int		maxFd;

	while ( 1 )
	{
		int i;
		struct timeval timeout;
		struct timeval *to;

		FD_ZERO( &readfds );
		FD_ZERO( &writefds );
		maxFd = -1;
		for ( i = 0; i < LWIP_SELECT_MAXNFDS; i++ )
		{
			if ( FD_ISSET( i, &anioReadSet ) )
			{
				maxFd = i;
				FD_SET( i, &readfds );
			}
			if ( FD_ISSET( i, &anioWriteSet ) )
			{
				maxFd = i;
				FD_SET( i, &writefds );
			}
		}

		// if there's nothing to monitor, we can't do anything here
		//require( maxFd != -1, exit );
		
		#warning "FIX ME: we need to know when the next timer will fire"
#if 1
		to = NULL;
		aio_next_enabled_timer = NULL;
		AsyncIO_GetNextTimerFireTime( &timeout, &aio_next_enabled_timer );
		if ( aio_next_enabled_timer != NULL )
		{
			to = &timeout;
		}
#else
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		to = NULL;
		if (!keepRunning )
		{
			to = &timeout;
		}
#endif

		require( ( maxFd >= 0 ) || ( to != NULL ), exit );
		int num;

//if ( log_aio_hap_failed ) { dlog( kDebugLevelMax, "hap: AsyncIO_Run\n" ); }



		if ( maxFd == -1 )
		{
			num = select( 0, NULL, NULL, NULL, to );
		}
		else
		{
			num = select( maxFd+1, &readfds, &writefds, NULL, to );
		}

		if ( num == 0 )
		{
			// timeout reached
			if ( aio_next_enabled_timer != NULL )
			{
				AsyncIO anio = aio_next_enabled_timer;

				// we don't do multishot timers, so we want to remove this one from the list
				AsyncIO_DisableTimer( anio );
				anioInProgress = anio;
				(*(anio->callback))( kAIO_TIMER_FIRED, anio, -1, anio->userdata );
				anioInProgress = NULL;
			}
			else
			{
				// I'm not certain this can happen, but I'm gonna check it just in case
				dlog( kDebugLevelTrace, "AsyncIO: timer fired, but has been disabled/deleted since priming\n" );
			}
		}
		else if ( num > 0 )
		{
			for ( i = 0; i <= maxFd; i++ )
			{
				if ( num == 0 )
					break;

				if ( FD_ISSET( i, &readfds ) )
				{
					num--;

					AsyncIO anio;
					lwip_socket_get_userdata( i, (void**)&anio );

					// clear it before call back, because they may request it again DURING the callback
					FD_CLR( i, &anioReadSet );

					anioInProgress = anio;
					if ( anio->type == kAIO_TYPE_LISTENER )
						(*(anio->callback))( kAIO_NEW_CONNECTION, anio, anio->fd, anio->userdata );
					else if ( anio->type == kAIO_TYPE_CONNECTION )
					{
						anio->notifyOnRead = false;
						(*(anio->callback))( kAIO_DATA_AVAILABLE, anio, anio->fd, anio->userdata );
					}
					anioInProgress = NULL;
				}

				if ( FD_ISSET( i, &writefds ) )
				{
					num--;

					AsyncIO anio;
					lwip_socket_get_userdata( i, (void**)&anio );

					// clear it before call back, because they may request it again DURING the callback
					FD_CLR( i, &anioWriteSet );

					anioInProgress = anio;
					anio->notifyOnWrite = false;
					(*(anio->callback))( kAIO_READY_FOR_WRITE, anio, anio->fd, anio->userdata );
					anioInProgress = NULL;
				}
			}

			check( num == 0 );
		}
		else
		{
			dlog( kDebugLevelTrace, "AsyncIO_Run: select returned %d (error = %d)\r\n", num, errno );
			break;
		}

		if ( !keepRunning )
			break;

#if TARGET_OS_FREERTOS
		stackFreeNow = uxTaskGetStackHighWaterMark( NULL );
		if ( stackFreeNow < stackFree )
		{
			stackFree = stackFreeNow;
			dlog( kDebugLevelTrace, "AsyncIO_Run: current stack low water mark is now %lu\n", (unsigned long)stackFree );
		}
#endif
	}
#endif

#if ASYNC_NETIO_USE_KQUEUE
	AsyncIO		anio;
	struct timespec	*to;
	struct timespec timeout;
#ifdef EV_SET64
	struct kevent64_s	kv;
#else
	struct kevent		kv;
#endif
	bool	got_first_event;


	got_first_event = false;
	while ( true )
	{
		int ident;
		int num;
		
		timeout.tv_sec = 0;
		timeout.tv_nsec = 0;
		to = NULL;
		// for the first event, we always use a NULL timeout...
		if ( ( !keepRunning ) && ( got_first_event ) )
			to = &timeout;
		errno = 0;
#ifdef EV_SET64
		num = kevent64( anioKQ, NULL, 0, &kv, 1, 0, to );
#else
		num = kevent( anioKQ, NULL, 0, &kv, 1, to );
#endif
		if ( ( num == 0 ) && ( !keepRunning ) )
		{
			result = 0;
			break;
		}
		if ( num != 1 ) { dlog( kDebugLevelError, "AsyncIO: kevent result %d (error %d)\n", num, errno ); }
#if __APPLE__
		if ( ( num != 1 ) && ( errno == EINTR ) && ( debug_running_in_debugger() ) ) { dlog( kDebugLevelTrace, "AsyncIO: kevent signal in debugger, likely breakpoint, ignoring\n" ); continue; }
#endif
		require_quiet( num == 1, exit );
		got_first_event = true;

		anio = (AsyncIO)kv.udata;
		anioInProgress = anio;


		switch ( kv.filter )
		{
			case EVFILT_READ:
				{
					ident = (int)kv.ident;
					if ( anio->type == kAIO_TYPE_LISTENER )
						(*(anio->callback))( kAIO_NEW_CONNECTION, anio, ident, anio->userdata );
					else if ( anio->type == kAIO_TYPE_CONNECTION )
					{
						anio->notifyOnRead = false;
						(*(anio->callback))( kAIO_DATA_AVAILABLE, anio, ident, anio->userdata );
					}
				}
				break;

			case EVFILT_WRITE:
				{
					ident = (int)kv.ident;
					anio->notifyOnWrite = false;
					(*(anio->callback))( kAIO_READY_FOR_WRITE, anio, ident, anio->userdata );
				}
				break;

			case EVFILT_TIMER:
				{
					(*(anio->callback))(kAIO_TIMER_FIRED, anio, -1, anio->userdata );
				}
				break;

			case EVFILT_PROC:
				{
					ident = (int)kv.ident;
					(*(anio->callback))(kAIO_PROCESS_EXITED, anio, ident, anio->userdata );
				}
				break;

			case EVFILT_SIGNAL:
				{
					ident = (int)kv.ident;
					(*(anio->callback))(kAIO_SIGNAL_DELIVERED, anio, ident, anio->userdata );
				}
		}

		if ( kv.flags & EV_EOF )
		{
			//check( kv.filter == EVFILT_READ );
			if ( kv.filter == EVFILT_READ )
			{
				// adding log message - can't remember if this works...
				dlog( kDebugLevelChatty, "kevent: EV_EOF hit\n" );

				require_continue_quiet( anioInProgress == anio );	// make sure it didn't get freed

				ident = (int)kv.ident;

				// let them know the socket closed
				(*(anio->callback))( kAIO_CONNECTION_CLOSED, anio, ident, anio->userdata );
			}
			else
			{
				// why do we get these?
			}
		}
		
		anioInProgress = NULL;
	}
#endif

exit:

	anioInProgress = NULL;
	
	return result;
}




// when input is closed, we close the output? unless flag is set?

#define REDIR_STATE_WAITING_FOR_DATA	1
#define REDIR_STATE_SENDING				2
//#define REDIR_STATE_WAITING_TO_SEND		3

typedef struct OpaqueAsyncIORedirect
{
	int 		state;

	//int fd_in;
	//int fd_out;
	const char * label;

	int			fd_in;
	AsyncIO 	anio_in;
	//bool		read_pending;

	int			fd_out;
	AsyncIO	anio_out;
	//bool		write_pending;

	char		buffer[512];
	size_t		num_in_buffer;

	AsyncRedirectIOEvent	callback;
	void*					callback_data;

} OpaqueAsyncIORedirect;

static void redirect_Pump( AsyncRedirectIO redir )
{
	bool	done = false;

	while ( !done )
	{
		switch ( redir->state )
		{
			case REDIR_STATE_WAITING_FOR_DATA:
				{
					ssize_t num_in;

					num_in = read( redir->fd_in, redir->buffer, sizeof( redir->buffer ) );
					if ( num_in < 0 )
					{
						if ( errno == EWOULDBLOCK )
						{
							AsyncIO_NotifyOnReadability( redir->anio_in );
							done = true;
							break;
						}

						dlog( kDebugLevelError, "AsyncIORedirect: error reading from input (%d)\n", errno );
						(redir->callback)( kAIO_REDIRECT_INPUT_ERROR_EVENT, redir, redir->callback_data );
					}
					else if ( num_in == 0 )
					{
						dlog( kDebugLevelChatty, "AsyncIORedirect: zero-length read from input (closed?)\n" );
						done = true;
						break;
					}
					else
					{
						redir->num_in_buffer = num_in;
						redir->state = REDIR_STATE_SENDING;
					}
				}
				break;

			case REDIR_STATE_SENDING:
				{
					ssize_t num_out = write( redir->fd_out, redir->buffer, redir->num_in_buffer );

					if ( num_out < 0 )
					{
						if ( ( errno == EWOULDBLOCK ) || ( errno == EAGAIN ) )
						{
							//redir->state = REDIR_STATE_WAITING_TO_SEND;
							AsyncIO_NotifyOnWritability( redir->anio_out );
							done = true;
							break;
						}

						dlog( kDebugLevelError, "AsyncIORedirect: error writing to output (%d)\n", errno );
						(redir->callback)( kAIO_REDIRECT_OUTPUT_ERROR_EVENT, redir, redir->callback_data );
					}
					else if ( num_out < redir->num_in_buffer )
					{
						// clear out anything we have
						size_t remaining = redir->num_in_buffer - num_out;
						memmove( redir->buffer, &redir->buffer[ num_out ], remaining );

						// we'll try to send again
					}
					else
					{
						(redir->callback)( kAIO_REDIRECT_DATA_WRITTEN, redir, redir->callback_data );

						redir->state = REDIR_STATE_WAITING_FOR_DATA;
					}
				}
				break;

			default:
				check( 0 );
				break;
		}
	}

}

static void redirect_AsyncIOEvent( int eventID, AsyncIO anio, int fd, void * userData )
{
	AsyncRedirectIO redir = (AsyncRedirectIO)userData;

	switch( eventID )
	{
		case kAIO_DATA_AVAILABLE:
			{
				check( fd == redir->fd_in );

				(redir->callback)( kAIO_REDIRECT_DATA_READY, redir, redir->callback_data );

				redirect_Pump( redir );
			}
			break;

		case kAIO_READY_FOR_WRITE:
			{
				check( fd == redir->fd_out );

				redirect_Pump( redir );
			}
			break;

		case kAIO_CONNECTION_CLOSED:
			{
				check( fd == redir->fd_in );

				// generate a callback?
				// close redir->fd_out?
				(redir->callback)( kAIO_REDIRECT_INPUT_CLOSED_EVENT, redir, redir->callback_data );
			}
			break;
	}
}

AsyncRedirectIO AsyncIO_Redirect( int fd_in, int fd_out, AsyncRedirectIOEvent callback, void * callback_data )
{
	AsyncRedirectIO result = NULL;
	AsyncRedirectIO redir;
	int err;

	redir = (AsyncRedirectIO)calloc( 1, sizeof( *redir ) );
	require( redir != NULL, exit );

	redir->callback = callback;
	redir->callback_data = callback_data;

	redir->fd_in = fd_in;
	redir->anio_in = AsyncIO_NewConnection( fd_in, redirect_AsyncIOEvent, redir );
	require( redir->anio_in != NULL, exit );

	err = AsyncIO_NotifyOnReadability( redir->anio_in );
	require( err == 0, exit );

	redir->state = REDIR_STATE_WAITING_FOR_DATA;

	redir->fd_out = fd_out;
	redir->anio_out = AsyncIO_NewConnection( fd_out, redirect_AsyncIOEvent, redir );
	require( redir->anio_out != NULL, exit );

	result = redir;
	redir = NULL;

exit:

	if ( redir != NULL )
	{
		ForgetAsyncIO( &redir->anio_in, 0 );
		ForgetAsyncIO( &redir->anio_out, 0 );
		ForgetMem( &redir );
	}

	return result;
}

void AsyncIO_ReleaseRedirect( AsyncRedirectIO redir, bool closeIn, bool closeOut )
{
	require( redir != NULL, exit );

	ForgetAsyncIO( &redir->anio_in, closeIn );
	ForgetAsyncIO( &redir->anio_out, closeOut );
	ForgetMem( &redir );

exit:
	;
}










//#define ANIO_TESTING	1

#if ANIO_TESTING

struct anio_listener
{
	AsyncIO	anio;
};

struct anio_connection
{
	AsyncIO	anio;

	int 		client;

	uint8_t	*output_buffer;
	size_t	output_buffer_size;
	size_t	output_buffer_pos;
};

static void		AsyncIOTest_DoRead( struct anio_connection * connection )
{
	uint8_t	buffer[32];
	while ( 1 )
	{
		ssize_t		amount, num_sent;

		amount = AsyncIO_Read( connection->anio, buffer, sizeof( buffer ) );

		if ( ( amount < 0 ) && ( errno == EWOULDBLOCK  ) )
		{
			break;
		}

		if ( amount == 0 )
		{
			// is this handled by EOF flag? what about when we get through here in the write case?
			break;
		}

		require( amount > 0, exit );

		dlog( kDebugLevelTrace, "received %ld bytes\n", (long)amount );
		
		// we don't send data as the client
		if ( connection->client )
			continue;

		connection->output_buffer = malloc( amount * 10 );
		connection->output_buffer_pos = 0;
		connection->output_buffer_size = amount * 10;

		{
			unsigned int i;
			for ( i = 0; i < 10; i++ )
			{
				memcpy( &(connection->output_buffer[i * amount ]), buffer, amount );
			}
		}

		num_sent = AsyncIO_Write( connection->anio, &connection->output_buffer[ connection->output_buffer_pos ], connection->output_buffer_size - connection->output_buffer_pos );
		require( num_sent >= 0, exit );

		dlog( kDebugLevelTrace, "sent %ld bytes\n", (long)num_sent );
		connection->output_buffer_pos += num_sent;
		if ( connection->output_buffer_size - connection->output_buffer_pos != 0 )
			break;
	}

exit:
	;
}


static void 	AsyncIOTestCallback( int eventID, AsyncIO anio, int fd, void * userData )
{
	switch( eventID )
	{
		case kAIO_NEW_CONNECTION:
			{
				struct sockaddr_storage	ss;
				socklen_t	ss_len;

				ss_len = sizeof( ss );
				int new_fd = accept( fd, (struct sockaddr*)&ss, &ss_len );
				if ( new_fd >= 0 )
				{
					// hack in a small output buffer to test the output code
					int err;
					unsigned int	snd_buf_size = 32;
					err = setsockopt( new_fd, SOL_SOCKET, SO_SNDBUF, &snd_buf_size, sizeof( snd_buf_size ) );
					require( err == 0, exit );

					struct anio_connection *c = calloc( 1, sizeof( struct anio_connection ) );
					c->anio = AsyncIO_NewConnection( new_fd, AsyncIOTestCallback, c );
				}
			}
			break;

		case kAIO_CONNECTION_CLOSED:
			{
				dlog( kDebugLevelTrace, "connection closed\n" );
				AsyncIO_Release( anio );
			}
			break;

		case kAIO_READY_FOR_WRITE:
			{
				dlog( kDebugLevelTrace, "socket is ready for writing\n" );

				struct anio_connection * connection = (struct anio_connection*)userData;
				
				if ( connection->client )
				{
					connection->output_buffer = strdup( "this is a message\n" );
					connection->output_buffer_size = strlen( connection->output_buffer );
					connection->output_buffer_pos = 0;
				}
				

				ssize_t num_sent = AsyncIO_Write( anio, &connection->output_buffer[ connection->output_buffer_pos ], connection->output_buffer_size - connection->output_buffer_pos );
				require( num_sent >= 0, exit );

				dlog( kDebugLevelTrace, "sent %ld bytes\n", (long)num_sent );
				connection->output_buffer_pos += num_sent;
				if ( connection->output_buffer_size - connection->output_buffer_pos == 0 )
				{
					connection->output_buffer_size = 0;
					connection->output_buffer_pos = 0;
					ForgetMem( &connection->output_buffer );

					AsyncIOTest_DoRead( connection );
				}
			}
			break;

		case kAIO_DATA_AVAILABLE:
			{
#if 1
				struct anio_connection * connection = (struct anio_connection*)userData;

				AsyncIOTest_DoRead( connection );
#else
				uint8_t	buffer[32];
				while ( 1 )
				{
					ssize_t		amount, num_sent;

					amount = AsyncIO_Read( anio, buffer, sizeof( buffer ) );

					if ( amount == 0 )
					{
						// is this handled by EOF flag?
						break;
					}

					require( amount > 0, exit );

					dlog( kDebugLevelTrace, "received %ld bytes\n", (long)amount );

					struct anio_connection * connection = (struct anio_connection*)userData;

					connection->output_buffer = malloc( amount * 10 );
					connection->output_buffer_pos = 0;
					connection->output_buffer_size = amount * 10;

					{
						unsigned int i;
						for ( i = 0; i < 10; i++ )
						{
							memcpy( connection->output_buffer[i * amount ], buffer, amount );
						}
					}

					num_sent = AsyncIO_Write( anio, &connection->output_buffer[ connection->output_buffer_pos ], connection->output_buffer_size - connection->output_buffer_pos );
					require( num_sent >= 0, exit );

					dlog( kDebugLevelTrace, "sent %ld bytes\n", (long)num_sent );
					connection->output_buffer_pos += num_sent;
					if ( connection->output_buffer_size - connection->output_buffer_pos != 0 )
						break;
				}
#endif

			}
			break;
	}

exit:
	;
}


void	AsyncIOTests( void )
{
	int fd, err, flags;

	dlog_set_level( kDebugLevelTrace );
	AsyncIO_Initialize();

	fd = socket( AF_INET6, SOCK_STREAM, IPPROTO_TCP );
	require( fd >= 0, exit );

	flags = 0;
	err = setsockopt( fd, IPPROTO_IPV6, IPV6_V6ONLY, &flags, sizeof( flags ) );
	require( err == 0, exit );

	struct sockaddr_in6 sin6;
	memset( &sin6, 0, sizeof( sin6 ) );
	sin6.sin6_family = AF_INET6;
	err = bind( fd, (struct sockaddr*)&sin6, sizeof( sin6 ) );
	require( err == 0, exit );

	socklen_t	sin6_len;
	sin6_len = sizeof( sin6 );
	err = getsockname( fd, (struct sockaddr*)&sin6, &sin6_len );
	require( err == 0, exit );

	fprintf( stdout, "Bound to port %hu\n", ntohs( sin6.sin6_port ) );
	err = listen( fd, 10 );
	require( err == 0, exit );

	struct anio_listener listener;
	listener.anio = AsyncIO_NewConnectionListener( fd, AsyncIOTestCallback, &listener );
	require( listener.anio != NULL, exit );

	struct anio_connection connector;
	fd = socket( AF_INET6, SOCK_STREAM, IPPROTO_TCP );
	require( fd >= 0, exit );
	
	connector.anio = AsyncIO_NewConnection( fd, AsyncIOTestCallback, &connector );
	require( connector.anio != NULL, exit );
	
	// port stays the same, but we want to connect to loopback
	memcpy( &sin6.sin6_addr, &in6addr_loopback, sizeof( in6addr_loopback ) );
	err = connect( fd, (struct sockaddr*)&sin6, sizeof( sin6 ) );
	dlog( kDebugLevelError, "connect = %d\n", errno );
	//require( ( err < 0 ) && ( errno == EINPROGRESS ), exit );

	

#if ASYNC_NETIO_USE_RUN_LOOP
	CFRunLoopRun();
#else
	AsyncIO_Run( 1 );
#endif

exit:
	;
}

#endif

#endif

