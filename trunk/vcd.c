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
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "analyzer.h"

static const char *g_names[] = {
	"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
	"B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7",
	"C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7",
	"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
};


static char wire_symbol(unsigned int i)
{
	return '!' + i;
}

static const char *wire_name(unsigned int i)
{
	if (i > 31)
	       return NULL;	
	return g_names[i];
}

static void dumpchannel(FILE *f, unsigned int v, unsigned int offset)
{
	for (unsigned int i = 0; i < 8; i++) {
		if (v & 1)
			fputs("1", f);
		else
			fputs("0", f);
		v >>= 1;
		fprintf(f, "%c\n", wire_symbol(i + offset));
	}
}

static void dumpvars(FILE *f, const void **ram, unsigned int *len, unsigned int channel)
{
	unsigned int A, B, C, D;
	unsigned char *ptr = (unsigned char *)*ram;

	if (len == 0)
		return;

	A = B = C = D = 0;
	A = *ptr++;
	(*len)--;
	if (channel > 7) {
		B = *ptr++;
		(*len)--;
	}
	if (channel > 15) {
		C = *ptr++;
		(*len)--;
	}
	if (channel > 23) {
		D = *ptr++;
		(*len)--;
	}
	*ram = ptr;

	dumpchannel(f, A, 0);
	if (channel < 8)
		return;
	dumpchannel(f, B, 8);
	if (channel < 16)
		return;
	dumpchannel(f, C, 16);
	if (channel < 24)
		return;
	dumpchannel(f, D, 24);
}

static unsigned int get_sample(unsigned char *ptr, unsigned int channel)
{
	unsigned int res;
	assert(sizeof(res) >= 4);
	memcpy(&res, ptr, channel/8);
	return res;
}

static void dumpdata(FILE *f, const void **ram, unsigned int *len, unsigned int channel)
{
	unsigned int time = 0;
	unsigned int sample, old_sample;
	unsigned char *ptr;
	unsigned int first = 1;

	while (*len > 0) {
		ptr = (unsigned char *)*ram;
		sample = get_sample(ptr, channel);
		if (sample != old_sample || first == 1) {
			fprintf(f, "#%d\n", time);
			dumpvars(f, ram, len, channel);
			old_sample = sample;
		} else {
			*len -= (channel/8);
		}
		time++;
		first = 0;
	}

	// this is added to make sure that the length of the trace will be stored
	fprintf(f, "#%d\n", time);
	for (unsigned int i = 0; i < channel; i++)
		fprintf(f, "x%c\n", wire_symbol(i));
}

void vcd_from_ram(const char *path, const void *ram, unsigned int len, unsigned int channel, int freq, int scale)
{
	FILE *f = NULL;

	printf("writing VCD file (this might take a while)...\n");
	f = fopen(path, "w");
	if (f == NULL) {
		perror("Unable to open output file");
		return;
	}

	fputs("$date\n", f);
	// TODO
	fputs("$end\n", f);
	fputs("$version\n", f);
	fputs("zerominus\n", f);
	fputs("$end\n", f);
	fputs("$comment\n", f);
	fputs("ohai!\n", f);
	fputs("$end\n", f);
	
	if (scale == FREQ_SCALE_HZ)
		fprintf(f, "$timescale %d ms $end\n", 1000/freq);
	else if (scale == FREQ_SCALE_KHZ)
		fprintf(f, "$timescale %d us $end\n", 1000/freq);
	else if (scale == FREQ_SCALE_MHZ)
		fprintf(f, "$timescale %d ns $end\n", 1000/freq);

	fputs("$scope module logic $end\n", f);

	for (unsigned int i = 0; i < channel; i++)
		fprintf(f, "$var wire 1 %c %s $end\n", wire_symbol(i), wire_name(i));

	fputs("$upscope $end\n", f);
	fputs("$enddefinitions $end\n", f);
	fflush(f);
	dumpdata(f, &ram, &len, channel);

	printf("wrote VCD file!\n");
	fclose(f);
}
