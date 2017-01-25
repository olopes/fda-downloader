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
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <stdarg.h>
#include <assert.h>
#include "fda-downloader.h"

// NOTE: Compile with -lm to include math functions


/* function declarations */

/**
 * Print usage message
 */
static void print_usage(const char *, ...);

/**
 * Send selected command to altimeter
 */
static int fda_send_cmd(struct fda_state* state);

/**
 * Calculate altitude from pressure and temperature readings (hypsometric equation)
 */
static double calc_altitude(long pressure, short temperature);

/**
 * Print received header
 */
static void print_data(unsigned char const * b, int size);

/**
 * Write contents to FDA/HKA file
 */
static int save_fda(struct fda_state* state, const char *file, const char *);

/**
 * Write contents to CSV file
 */
static int save_dlm(struct fda_state* state, const char *file, const char * dlm);

/**
 * Release all allocated resources and closes TTY/COM device
 */
static int fda_dispose(struct fda_state* state);

/** UNIT CONVERSION FUNCTIONS */

/**
 * Convert meters to feets
 */
static double m_to_ft(double h);

/**
 * Convert degree Celsius to Fahrenheit
 */
static double c_to_F(double t);

/**
 * Convert Pascal to PSI (Pound Square Inch)
 */
static double pa_to_psi(double p);

/**
 * Identity function: returns the same value as the parameter
 */
static double identity(double i);

#define FDA_BUF_SIZE 4096
#define FDA_HEADER_SIZE 8
#define FDA_CMD_SIZE 7
#define FDA_FORMAT_FDA "fda"
#define FDA_FORMAT_DLM "dlm"

/* command list */
/* upload altimeter contents */
 unsigned char const cmd_upload[FDA_CMD_SIZE] = {0x0f, 0xda, 0x10, 0x00, 0xca, 0x00, 0x00};
/* set record frequency to 1Hz */
static unsigned char const cmd_set1hz[FDA_CMD_SIZE] = {0x0f, 0xda, 0x10, 0x00, 0xcb, 0x00, 0x00};
/* set record frequency to 2Hz */
static unsigned char const cmd_set2hz[FDA_CMD_SIZE] = {0x0f, 0xda, 0x10, 0x00, 0xcb, 0x00, 0x01};
/* set record frequency to 4Hz */
static unsigned char const cmd_set4hz[FDA_CMD_SIZE] = {0x0f, 0xda, 0x10, 0x00, 0xcb, 0x00, 0x02};
/* set record frequency to 8Hz */
static unsigned char const cmd_set8hz[FDA_CMD_SIZE] = {0x0f, 0xda, 0x10, 0x00, 0xcb, 0x00, 0x03};
/* erase data */
static unsigned char const cmd_erased[FDA_CMD_SIZE] = {0x0f, 0xda, 0x10, 0x00, 0xcc, 0x00, 0x00};


static int verbose = 0;

/* reference sea level pressure in Pa */
static const double seaLevelPressure=101325;
/* reference sea level temperature in K (15C) */
static const double seaLevelTemperature=288.15;
/* Specific gas constant for dry air in N.m/(mol.K) */
static const double R=8.3144598;
/* Gravitational acceleration in m/s2 */
static const double g=9.80665;
/* Molar mass of Earth's air in Kg/mol */
static const double M=0.0289644;
/* standard temperature lapse rate [K/m] = -0.0065 [K/m] */
static const double L=-0.0065 ; // K/m


static double(*f_pressure)(double)     = &identity;
static double(*f_temperature)(double)  = &identity;
static double(*f_height)(double)       = &identity;

/**
 * Entry point
 */
