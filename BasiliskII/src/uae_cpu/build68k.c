/*
 * UAE - The Un*x Amiga Emulator
 *
 * Read 68000 CPU specs from file "table68k" and build table68k.c
 *
 * Copyright 1995,1996 Bernd Schmidt
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>

#include "sysdeps.h"
#include "readcpu.h"

static FILE *tablef;
static int nextch = 0;

static void getnextch(void)
{
    do {
	nextch = fgetc(tablef);
	if (nextch == '%') {
	    do {
		nextch = fgetc(tablef);
	    } while (nextch != EOF && nextch != '\n');
	}
    } while (nextch != EOF && isspace(nextch));
}

static int nextchtohex(void)
{
    switch (isupper (nextch) ? tolower (nextch) : nextch) {
     case '0': return 0;
     case '1': return 1;
     case '2': return 2;
     case '3': return 3;
     case '4': return 4;
     case '5': return 5;
     case '6': return 6;
     case '7': return 7;
     case '8': return 8;
     case '9': return 9;
     case 'a': return 10;
     case 'b': return 11;
     case 'c': return 12;
     case 'd': return 13;
     case 'e': return 14;
     case 'f': return 15;
     default: abort();
    }
}

int main(int argc, char **argv)
{
    int no_insns = 0;

    printf ("#include \"sysdeps.h\"\n");
    printf ("#include \"readcpu.h\"\n");
    printf ("struct instr_def defs68k[] = {\n");
#ifdef WIN32
    tablef = fopen(argc > 1 ? argv[1] : "table68k","r");
    if (tablef == NULL) {
	fprintf(stderr, "table68k not found\n");
	exit(1);
    }
#else
    tablef = stdin;
#endif
    getnextch();
    while (nextch != EOF) {
	int cpulevel, plevel, sduse;
	int i;

	char patbits[16];
	char opcstr[256];
	int bitpos[16];
	int flagset[5], flaguse[5];
	char cflow;

	unsigned int bitmask,bitpattern;
	int n_variable;

	n_variable = 0;
	bitmask = bitpattern = 0;
	memset (bitpos, 0, sizeof(bitpos));
	for(i=0; i<16; i++) {
	    int currbit;
	    bitmask <<= 1;
	    bitpattern <<= 1;

	    switch (nextch) {
	     case '0': currbit = bit0; bitmask |= 1; break;
	     case '1': currbit = bit1; bitmask |= 1; bitpattern |= 1; break;
	     case 'c': currbit = bitc; break;
	     case 'C': currbit = bitC; break;
	     case 'f': currbit = bitf; break;
	     case 'i': currbit = biti; break;
	     case 'I': currbit = bitI; break;
	     case 'j': currbit = bitj; break;
	     case 'J': currbit = bitJ; break;
	     case 'k': currbit = bitk; break;
	     case 'K': currbit = bitK; break;
	     case 's': currbit = bits; break;
	     case 'S': currbit = bitS; break;
	     case 'd': currbit = bitd; break;
	     case 'D': currbit = bitD; break;
	     case 'r': currbit = bitr; break;
	     case 'R': currbit = bitR; break;
	     case 'z': currbit = bitz; break;
		 case 'E': currbit = bitE; break;
		 case 'p': currbit = bitp; break;
	     default: abort();
	    }
	    if (!(bitmask & 1)) {
		bitpos[n_variable] = currbit;
		n_variable++;
	    }

	    if (nextch == '0' || nextch == '1')
		bitmask |= 1;
	    if (nextch == '1')
		bitpattern |= 1;
	    patbits[i] = nextch;
	    getnextch();
	}

	while (isspace(nextch) || nextch == ':') /* Get CPU and privilege level */
	    getnextch();

	switch (nextch) {
	 case '0': cpulevel = 0; break;
	 case '1': cpulevel = 1; break;
	 case '2': cpulevel = 2; break;
	 case '3': cpulevel = 3; break;
	 case '4': cpulevel = 4; break;
	 case '5': cpulevel = 5; break;
	 default: abort();
	}
	getnextch();

	switch (nextch) {
	 case '0': plevel = 0; break;
	 case '1': plevel = 1; break;
	 case '2': plevel = 2; break;
	 case '3': plevel = 3; break;
	 default: abort();
	}
	getnextch();

	while (isspace(nextch))                   /* Get flag set information */
	    getnextch();

	if (nextch != ':')
	    abort();

	for(i = 0; i < 5; i++) {
	    getnextch();
	    switch(nextch){
	     case '-': flagset[i] = fa_unset; break;
	     case '0': flagset[i] = fa_zero; break;
	     case '1': flagset[i] = fa_one; break;
	     case 'x': flagset[i] = fa_dontcare; break;
	     case '?': flagset[i] = fa_unknown; break;
	     default: flagset[i] = fa_set; break;
	    }
	}

	getnextch();
	while (isspace(nextch))
	    getnextch();

	if (nextch != ':')                        /* Get flag used information */
	    abort();

	for(i = 0; i < 5; i++) {
	    getnextch();
	    switch(nextch){
	     case '-': flaguse[i] = fu_unused; break;
	     case '?': flaguse[i] = fu_unknown; break;
	     default: flaguse[i] = fu_used; break;
	    }
	}

	getnextch();
	while (isspace(nextch))
	    getnextch();

	if (nextch != ':')                        /* Get control flow information */
	    abort();
	
	cflow = 0;
	for(i = 0; i < 2; i++) {
		getnextch();
		switch(nextch){
		 case '-': break;
		 case 'R': cflow |= fl_return; break;
		 case 'B': cflow |= fl_branch; break;
		 case 'J': cflow |= fl_jump; break;
		 case 'T': cflow |= fl_trap; break;
		 default: abort();
		}
	}
	
	getnextch();
	while (isspace(nextch))
	    getnextch();

	if (nextch != ':')                        /* Get source/dest usage information */
	    abort();

	getnextch();
	sduse = nextchtohex() << 4;
	getnextch();
	sduse |= nextchtohex();

	getnextch();
	while (isspace(nextch))
	    getnextch();

	if (nextch != ':')
	    abort();

	fgets(opcstr, 250, tablef);
	getnextch();
	{
	    int j;
	    /* Remove superfluous spaces from the string */
	    char *opstrp = opcstr, *osendp;
	    int slen = 0;

	    while (isspace(*opstrp))
		opstrp++;

	    osendp = opstrp;
	    while (*osendp) {
		if (!isspace (*osendp))
		    slen = osendp - opstrp + 1;
		osendp++;
	    }
	    opstrp[slen] = 0;

	    if (no_insns > 0)
		printf(",\n");
	    no_insns++;
	    printf("{ %d, %d, {", bitpattern, n_variable);
	    for (j = 0; j < 16; j++) {
		printf("%d", bitpos[j]);
		if (j < 15)
		    printf(",");
	    }
	    printf ("}, %d, %d, %d, { ", bitmask, cpulevel, plevel);
	    for(i = 0; i < 5; i++) {
		printf("{ %d, %d }%c ", flaguse[i], flagset[i], i == 4 ? ' ' : ',');
	    }
	    printf("}, %d, %d, \"%s\"}", cflow, sduse, opstrp);
	}
    }
    printf("};\nint n_defs68k = %d;\n", no_insns);
    fflush(stdout);
    return 0;
}
