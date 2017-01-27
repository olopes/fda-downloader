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
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include "fda-downloader.h"

// ler isto para ver se consigo usar o select()
// Tentar fazer o mais posix possivel
// https://www.cmrr.umn.edu/~strupp/serial.html
const char *TTY_DEVICE="/dev/ttyUSB1";



// http://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c

static int set_interface_attribs (int fd, int speed, int parity) {
	struct termios tty;
	memset (&tty, 0, sizeof tty);
	if (tcgetattr (fd, &tty) != 0)
	{
		perror ("error from tcgetattr");
		return -1;
	}

	cfsetospeed (&tty, speed);
	cfsetispeed (&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK;         // disable break processing
	tty.c_lflag = 0;                // no signaling chars, no echo,
	// no canonical processing
	tty.c_oflag = 0;                // no remapping, no delays
	tty.c_cc[VMIN]  = 0;            // read doesn't block
	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
	// enable reading
	tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr (fd, TCSANOW, &tty) != 0)
	{
		perror ("error from tcsetattr");
		return -1;
	}
	return 0;
}

static int set_blocking (int fd, int should_block) {
	struct termios tty;
	memset (&tty, 0, sizeof tty);
	if (tcgetattr (fd, &tty) != 0)
	{
		perror ("error from tggetattr");
		return -1;
	}

	tty.c_cc[VMIN]  = should_block ? 1 : 0;
	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	if (tcsetattr (fd, TCSANOW, &tty) != 0) {
		perror ("error setting term attributes");
		return -1;
	}

	return 0;
}

/* API FUNCTIONS */
struct fda_fd {
	int fd;
};

int fda_init(struct fda_state* state) {
	int fds;
	struct fda_fd *ptr;
	/* open and configure tty device */
	fds = open(state->tty_device, O_RDWR | O_NOCTTY | O_NDELAY);
	if(fds == -1) {
		perror("Error opening tty device");
		return -1;
	}

	/* set speed to 19,200 bps, 8n1 (no parity) */
	if(set_interface_attribs (fds, B19200, 0))
		return 3;
		
	/* set blocking */
	if(set_blocking (fds, 0))
		return 5;

	ptr =(struct fda_fd*) malloc(sizeof(struct fda_fd));
	ptr->fd=fds;

	state->handle=(void*)ptr;
	return 0;
}

static int _fda_do_select(int fd) {
	struct timeval timeout;
	int retval;
	fd_set rfds;
	// wait up to 1 second
	timeout.tv_sec=1;
	timeout.tv_usec=0;
    /* Watch fd to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
	retval = select(fd+1, &rfds, NULL, NULL, &timeout);

	if(retval < 0) {
		perror("select()\n");
		return retval;
	} else if (retval == 0) {
		print_msg("select () timeout\n");
	} else {
		print_msg("select () data ready\n");
	}
	return 0;
}

int fda_read(struct fda_state* state, unsigned char * buff, int n) {
	struct fda_fd *ptr=(struct fda_fd *)state->handle;
	int r = -1, fds = ptr->fd;
	if(_fda_do_select(fds))
		return -8;
	r = read(fds, buff, n);
	if(r == -1) {
		if(errno == EAGAIN) {
			errno = 0;
			r = 0;
		} else {
			perror("Could not read data from TTY");
			return -8;
		}
	}
	return r;
}

int fda_flush(struct fda_state* state) {
	struct fda_fd *ptr=(struct fda_fd *)state->handle;
	int fds = ptr->fd;
	print_msg("Waiting...\n");
	/* wait for an answer... Actually I don't listen to any events. just flush buffers and wait a few millis. */
	if(tcflush(fds, TCOFLUSH) == -1) {
		perror("Error sending cmd to TTY");
		return -6;
	}
	return 0;
}

int fda_write(struct fda_state* state, const unsigned char * buff, int n) {
	struct fda_fd *ptr=(struct fda_fd *)state->handle;
	int fds = ptr->fd;
	int w = -1;
	w = write(fds, buff, n);
	if(w == -1) {
		perror("Error sending cmd to TTY");
		return -6;
	}
	return w;
}

int fda_close(struct fda_state*state) {
	struct fda_fd *ptr=(struct fda_fd *)state->handle;
	int fds = ptr->fd;
	free(ptr);
	state->handle=NULL;

	/* close the serial port */
	if (close(fds) == -1) {
		/* error code goes here */
		perror("Error closing tty");
		return -11;
	}

	return 0;
}


