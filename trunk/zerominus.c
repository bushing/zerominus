/*
 Copyright (c) 2010 Sven Peter <sven@fail0verflow.com>
 Copyright (c) 2010 Haxx Enterprises <bushing@gmail.com>
 All rights reserved.

 Redistribution and use in source and binary forms, with or
 without modification, are permitted provided that the following 
 conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation 
   and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
  THE POSSIBILITY OF SUCH DAMAGE.
 
*/


#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>

#include "gl.h"
#include "analyzer.h"
#include "vcd.h"

#define VERSION "0.1.0"
#define VENDOR_ID 0x0C12

// global variables \o/
static int g_memory_size = -1;
static int g_pre_trigger = -1;
static int g_channel = -1;
static int g_trigger_edge = -1;
static int g_trigger_count = -1;
static int g_trigger_level = -1;
static int g_freq = -1;
static int g_freq_scale = -1;
static int g_button_wait = 0;
static int g_double_mode = 0;
static int g_compression = 0;
static int g_vcd = 0;
static char *g_path = NULL;

static void show_help(const char *progname)
{
    printf("Usage: %s [OPTION]... <FILE>\n", progname);
	printf("Options:\n");
    printf("  -m, --memory-size=SIZE\tset memory size of device\n");
    printf("  -c, --channel=COUNT\t\tspecify channel count\n");
    printf("  -f, --freq=FREQ\t\tspecify sampling frequency (in MHz or KHz)\n");
    printf("  -p, --pre-trigger=n%%\t\tspecify pre-trigger (0%%-100%%)\n");
    printf("  -t, --trigger=CHAN:TYPE\tset triggering condition (type is hi, lo, posedge, negedge, anyedge)\n");
    printf("  -x, --filter=CHAN:LEVEL\tset a channel filter (e.g. a0:hi, c7:lo)\n");
    printf("  -b, --button\t\t\twait for button press before sampling\n");
    printf("  -r, --trigger-count=COUNT\tspecify trigger count\n");
    printf("  -d, --double\t\t\tenable \"double mode\"\n");
    printf("  -o, --compression\t\tenable compression\n");
    printf("  -v, --vcd\t\t\tuse VCD output format\n");
    printf("  -h, --help\t\t\tDisplay this usage info\n");
	printf("\n");
	printf("Samples will be output to FILE.\n");
}

static int get_channel(char arg)
{
	switch (arg) {
		case 'a':
		case 'A':
			return CHANNEL_A;
		case 'b':
		case 'B':
			return CHANNEL_B;
		case 'c':
		case 'C':
			return CHANNEL_C;
		case 'd':
		case 'D':
			return CHANNEL_D;
	}
	return -1;
}

enum {
	HIGH = 0,
	LOW,
	POSEDGE,
	NEGEDGE,
	ANYEDGE
};

static int parse_channel(char *arg, int *type, int *id)
{
	int len;
	int channel;
	int my_id;
	char *type_ptr = NULL;

	len = strlen(arg);
	if (len < 4)
		return -1;

	channel = get_channel(arg[0]);
	if (channel < 0)
		return -1;

	my_id = strtol(arg + 1, &type_ptr, 10);
	if (my_id < 0 || my_id > 7)
		return -1;
	if (type_ptr == NULL || *type_ptr != ':')
		return -1;

	*id = channel | my_id;
	if (*type_ptr++ == 0)
		return -1;

	if (strcasecmp(type_ptr, "hi") == 0)
		*type = HIGH;
	else if (strcasecmp(type_ptr, "lo") == 0)
		*type = LOW;
	else if (strcasecmp(type_ptr, "posedge") == 0)
		*type = POSEDGE;
	else if (strcasecmp(type_ptr, "negedge") == 0)
		*type = NEGEDGE;
	else if (strcasecmp(type_ptr, "anyedge") == 0)
		*type = ANYEDGE;
	else
		return -1;
	return 0;
}

