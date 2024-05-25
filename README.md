
# CommonUtilities

I've been using and enhancing this library of utility routines for almost a dozen years.  

At its core are two files (CommonUtilities.h and DebugUtilities.h) that are inspired by code I encountered at Apple (specifically, AssertMacros.h and the debug library in mDNSResponder).  I use these macros almost everywhere because this:

	int some_function( int parameter )
	{
		int result = -1;
		int err;

		err = step1( parameter );
		require( err != 0, exit );

		err = step2( parameter );
		require( err != 0, exit );

		err = step3( parameter );
		require( err != 0, exit );

		result = 0;

	exit:
		
		return result;
	}

is so much easier to read than this:

	int some_function( int parameter )
	{
		int result = -1;
		int err;

		err = step1( parameter );
		if ( err == 0 )
		{
			err = step2( parameter );
			if ( err == 0 )
			{
				err = step3( parameter );
				if ( err == 0 )
				{
					result = 0;
				}
			}
		}

		return result;
	}
		
My version is usually API compatible but has my own implementation, and in many cases has diverged or added APIs as I needed.

Beyond the core set of macros, there are libraries that attempt to provide a common interface for doing "common" things across different OSes (i.e., a "delay" API that will work on macOS, NetBSD, FreeRTOS and Zephyr), or for doing "complicated" things in a relatively portable way (i.e., a library for doing asynchronous I/O for macOS, NetBSD and Linux that might partially work on an RTOS).

