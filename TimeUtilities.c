/*
 *	TimeUtilities.c
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

#include "TimeUtilities.h"

#include "CommonUtilities.h"
#include "DebugUtilities.h"

bool IsLeapYear( int year )
{
	if ( year % 400 == 0 )	{ return true; }
	if ( year % 100 == 0 )	{ return false; }
	if ( year % 4 == 0 )	{ return true; }

	return false;
}

static int			days_in_month[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const char*	month_strings[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

const char*		MonthString( int m )
{
	const char* result = NULL;
	require( ( m > 0 ) && ( m < 13 ), exit );

	result = month_strings[ m - 1 ];

exit:

	return result;
}


int DaysInMonth( int m, int y )
{
	int result = 0;
	require( ( m > 0 ) && ( m < 13 ), exit );

	result = days_in_month[ m - 1 ];
	require_quiet( m == 2, exit );

	result += IsLeapYear( y ) ? 1 : 0;

exit:

	return result;
}

#if TARGET_OS_UNIXLIKE

uint64_t	NanosecondCounter( void )
{
	uint64_t			result;
	struct timespec		now;

	clock_gettime( CLOCK_MONOTONIC, &now );

	result = now.tv_sec;
	result *= 1000000000ULL;
	result += now.tv_nsec;

	return result;
}

void		NanosecondsToTimespec( uint64_t nanoseconds, struct timespec *ts )
{
	uint64_t	seconds = nanoseconds / 1000000000;
	ts->tv_sec = seconds;

	if ( seconds > 0 )
	{
		seconds *= 1000000000;
		nanoseconds -= seconds;
	}
	ts->tv_nsec = nanoseconds;
}

uint32_t	MillisecondCounter( void )
{
	uint64_t	nanos = NanosecondCounter();

	nanos /= ( 1000 * 1000 );

	return (uint32_t)nanos;
}

void		DelayMilliseconds( uint32_t ms )
{
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = ( ms % 1000 ) * NANOSECONDS_PER_MILLISECOND;
	nanosleep( &ts, NULL );
}

#endif
