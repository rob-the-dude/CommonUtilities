/*
 *	SerialUtilities.c
 *
 *	Copyright (C) 2023-2024 Rob Newberry <robthedude@mac.com>
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

#include "SerialUtilities.h"

#include "CommonUtilities.h"
#include "DebugUtilities.h"


int GetTerminalSettings( int fd, struct termios *tio )
{
	int err;
	
	err = tcgetattr( fd, tio );
	require( err == 0, exit );
	
exit:

	return err;
}

int SetTerminalSettings( int fd, struct termios *tio )
{
	int err;
	
	err = tcsetattr( fd, TCSANOW, tio );
	require_action_quiet( err == 0, exit, dlog( kDebugLevelError, "tcsetattr: error %d\n", errno ) );
	
exit:

	return err;

}

int ConfigureTerminalSettings( struct termios *tio, int speed, int data_bits, int stop_bits, bool parity, serial_flow_control_t flow_control, bool add_CR_to_NL )
{
	int err = -1;

	// laziness, pure and simple -- we only currently support 8N1
	require( data_bits == 8, exit );
	require( stop_bits == 1, exit );
	require( parity == 0, exit );

	err = cfsetispeed( tio, speed );
	require( err == 0, exit );
	err = cfsetospeed( tio, speed );
	require( err == 0, exit );

	cfmakeraw( tio );
	
	// input processing: (c_iflag)
	tio->c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON );
	tio->c_iflag |= ( flow_control == kSERIAL_FLOW_CONTROL_SW ) ? (IXON | IXOFF | IXANY) : 0;

	// output processing: (c_oflag)
	tio->c_oflag = add_CR_to_NL ? ( OPOST | ONLCR ) : 0;

	// line processing: (c_lflag)
	tio->c_lflag &= ~( ECHO | ECHONL | ICANON | IEXTEN | ISIG );

	// character procewssing: (c_cflag)

	//tio->c_cflag |= ( CLOCAL | CREAD );
	//tio->c_cflag &= ~( PARENB | CSTOPB | CSIZE )
	//tio->c_cflag = CLOCAL | CREAD;
	tio->c_cflag &= ~( PARENB | CSIZE );
	tio->c_cflag |= CS8;
	tio->c_cflag |= ( flow_control == kSERIAL_FLOW_CONTROL_HW ) ? CRTSCTS : 0;

	// make sure to set these...
	tio->c_cc[VMIN] = 1;
	tio->c_cc[VTIME] = 0;

	err = 0;

exit:

	return err;
}

int ConfigureTTY( int fd, int speed, int data_bits, int stop_bits, bool parity, serial_flow_control_t flow_control, bool add_CR_to_NL )
{
	int result = -1;
	int err;
	struct termios tio;
	
	err = GetTerminalSettings( fd, &tio );
	require( err == 0, exit );
	
	err = ConfigureTerminalSettings( &tio, speed, data_bits, stop_bits, parity, flow_control, add_CR_to_NL );
	require( err == 0, exit );
	
	err = SetTerminalSettings( fd, &tio );
	require( err == 0, exit );
	
	result = 0;

exit:

	return result;
}
