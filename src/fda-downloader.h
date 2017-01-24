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
#ifndef FDA_DOWNLOADER_H_
#define FDA_DOWNLOADER_H_

struct fda_state {
	void* handle;
	/*void *options;*/
    const unsigned char *tty_cmd;
    const char * tty_device;
    int cmd_set;
    char selected_cmd;
    int data_size;
    unsigned char * data;
};

/**
 * Default TTY/COM device for each implementation
 */
extern const char *TTY_DEVICE;

/**
 * Initialize selected TTY/COM device
 */
extern int fda_init(struct fda_state*);

/**
 * Read 'n' chars from altimeter into 'buff'.
 *
 * Return num chars read or < 0 if an error occurred.
 */
extern int fda_read(struct fda_state*, unsigned char * buff, int n);

/**
 * Waits until data is ready
 *
 * Returns 0 if success
 */
extern int fda_wait(struct fda_state*);

/**
 * Write 'n' chars from 'buff' into altimeter.
 *
 * Return num chars written or < 0 if an error occurred.
 */
extern int fda_write(struct fda_state*, const unsigned char * buff, int n);

/**
 * Close TTY/COM device
 */
extern int fda_close(struct fda_state*);

/**
 * Helper functions
 */
extern void print_msg(const char *format, ...);
extern void flush_msgs();

#endif /* FDA_DOWNLOADER_H_ */