static int parse_trigger(char *arg)
{
	int id;
	int type;
	
	if (arg == NULL)
		goto parse_trigger_fail;

	if (parse_channel(arg, &type, &id) < 0)
	       goto parse_trigger_fail;	

	if (type == HIGH) {
		type = TRIGGER_HIGH;
	} else if (type == LOW) {
		type = TRIGGER_LOW;
	} else {
		if (type == POSEDGE)
			type = TRIGGER_POSEDGE;
		else if (type == NEGEDGE)
			type = TRIGGER_NEGEDGE;
		else if (type == ANYEDGE)
			type = TRIGGER_ANYEDGE;
		else
			goto parse_trigger_fail;	
		
		if (g_trigger_edge != -1) {
			fprintf(stderr, "Only one trigger on a signal edge is possible!\n");
			goto parse_trigger_fail;
		}
		g_trigger_edge = 1;
	}
	
	g_trigger_level = 1;
	analyzer_add_trigger(id, type);
	return 0;
	
parse_trigger_fail:
	fprintf(stderr, "Invalid trigger: %s. Try -h for help.\n", arg);
	return -1;
}

static int parse_filter(char *arg)
{
	int id;
	int type;
	
	if (arg == NULL)
		goto parse_filter_fail;

	if (parse_channel(arg, &type, &id) < 0)
		goto parse_filter_fail;	

	if (type == HIGH)
		type = FILTER_HIGH;
	else if (type == LOW)
		type = FILTER_LOW;
	else
		goto parse_filter_fail;	

	analyzer_add_filter(id, type);
	return 0;

parse_filter_fail:
	fprintf(stderr, "Invalid filter: %s. Try -h for help.\n", arg);
	return -1;
}

static int parse_freq(const char *arg)
{
	int f = 0;
	int s = FREQ_SCALE_HZ;
	char *scale = NULL;

	if (arg == NULL)
		goto parse_freq_fail;

	f = strtol(arg, &scale, 10);
	if (f == 0)
		goto parse_freq_fail;

	if (scale != NULL && strcasecmp(scale, "MHz") == 0)
		s = FREQ_SCALE_MHZ;
	else if (scale != NULL && strcasecmp(scale, "KHz") == 0)
		s = FREQ_SCALE_KHZ;

	if (s == FREQ_SCALE_HZ && f >= 1000) {
		s = FREQ_SCALE_KHZ;
		f = f/1000;
	}
	if (s == FREQ_SCALE_KHZ && f >= 1000) {
		s = FREQ_SCALE_MHZ;
		f = f/1000;
	}
	if (s == FREQ_SCALE_MHZ && f > 200) {
		fprintf(stderr, "Frequency is too high.\n");
		goto parse_freq_fail;
	}

	g_freq = f;
	g_freq_scale = s;
	return 0;

parse_freq_fail:
	fprintf(stderr, "Invalid frequency: %s. Try -h.\n", arg);
	return -1;
}

static int parse_memory_size(char *arg)
{
	if (arg == 0)
		goto parse_memory_size_fail;

	if (strcasecmp(arg, "2K") == 0)
		g_memory_size = MEMORY_SIZE_8K;
	else if (strcasecmp(arg, "16K") == 0)
		g_memory_size = MEMORY_SIZE_64K;
	else if (strcasecmp(arg, "32K") == 0)
		g_memory_size = MEMORY_SIZE_128K;
	else if (strcasecmp(arg, "128K") == 0)
		g_memory_size = MEMORY_SIZE_512K;
	else
		goto parse_memory_size_fail;

	return 0;

parse_memory_size_fail:
	fprintf(stderr, "Invalid memory size: %s.  Supported values: 2K, 16K, 32K, 128K\n", arg);
	return -1;
}

static unsigned int get_memory_size(int type)
{
	if (type == MEMORY_SIZE_8K)
		return 8*1024;
	else if (type == MEMORY_SIZE_64K)
		return 64*1024;
	else if (type == MEMORY_SIZE_128K)
		return 128*1024;
	else if (type == MEMORY_SIZE_512K)
		return 512*1024;
	else
		return 0;
}