int main(int argc, char** argv) {
	/* getopt_long stores the option index here. */
	int option_index = 0;
	int c;
	int retval;
    struct fda_state state, *statep=&state;
	const char *out_file=NULL, *dlm=NULL, *out_format=NULL, *cmd_param=NULL;
	int (*f_save)(struct fda_state*,const char*, const char*)=NULL;
	
    /* prepare state */
    memset(statep, 0, sizeof(state));
    state.tty_device=TTY_DEVICE;

    while(1) {
    	static struct option long_options[] =
    	{
    		/* These options don't set a flag.
			Their indices must be used in order to distinguish them. */
    		{"upload",    required_argument, 0, 'u'},
			{"erase",     no_argument,       0, 'e'},
			{"setup" ,    required_argument, 0, 's'},
			{"tty",       required_argument, 0, 't'},
			{"verbose",   no_argument,       0, 'v'},
			{"format",    required_argument, 0, 'f'},
			{"delimiter", required_argument, 0, 'd'},
			{"imperial" , no_argument,       0, 'i'},
			{0, 0, 0, 0}
    	};

    	c = getopt_long (argc, argv, "u:es:t:vf:d:i", long_options, &option_index);

    	/* Detect the end of the options. */
    	if (c == -1)
    		break;

    	switch(c) {
    	case 'u':
    	case 's':
    		cmd_param=optarg;
    		/* no break */
    	case 'e':
    		state.cmd_set++;
    		state.selected_cmd=c;
    		break;
    	case 't':
    		state.tty_device=optarg;
    		break;
    	case 'v':
    		verbose=1;
    		break;
    	case 'f':
    		out_format=optarg;
    		break;
    	case 'd':
    		dlm=optarg;
    		break;
		case 'i':
			/* use imperial units */
			f_pressure     = &pa_to_psi;
			f_temperature  = &c_to_F;
			f_height       = &m_to_ft;
			break;

    	case '?':
    		/* already handled? */
    		break;

    	default:
    		return 1;
    	}
    }

    if(state.cmd_set != 1 || optind < argc) {
    	print_usage(NULL);
    	return 1;
    }

    /* print_msg("Command: %c; Param: %s; TTY: %s\n", selected_cmd, param, tty_device); */

    /* this could be better written, but I don't care. :-P */

    /* decide what command should be sent to the altimeter */
    if(state.selected_cmd == 'e') {
    	state.tty_cmd=cmd_erased;
    } else if(state.selected_cmd == 'u') {
    	state.tty_cmd=cmd_upload;
		out_file = cmd_param;
		// validate output format
		if(out_format == NULL || !strcmp("fda",out_format) || !strcmp("hka",out_format)) {
			f_save = &save_fda;
			print_msg("FDA output format selected\n");
		} else if(!strcmp("dlm",out_format)) {
			f_save=&save_dlm;
			if(dlm == NULL) dlm=",";
			print_msg("DLM output format selected. Delimiter: '%s'\n", dlm);
		} else {
			print_usage("Invalid file format: %s\n", out_format);
			return 15;
		}
    } else if(state.selected_cmd == 's') {
		if(!strcmp("1",cmd_param)) {
			state.tty_cmd=cmd_set1hz;
		} else if(!strcmp("2",cmd_param)) {
			state.tty_cmd=cmd_set2hz;
		} else if(!strcmp("4",cmd_param)) {
			state.tty_cmd=cmd_set4hz;
		} else if(!strcmp("8",cmd_param)) {
			state.tty_cmd=cmd_set8hz;
		} else {
			print_usage("Invalid sample rate: %s\n", cmd_param);
		}
	}

    // initialize device
    retval=fda_init(statep);
    if(retval) {
    	// use perror?
    	print_msg("Error initializing device\n");
    	return retval;
    }


    retval = fda_send_cmd(statep);
    if(retval) {
    	// use perror?
    	print_msg("Error sending command to device\n");
    }
    retval = fda_close(statep);
    if(retval) {
    	// use perror?
    	print_msg("Error closing device\n");
    	return retval;
    }

    if(state.selected_cmd == 'u' && state.data_size > 0) {
    	retval = (*f_save)(statep, out_file, dlm);
    }

    retval = fda_dispose(statep);
    print_msg("Done!\n");

	return 0;
}

void print_usage(const char * err_msg, ...) {
	if(err_msg) {
		va_list args;
		va_start(args, err_msg);
		vprintf(err_msg, args);
		va_end(args);
	}
	
    /* print usage */
    printf("Usage: fda-downloader [OPTIONS] <cmd>\n");
    printf("<cmd> is one of:\n");
    printf("    -u, --upload <file>     Retrieve contents from altimeter\n");
    printf("    -e, --erase             Erase altimeter contents\n");
    printf("    -s, --setup <rate>      Set altimeter sample rate in Hz.\n");
    printf("                            Possible values are: 1, 2, 4 or 8\n");
    printf("Options are:\n");
    printf("    -f, --format <fmt>      Set output format. Can be either 'fda' or 'dlm'\n");
    printf("    -d, --delimiter <delim> Use <delim> as delimiter for 'dlm' files.\n");
    printf("    -t, --tty <device>      Serial device to use.\n");
    printf("                            Defaults to %s\n", TTY_DEVICE);
    printf("    -v, --verbose           Enable verbose mode\n");
}

