/*
 *	CommonUtilities.h
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

#ifndef __COMMON_UTILITIES_H__
#define __COMMON_UTILITIES_H__

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// do we really care about TARGET_CPU ?

#if __NetBSD__

	#define TARGET_OS_UNIXLIKE		1
	#define TARGET_OS_NETBSD		1

	#if ( __ARM_ARCH == 7 ) || ( __ARM_ARCH == 8 )

		#define TARGET_CPU_ARM		1

	#elif ( __amd64 ) || ( __x86_64 )

		#define TARGET_CPU_X86_64	1

	#else

		#error

	#endif

	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

		#ifndef __LITTLE_ENDIAN__
		#define __LITTLE_ENDIAN__	1
		#endif /* __LITTLE_ENDIAN__ */

	#endif /* __BYTE_ORDER */


#elif __APPLE__

	#ifndef TARGET_OS_NETBSD
	#define TARGET_OS_NETBSD	0
	#endif

	#ifndef TARGET_OS_NONE
	#define TARGET_OS_NONE 0
	#endif


	// how do we distinguish between Mac & iOS?
	// can't asume x86 == Mac, because iOS simulator is x86

	// and what if we are trying to "simulate" FreeRTOS or NetBSD?
	#if __has_include(<FreeRTOS.h>)

		#define TARGET_OS_FREERTOS			1	// we're running FreeRTOS on Mac
		#define TARGET_OS_FREERTOS_SIM		1	// we're running FreeRTOS on Mac
		#define TARGET_OS_FREERTOS_SIMULATOR		TARGET_OS_FREERTOS_SIM

	#else

		#ifndef TARGET_OS_FREERTOS
			#define TARGET_OS_FREERTOS 	0
		#endif

		#ifndef TARGET_OS_FREERTOS_SIM
			#define TARGET_OS_FREERTOS_SIM 	0
			#define TARGET_OS_FREERTOS_SIMULATOR		TARGET_OS_FREERTOS_SIM
		#endif

		#ifndef TARGET_OS_ZEPHYR
			#define TARGET_OS_ZEPHYR		0
		#endif

		#define TARGET_OS_UNIXLIKE		1

		#if __has_include(<UIKit/UIKit.h>)
			#define TARGET_OS_IPHONE		1
		#else
			#define TARGET_OS_MAC			1
		#endif

	#endif

	#if __x86_64

		#define TARGET_CPU_X86_64	1

	#elif __arm64

		#define TARGET_CPU_ARM64	1

	#elif __arm__

		#define TARGET_CPU_ARM		1

	#else

		#error

	#endif

#elif __linux__

	#define TARGET_OS_UNIXLIKE		1
	#define TARGET_OS_LINUX			1

	#if (__ARM_ARCH == 6 ) || ( __ARM_ARCH == 7 ) || ( __ARM_ARCH == 8 )

		#define TARGET_CPU_ARM		1

	#elif __x86_64

		#define TARGET_CPU_X86_64	1

	#elif __arm64

		#define TARGET_CPU_ARM64	1

	#else

		#error

	#endif

	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

		#ifndef __LITTLE_ENDIAN__
		#define __LITTLE_ENDIAN__	1
		#endif /* __LITTLE_ENDIAN__ */

	#endif /* __BYTE_ORDER */



#elif ( ( __ARM_ARCH == 7 ) || ( __ARM_ARCH == 8 ) ) && ( __ARM_ARCH_PROFILE == 'M' )	// Cortex-M

	#if defined(TARGET_OS_NONE) && TARGET_OS_NONE

		// assume they know what they're doing

	#elif __ZEPHYR__

		#define TARGET_OS_ZEPHYR		1

	#else

		// this is the compiler we use for FreeRTOS, so assume that for now
		#define TARGET_OS_FREERTOS		1
		#define TARGET_OS_FREERTOS_SIM	0
		#define TARGET_OS_ZEPHYR		0

	#endif

	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

		#ifndef __LITTLE_ENDIAN__
		#define __LITTLE_ENDIAN__	1
		#endif /* __LITTLE_ENDIAN__ */

	#endif /* __BYTE_ORDER */

	#if __ARMCOMPILER_VERSION

	/* Keil/ARM compiler -- there are some things we may need to define... */
	#include <errno.h>

	#ifndef ENOMEM
		#define ENOMEM	100
	#endif

	#ifndef ENOBUFS
		#define ENOBUFS 101
	#endif

	#ifndef EBUSY
		#define EBUSY 102
	#endif

	#ifndef ETIMEDOUT
		#define ETIMEDOUT 103
	#endif

	#endif


#else

	#error Unable to determine TARGET_ setting

	// probably need to run <...>-gcc <flags> -dM -E - < /dev/null and see what's defined to help us figure it out

#endif


#ifdef __cplusplus
extern "C" {
#endif


#define IsValidFD( fd )					( fd >= 0 ) ? true : false
#define kInvalidFD						(-1)

#define ForgetMem( p )					do { if ( (*p) != NULL )		{ free( *p ); 						*p = NULL; 			} } while(0)
#define ForgetFD( fd )					do { if ( IsValidFD( *fd ) ) 	{ close( *fd ); 					*fd = kInvalidFD; 	} } while(0)
#define ForgetFILE( f )					do { if ( *f != NULL )			{ fclose( *f ); 					*f = NULL; 			} } while(0)
#define ForgetMMAP( addr, sz )			do { if ( *addr != MAP_FAILED )	{ munmap( *addr, sz );				*addr = NULL; 		} } while(0)
#define ForgetDIR( d )					do { if ( *d != NULL ) 			{ closedir( *d ); 					*d = NULL;	 		} } while(0)
#define Forgetaddrinfo( a )				do { if ( *a != NULL )			{ freeaddrinfo( *a ); 				*a = NULL; 			} } while(0)
#define Forgetifaddrs( a )				do { if ( *a != NULL )			{ freeifaddrs( *a ); 				*a = NULL; 			} } while(0)

#define ForgetMember( p, forget )		do { if ( p != NULL )			{ forget } } while(0)

#define ForgetDNSServiceRef( ref )		do { if ( *ref != NULL ) 		{ DNSServiceRefDeallocate( *ref ); 	*ref = NULL; 		} } while(0)

#if __APPLE__
#define ForgetCF( cf )					do { if ( *cf != NULL )			{ CFRelease( *cf ); *cf = NULL; 						} } while(0)
#endif



#if defined(__LITTLE_ENDIAN__)
	#define TARGET_RT_LITTLE_ENDIAN		1
	#define TARGET_RT_BIG_ENDIAN		0
#else
	#define TARGET_RT_LITTLE_ENDIAN		0
	#define TARGET_RT_BIG_ENDIAN		1
#endif

#ifndef NELEMENTS
	#define NELEMENTS(array)		sizeof( array ) / sizeof( array[0] )
#endif

#ifndef Minimum
	#define Minimum( a, b ) 		( ( a < b ) ? a : b )
#endif

#ifndef Maximum
	#define Maximum( a, b ) 		( ( a > b ) ? a : b )
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif
