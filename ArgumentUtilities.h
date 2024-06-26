/*
 *	ArgumentUtilities.h
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

#ifndef __ARGUMENT_UTILITIES_H__
#define __ARGUMENT_UTILITIES_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	kFIND_OPT_U16		= 1,	// uint16_t
	kFIND_OPT_U32		= 2,	// uint32_t
	kFIND_OPT_U64		= 3,	// uint64_t
	kFIND_OPT_STR_DUP	= 4,	// strdup'd string is returned, caller must free
	kFIND_OPT_STR_REF	= 5,	// reference to argv[x] is returned, caller should not free
	kFIND_OPT_INDEX		= 6		// int, argc is returned (can reference directly from argv)
} FindOptionParameterType;

int	FindOptionWithValue( int argc, const char *argv[], char option, FindOptionParameterType type, void * value );
int FindOption( int argc, const char *argv[], char option );
int FindArgument( int argc, const char *argv[], const char *option, int *position );


#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __ARGUMENT_UTILITIES_H__ */