static void print_data(unsigned char const * buf, int size) {
	int i;
	char str[5*size+1], *p=str;
	if(!verbose) return;
	for(i = 0; i < size; i++)
		p += sprintf(p, "0x%02x ", buf[i]);
	str[5*size-1]='\0';
	print_msg("%s\n",str);
}

static int fda_send_cmd(struct fda_state* state) {
	int w, r, n, total, done;
	unsigned char *buf;
	unsigned char b[FDA_HEADER_SIZE+4];

	// Send specified text (remaining command line arguments)
	print_msg("Sending bytes...\n");
	print_data(state->tty_cmd, FDA_CMD_SIZE);
	w = fda_write(state, state->tty_cmd, FDA_CMD_SIZE);
	if(w < 0)
	{
		print_msg("Error sending command to %s\n", state->tty_device);
		return 6;
	}
	print_msg("%d bytes written\n", w);

	// Wait for answer
	if(fda_wait(state)) {
		print_msg("Error waiting for RX eventv");
		return 7;
	}
	
	// check answer
	buf = b;
	n = 0;
	while (n < FDA_HEADER_SIZE) {
		r = fda_read(state, buf+n, FDA_HEADER_SIZE - n);
		if (r < 0) {
			print_msg("Error reading %s\n", state->tty_device);
			return 8;
		}
		n+=r;
	}

	if(n != FDA_HEADER_SIZE) {
		print_msg("Error condition detected: number of bytes read don't match header size. leaving...\n");
		return 9;
	}

	/* print command to be sent just for debug purposes */
	print_data(buf, FDA_HEADER_SIZE);

	// XXX About erase, maybe we have to keep reading until a good answer is received?
	
	/* check signature */
	if(buf[0]!=0x07 || memcmp(buf+1, state->tty_cmd, FDA_CMD_SIZE)) {
		print_msg("Invalid signature header found.\n");
		return 10;
	}

	// if upload contents was requested, continue to read altimeter data and output to file
	state->data_size=0;
	state->data=NULL;
	if(state->selected_cmd == 'u') {

		int upl_header_size=FDA_HEADER_SIZE+4; /* extra bytes for data size */
		/* handle upload - send altimeter output to file */
		/* n is not set to zero in order to keep upload header as one */
		while (n < upl_header_size) {
			r = fda_read(state, buf+n, 4);
			if (r < 0) {
				print_msg("Error reading %s\n", state->tty_device);
				return 11;
			}
			n+=r;
		}

		total = buf[9]-2; /* tricky one! */
		total = total<<8 | buf[10];
		total = total<<8 | buf[11];
		/* add header size */
		total += upl_header_size;

		// total = ((buf[9]-2)*256 + buf[10])*256+buf[11]+upl_header_size;
		done=upl_header_size;
		print_msg("total bytes: %d\n", total);
		flush_msgs();

		if(total <= done) {
			print_msg("No data available, nothing to do!\n");
			flush_msgs();
		} else {
		
			/* realloc buffer to hold all data */
			buf = (unsigned char*) malloc(total*sizeof(unsigned char));
			state->data = buf;
			state->data_size=total;
			/* copy bytes previously read */
			memcpy(buf, b, sizeof(b));
			buf += sizeof(b);
			
			/* read remaining data */
			while (n < total) {
				r = fda_read(state, buf, FDA_BUF_SIZE);
				if (r < 0) {
					print_msg("Error reading %s\n", state->tty_device);
					return 12;
				} else if (r == 0) {
					/* no data, nothing to do, loop */
					print_msg("no data...\n");
				} else {
					buf += r;
					n += r;
					printf("%d -> %u/%u (%u%%)\n", r, n,total,n*100/total);
					flush_msgs();
				}
			}
		}
	}

	return 0;
}

void print_msg(const char *format, ...) {
	if(verbose) {
		va_list args;
		va_start(args, format);
		// enable if verbose selected
		vfprintf(stderr, format, args);
		va_end(args);
	}
}

void flush_msgs()  {
	fflush(stderr);
}


