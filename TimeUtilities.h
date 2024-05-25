/*
 *	TimeUtilities.h
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

#ifndef __TIME_UTILITIES_H__
#define __TIME_UTILITIES_H__

#include <sys/time.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool IsLeapYear( int year );
int DaysInMonth( int month, int year );

time_t			StringToTime( const char * dateString );
const char*		MonthString( int mon );


uint64_t	NanosecondCounter( void );
void		NanosecondsToTimespec( uint64_t nanoseconds, struct timespec *ts );

uint32_t	MillisecondCounter( void );

void		DelayMilliseconds( uint32_t ms );

#ifndef NANOSECONDS_PER_SECOND
	#define NANOSECONDS_PER_SECOND 		1000000000ULL
#endif
#ifndef NANOSECONDS_PER_MILLISECOND
	#define NANOSECONDS_PER_MILLISECOND 1000000ULL
#endif
#ifndef MICROSECONDS_PER_SECOND
	#define MICROSECONDS_PER_SECOND 	1000000ULL
#endif
#ifndef NANOSECONDS_PER_MICROSECOND
	#define NANOSECONDS_PER_MICROSECOND 1000ULL
#endif
#ifndef MILLISECONDS_PER_SECOND
	#define MILLISECONDS_PER_SECOND		1000ULL
#endif





#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __TIME_UTILITIES_H__ */