static int remove_channel(unsigned char *memory, unsigned int len, unsigned int channel)
{
	if (channel == 32)
		return len;

	int i = 0;
	int j = 0;
	while (i < (int)len) {
		if (channel == 16) {
			memory[j] = memory[i];
			memory[j+1] = memory[i+1];
			i += 4;
			j += 2;
		} else if (channel == 8) {
			memory[j] = memory[i];
			j++;
			i += 4;
		}
	}

	if (channel == 16)
		len >>= 1;
	else if (channel == 8)
		len >>= 2;

	return len;
}

static int parse_num(const char *name, char *arg)
{
	int res;

	if (optarg == NULL) {
		fprintf(stderr, "No %s specified!\n", name);
		return -1;
	}

	res = strtol(arg, NULL, 10);
	if (res == 0) {
		fprintf(stderr, "Invalid number for %s: %s\n", name, arg);
		return -1;
	}

	return res;	
}

static int parse_args(int argc, char *argv[])
{
	int c, i;
	const char *progname = argv[0];
	const struct option opts[] = {
		{ "help", no_argument, 0, 'h' },
		{ "memory-size", required_argument, 0, 'm' },
		{ "channel", required_argument, 0, 'c' },
		{ "freq", required_argument, 0, 'f' },
		{ "pre-trigger", required_argument, 0, 'p' },
		{ "trigger", required_argument, 0, 't' },
		{ "filter", required_argument, 0, 'x' },
		{ "button", no_argument, 0, 'b'},
		{ "trigger-count", required_argument, 0, 'r'},
		{ "double", no_argument, 0, 'd'},
		{ "compression", no_argument, 0, 'o'},
		{ "vcd", no_argument, 0, 'v'},
		{ 0, 0, 0, 0 }
	};

	c = 0;
	while (c >= 0) {
		c = getopt_long(argc, argv, "-m:c:f:p:t:r:x:dbhov", opts, &i);
		switch (c) {
			case 'h':
				show_help(progname);
				return -1;
			case 'm':
				if (parse_memory_size(optarg))
					return -1;
				break;
			case 'c':
				g_channel = parse_num("channel count", optarg);
				if (g_channel < 0)
					return -1;
				break;
			case 'p':
				g_pre_trigger = parse_num("pre trigger", optarg);
				if (g_pre_trigger < 0)
					return -1;
				break;
			case 't':
				if (parse_trigger(optarg) < 0)
					return -1;
				break;
			case 'x':
				if (parse_filter(optarg) < 0)
					return -1;
				break;
			case 'f':
				if (parse_freq(optarg) < 0)
					return -1;
				break;
			case 'b':
				g_button_wait = 1;
				break;
			case 'r':
				g_trigger_count = parse_num("trigger count", optarg);
				if (g_trigger_count < 0)
					return -1;
				break;
			case 'd':
				g_double_mode = 1;
				break;
			case 'o':
				g_compression = 1;
				break;
			case 'v':
				g_vcd = 1;
				break;
			case 1:
				if (g_path != NULL) {
					fprintf(stderr, "More than one filename specified: %s and %s\n", g_path, optarg);
					return -1;
				}
				g_path = strdup(optarg);
				break;
		}
	}
	return 0;
}

static int validate_settings(void)
{
	if (g_path == NULL) {
		fprintf(stderr, "No filename specified. Try -h for help.\n");
		return -1;
	}
	
	if (g_memory_size == -1)
		g_memory_size = MEMORY_SIZE_512K;
	
	if (g_pre_trigger == -1)
		g_pre_trigger = 0;

	if (g_pre_trigger > 100) {
		fprintf(stderr, "Invalid pre-trigger %d%% > 100%%\n", g_pre_trigger);
		return -1;
	}

	if (g_double_mode && g_channel != -1) {
		fprintf(stderr, "ERROR: Do not specify a number of channels when double mode is activated!\n");
		return -1;
	}

	if (g_double_mode && g_compression) {
		fprintf(stderr, "ERROR: please select either compression or double mode\n");
		return -1;
	}

	if (g_channel == -1)
		g_channel = 32;
	
	if (g_freq == -1 || g_freq_scale == -1) {
		g_freq = 100;
		g_freq_scale = FREQ_SCALE_MHZ;
	}

	if (g_trigger_count == -1)
		g_trigger_count = 1;

	if (g_channel != 8 && g_channel != 16 && g_channel != 32) {
		fprintf(stderr, "Invalid number of channels: %d\n", g_channel);
		return -1;
	}

	if (g_trigger_level == -1 && g_pre_trigger != 0) {
		fprintf(stderr, "WARNING: pre_trigger %d%% does not make sense without a trigger. using 0 instead.\n", g_pre_trigger);
		g_pre_trigger = 0;
	}

	return 0;
}