static int save_fda(struct fda_state* state, const char *file, const char *dlm) {
	FILE *fdf;
	int n, total, w;
	unsigned char *buf;

	if(state->data_size <= 0) {
		print_msg("No data to write.\n");
		return -1;
	}

	fdf = fopen(file, "wb");
	if(!fdf) {
		perror("Error opening output file");
		return -2;
	}

	n = 0;
	total = state->data_size;
	buf = state->data;
	while( n < total ) {
		w = fwrite(buf+n, sizeof(unsigned char), total-n, fdf);
		if(w < 0) {
			print_msg("Error writing to file %s (%d)\n", file, w);
			break;
		}
		/* update counters and pointers */
		n += w;
	}

	fflush(fdf);
	
	return fclose(fdf);
}

#define FDA_SAMPLE_SIZE 4
static int save_dlm(struct fda_state* state, const char *file, const char *dlm) {
	FILE *fdf;
	int n, st, freq, size;
	unsigned char *sample, empty[FDA_SAMPLE_SIZE];
	long pressure;
	short temperature;
	double altitude, ts, tIncr, press_conv, temp_conv, alti_conv;
	memset(&empty, 0xff, sizeof(unsigned char)*FDA_SAMPLE_SIZE);

	if(state->data_size <= 0) {
		print_msg("No data to write.\n");
		return -1;
	}
	
	fdf = fopen(file, "w");
	if(!fdf) {
		perror("Error opening output file");
		return -2;
	}
	print_msg("File \"%s\"open, start data output with delimiter=%s\n", file, dlm);
	flush_msgs();

	/* skip header and go straight to the first observation */
	/* reposition buffer to samples location */
	st = 1;
	size = state->data_size;
	n = FDA_HEADER_SIZE+FDA_SAMPLE_SIZE;
	sample = state->data+n;
	for(; n < size; n+=FDA_SAMPLE_SIZE, sample+=FDA_SAMPLE_SIZE) {
		if(!memcmp(sample, empty, sizeof(unsigned char)*FDA_SAMPLE_SIZE)) {
			st++;
			if(st < 2)
				fprintf(fdf, "\n");
			print_msg("Empty sample, output an empty line\n");
		} else if(st) {
			st = 0;
			fprintf(fdf, "TIME%sPRESSURE%sTEMPERATURE%sALTITUDE\n",dlm,dlm,dlm);
			freq = 1 << sample[3];
			ts = 0.0;
			tIncr = 1.0/(double)freq;
			print_msg("First record, output header line. freq=%d; tIncr=%.3f\n", freq, tIncr);
		} else {
			temperature = sample[0];
			pressure = sample[1];
			pressure = pressure<<8 | sample[2];
			pressure = pressure<<8 | sample[3];
			altitude = calc_altitude(pressure, temperature);
			
			press_conv=(*f_pressure)(pressure);
			temp_conv=(*f_temperature)(temperature);
			alti_conv=(*f_height)(altitude);
			fprintf(fdf, "%.3f%s%.2f%s%.2f%s%.2f\n",ts,dlm,press_conv,dlm,temp_conv,dlm,alti_conv);
			
			// debug message
			print_msg("Regular record, output record line. ts=%.3f; pressure=%ld (%.2f) ; temperature=%hd (%.2f); altitude=%.2f (%.2f)\n",ts,pressure,press_conv,temperature,temp_conv,altitude,alti_conv);
			
			ts += tIncr;
		}
		// flush_msgs();
	}
	
	fflush(fdf);

	print_msg("Output complete. %d samples written. Closing file...\n", size);
	flush_msgs();
	return fclose(fdf);

}

static int fda_dispose(struct fda_state* state) {
	free(state->data);
	state->data=NULL;
	state->data_size=0;
	return 0;
}

double calc_altitude(long pressure, short temp) {
	const double hb = 0;
	double h = hb + (seaLevelTemperature/L) * (pow(pressure/seaLevelPressure, (-R*L)/(g*M)) - 1.0);
	// should consider M?
	return h;
}

static double identity(double h) {
	return h;
}

static double m_to_ft(double h) {
	// 1 m = 3.28 ft
	return h*3.28;
}

static double c_to_F(double t) {
	// multiply by 1.8 (or 9/5) and add 32
	return t*9/5+32;
}

static double pa_to_psi(double p) {
	// psi equals to 6894.75729 Pa.
	return p/6894.75729;
}

