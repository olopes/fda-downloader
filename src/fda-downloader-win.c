/**
 * fda-downloader - Simple reader for FlyDream Altimeter or Hobbyking Altimeter
 * 
 * Copyright (C) 2017  OLopes
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <windows.h>
#include "fda-downloader.h"

#define _DEV_NAME_SIZE 100

const char *TTY_DEVICE="COM9";
int fda_init(struct fda_state* state) {
	HANDLE hComm;
	DCB dcbSerialParams = {0};
	COMMTIMEOUTS timeouts = {0};
	char device [_DEV_NAME_SIZE];
	memset(device, 0, sizeof(char)*_DEV_NAME_SIZE);
	snprintf(device, _DEV_NAME_SIZE, "\\\\.\\%s", state->tty_device);

	hComm = CreateFile(device,                //port name
			GENERIC_READ | GENERIC_WRITE,     //Read/Write
			0,                                // No Sharing
			NULL,                             // No Security
			OPEN_EXISTING,                    // Open existing port only
			FILE_ATTRIBUTE_NORMAL,            // Non Overlapped I/O
			NULL);                            // Null for Comm Devices

	if (hComm == INVALID_HANDLE_VALUE) {
		print_msg("Error opening device %s\n", state->tty_device);
		return 1;
	}

	// Set device parameters (38400 baud, 1 start bit,
	// 1 stop bit, no parity)
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
	if (GetCommState(hComm, &dcbSerialParams) == 0)
	{
		print_msg("Error getting device state\n");
		CloseHandle(hComm);
		return 2;
	}

	dcbSerialParams.BaudRate = CBR_19200;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT; // or TWOSTOPBITS?
	dcbSerialParams.Parity = NOPARITY;
	if(SetCommState(hComm, &dcbSerialParams) == 0)
	{
		print_msg("Error setting device parameters\n");
		CloseHandle(hComm);
		return 3;
	}

	// Set COM port timeout settings - 0.5s like the linux one
	timeouts.ReadIntervalTimeout = 500;
	timeouts.ReadTotalTimeoutConstant = 500;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 500;
	timeouts.WriteTotalTimeoutMultiplier = 10;
	if(SetCommTimeouts(hComm, &timeouts) == 0)
	{
		print_msg("Error setting timeouts\n");
		CloseHandle(hComm);
		return 4;
	}

	// setup a RX event
	if(SetCommMask(hComm, EV_RXCHAR) == 0) {
		print_msg("Failed to set RX event\n");
		return 5;
	}


	state->handle=(void*)malloc(sizeof(HANDLE));
	memcpy(state->handle, &hComm, sizeof(HANDLE));
	return 0;
}

int fda_read(struct fda_state* state, unsigned char * buff, int n) {
	HANDLE hComm = *((HANDLE*)state->handle);
	DWORD dwEventMask, lastErr, r;
	
	r = -1;
	if (ReadFile(hComm,      // Handle of the Serial port
			buff,            // buffer
			n,               // bytes to read
			&r,              // Number of bytes read
			NULL) == 0) {
		lastErr = GetLastError();
		if (lastErr == ERROR_IO_PENDING) {
			// sleep/wait until data is available
			if(WaitCommEvent(hComm, &dwEventMask, NULL) == 0) {
				print_msg( "Error waiting for RX event\n");
				return -7;
			}
		} else if(lastErr != ERROR_SUCCESS) {
			char mbuf[256];
			FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, lastErr,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), mbuf, 256,
					NULL);
			print_msg("Could not read data from TTY: %s\n", mbuf);
			return -8;
		}
	}
	return (int)r;
}

int fda_wait(struct fda_state* state) {
	HANDLE hComm = *((HANDLE*)state->handle);
	DWORD dwEventMask;
	/* wait for an answer... */
	if(WaitCommEvent(hComm, &dwEventMask, NULL) == 0) {
		print_msg("Error waiting for RX event\n");
		return -7;
	}
	return 0;
}

int fda_write(struct fda_state* state, const unsigned char * buff, int n) {
	HANDLE hComm = *((HANDLE*)state->handle);
	DWORD bytes_written;

	// Send specified text (remaining command line arguments)
	print_msg("Sending bytes...\n");
	if(!WriteFile(hComm, buff, n, &bytes_written, NULL))
	{
		print_msg( "Write error\n");
		return -6;
	}
	print_msg("%lu bytes written\n", bytes_written);

	return (int) bytes_written;
}

int fda_close(struct fda_state* state) {
	HANDLE hComm = *((HANDLE*)state->handle);
	free(state->handle);

	// Close the Serial Port
	if(CloseHandle(hComm)==0) {
		print_msg("Error closing serial device\n");
		return -1;
	}

	return 0;
}

