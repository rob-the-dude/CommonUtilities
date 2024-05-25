/*
 *	StringUtilities.h
 *
 *	Copyright (C) 2021-2024 Rob Newberry <robthedude@mac.com>
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

#ifndef __STRING_UTILITIES_H__
#define __STRING_UTILITIES_H__

#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

// on many platform, we need to cast -- wish the platform would actually fix this
// we should test periodically to see if we can remove this, at least in some places
//	(we could conditionalize it on something we can deduce, like in CommonUtilities.h)

#define IsSpace( ch )		isspace( ((int)(ch)) )

#define IsDigit( ch )		isdigit( ((int)(ch)) )

#define IsXDigit( ch )		isxdigit( ((int)(ch)) )

#define ToLower( ch )		tolower( ((int)(ch)) )

#define ToUpper( ch )		toupper( ((int)(ch)) )


#if defined(_REDEFINE_STD_CHAR_MACROS) && _REDEFINE_STD_CHAR_MACROS
	
	#undef isspace
	#define	isspace(c)	IsSpace(c)

	#undef isdigit
	#define	isdigit(c)	IsDigit(c)

	#undef isxdigit
	#define isxdigit(c) IsXDigit(c)

	#undef tolower
	#define tolower(c) ToLower(c)

	#undef toupper
	#define toupper(c) ToUpper(c)

#endif

#ifdef __cplusplus
} // extern "C"
#endif


#endif /* __STRING_UTILITIES_H__ */
