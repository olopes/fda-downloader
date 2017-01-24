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
#include <stdio.h>
#include "fda-downloader.h"

/* API FUNCTIONS */
const char *TTY_DEVICE="dummy";

int fda_init(struct fda_state* state) {
	print_msg("Not implemented\n");
	return -1;
}

int fda_read(struct fda_state* state, unsigned char * buff, int n) {
	print_msg("Not implemented\n");
	return -1;
}

int fda_write(struct fda_state* state, const unsigned char * buff, int n) {
	print_msg("Not implemented\n");
	return -1;
}

int fda_wait(struct fda_state* state) {
	print_msg("Not implemented\n");
	return -1;
}

int fda_close(struct fda_state* state) {
	print_msg("Not implemented\n");
	return -1;
}