static void write_file(unsigned char *memory, unsigned int len)
{
	FILE *fp;
	int r;

	fp = fopen(g_path, "w");

	if (!fp) {
		perror("Failed to open output file");
		return;
	}
	
	r = fwrite(memory, len, 1, fp);
	if (r != 1)
		perror("fwrite() failed");
	else
		printf("wrote %d bytes to %s!\n", len, g_path);

	fclose(fp);
}

int main(int argc, char *argv[])
{
	int r = 0;
	unsigned char *memory = NULL;	
	unsigned char *memory_real = NULL;

	printf("zerominus " VERSION "\n");
	
	r = gl_open(VENDOR_ID);
	if (r < 0) {
		fprintf(stderr, "Zeroplus device not found (%d)!\n", r);
		return -1;
	}

	if (parse_args(argc, argv) < 0)
		goto fail;

	if (validate_settings() < 0)
		goto fail;	
	
	analyzer_reset();
	analyzer_initialize();

	if (g_button_wait) {
		printf("Waiting for button press\n");
		analyzer_wait_button();
	}

	analyzer_set_memory_size(g_memory_size);
	analyzer_set_freq(g_freq, g_freq_scale);
	analyzer_set_trigger_count(g_trigger_count);
	analyzer_set_ramsize_trigger_address((((100 - g_pre_trigger) * get_memory_size(g_memory_size)) / 100) >> 2);

	if (g_double_mode == 1)
		analyzer_set_compression(COMPRESSION_DOUBLE);
	else if (g_compression == 1)
		analyzer_set_compression(COMPRESSION_ENABLE);
	else
		analyzer_set_compression(COMPRESSION_NONE);

	analyzer_configure();
	analyzer_start();

	printf("Waiting for data\n");
	analyzer_wait_data();	

	printf ("Stop address    = 0x%x\n", analyzer_get_stop_address());
	printf ("Now address     = 0x%x\n", analyzer_get_now_address());
	printf ("Trigger address = 0x%x\n", analyzer_get_trigger_address());
	printf("ID = %08x\n", analyzer_read_id());

	memory = malloc(get_memory_size(g_memory_size));
	if (!memory) {
		perror("Failed to allocate memory");
		goto fail;
	}

	memset(memory, 0xaa, get_memory_size(g_memory_size));
	
	int res = analyzer_read(memory, get_memory_size(g_memory_size));

	if (res < 0) {
		fprintf(stderr, "Reading data failed with %d.\n", res);
		goto fail;
	}

	if (g_compression) {
		memory_real = malloc(get_memory_size(g_memory_size) * 256);
		if (memory_real == NULL) {
			perror("Failed to allocate memory for decompressed data");
			goto fail;
		}

		printf("decompressing data...\n");
		res = analyzer_decompress(memory, get_memory_size(g_memory_size), memory_real, get_memory_size(g_memory_size) * 256);
		
		res = remove_channel(memory_real, (unsigned int)res, g_channel);
		free(memory);
		memory = memory_real;
		memory_real = NULL;
	} else {
		res = remove_channel(memory, (unsigned int)res, g_channel);
	}

	if (g_vcd)
		vcd_from_ram(g_path, memory, (unsigned int)res, g_channel, g_freq, g_freq_scale);
	else
		write_file(memory, (unsigned int)res);

	free(memory);
	if (memory_real != NULL)
		free(memory_real);
	free(g_path);

	analyzer_reset();
	gl_close();
	
	return 0;

fail:
	if (g_path)
		free(g_path);
	if (memory)
		free(memory);
	if (memory_real != NULL)
		free(memory_real);
	gl_close();
	return -1;
}
