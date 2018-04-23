/*
 * UAE - The Un*x Amiga Emulator
 *
 * cpuopti.c - Small optimizer for cpu*.s files
 *             Based on work by Tauno Taipaleenmaki
 *
 * Copyright 1996 Bernd Schmidt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "sysdeps.h"

struct line {
    struct line *next, *prev;
    int delet;
    char *data;
};

struct func {
    struct line *first_line, *last_line;
    int initial_offset;
};

static void oops(void)
{
    fprintf(stderr, "Don't know how to optimize this file.\n");
	exit(1);
}

static char * match(struct line *l, const char *m)
{
    char *str = l->data;
    int len = strlen(m);
    while (isspace(*str))
	str++;

    if (strncmp(str, m, len) != 0)
	return NULL;
    return str + len;
}

static int insn_references_reg (struct line *l, char *reg)
{
    if (reg[0] != 'e') {
	fprintf(stderr, "Unknown register?!?\n");
	exit(1);
    }
    if (strstr (l->data, reg) != 0)
	return 1;
    if (strstr (l->data, reg+1) != 0)
	return 1;
    if (strcmp (reg, "eax") == 0
	&& (strstr (l->data, "%al") != 0 || strstr (l->data, "%ah") != 0))
	return 1;
    if (strcmp (reg, "ebx") == 0
	&& (strstr (l->data, "%bl") != 0 || strstr (l->data, "%bh") != 0))
	return 1;
    if (strcmp (reg, "ecx") == 0
	&& (strstr (l->data, "%cl") != 0 || strstr (l->data, "%ch") != 0))
	return 1;
    if (strcmp (reg, "edx") == 0
	&& (strstr (l->data, "%dl") != 0 || strstr (l->data, "%dh") != 0))
	return 1;
    return 0;
}

static void do_function(struct func *f)
{
    int v;
    int pops_at_end = 0;
    struct line *l, *l1, *fl, *l2;
    char *s, *s2;
    int in_pop_area = 1;

    f->initial_offset = 0;

    l = f->last_line;
    fl = f->first_line;

    if (match(l,".LFE"))
	l = l->prev;
    if (!match(l,"ret"))
	oops();

    while (!match(fl, "op_"))
	fl = fl->next;
    fl = fl->next;

    /* Try reordering the insns at the end of the function so that the
     * pops are all at the end. */
    l2 = l->prev;
    /* Tolerate one stack adjustment */
    if (match (l2, "addl $") && strstr(l2->data, "esp") != 0)
	l2 = l2->prev;
    for (;;) {
	char *forbidden_reg;
	struct line *l3, *l4;

	while (match (l2, "popl %"))
	    l2 = l2->prev;

	l3 = l2;
	for (;;) {
	    forbidden_reg = match (l3, "popl %");
	    if (forbidden_reg)
		break;
	    if (l3 == fl)
		goto reordered;
	    /* Jumps and labels put an end to our attempts... */
	    if (strstr (l3->data, ".L") != 0)
		goto reordered;
	    /* Likewise accesses to the stack pointer... */
	    if (strstr (l3->data, "esp") != 0)
		goto reordered;
	    /* Function calls... */
	    if (strstr (l3->data, "call") != 0)
		goto reordered;
	    l3 = l3->prev;
	}
	if (l3 == l2)
	    exit(1);
	for (l4 = l2; l4 != l3; l4 = l4->prev) {
	    /* The register may not be referenced by any of the insns that we
	     * move the popl past */
	    if (insn_references_reg (l4, forbidden_reg))
		goto reordered;
	}
	l3->prev->next = l3->next;
	l3->next->prev = l3->prev;
	l2->next->prev = l3;
	l3->next = l2->next;
	l2->next = l3;
	l3->prev = l2;
    }
reordered:

    l = l->prev;

    s = match (l, "addl $");
    s2 = match (fl, "subl $");

    l1 = l;
    if (s == 0) {
	char *t = match (l, "popl %");
	if (t != 0 && (strcmp (t, "ecx") == 0 || strcmp (t, "edx") == 0)) {
	    s = "4,%esp";
	    l = l->prev;
	    t = match (l, "popl %");
	    if (t != 0 && (strcmp (t, "ecx") == 0 || strcmp (t, "edx") == 0)) {
		s = "8,%esp";
		l = l->prev;
	    }
	}
    } else {
	l = l->prev;
    }

    if (s && s2) {
	int v = 0;
	if (strcmp (s, s2) != 0) {
	    fprintf (stderr, "Stack adjustment not matching.\n");
	    return;
	}

	while (isdigit(*s)) {
	    v = v * 10 + (*s) - '0';
	    s++;
	}

	if (strcmp (s, ",%esp") != 0) {
	    fprintf (stderr, "Not adjusting the stack pointer.\n");
	    return;
	}
	f->initial_offset = v;
	fl->delet = 3;
	fl = fl->next;
	l1->delet = 2;
	l1 = l1->prev;
	while (l1 != l) {
	    l1->delet = 1;
	    l1 = l1->prev;
	}
    }

    while (in_pop_area) {
	char *popm, *pushm;
	popm = match (l, "popl %");
	pushm = match (fl, "pushl %");
	if (popm && pushm && strcmp(pushm, popm) == 0) {
	    pops_at_end++;
	    fl->delet = l->delet = 1;
	} else
	    in_pop_area = 0;
	l = l->prev;
	fl = fl->next;
    }
    if (f->initial_offset)
	f->initial_offset += 4 * pops_at_end;
}

static void output_function(struct func *f)
{
    struct line *l = f->first_line;

    while (l) {
	switch (l->delet) {
	 case 1:
	    break;
	 case 0:
	    printf("%s\n", l->data);
	    break;
	 case 2:
	    if (f->initial_offset)
		printf("\taddl $%d,%%esp\n", f->initial_offset);
	    break;
	 case 3:
	    if (f->initial_offset)
		printf("\tsubl $%d,%%esp\n", f->initial_offset);
	    break;
	}
	l = l->next;
    }
}

int main(int argc, char **argv)
{
    FILE *infile = stdin;
    char tmp[4096];

#ifdef __mc68000__
    if(system("perl machdep/cpuopti")==-1) {
       perror("perl machdep/cpuopti");
       return 10;
   } else return 0;
#endif

    /* For debugging... */
    if (argc == 2)
	infile = fopen (argv[1], "r");

    for(;;) {
	char *s;

	if ((fgets(tmp, 4095, infile)) == NULL)
	    break;

	s = strchr (tmp, '\n');
	if (s != NULL)
	    *s = 0;

	if (strncmp(tmp, ".globl op_", 10) == 0) {
	    struct line *first_line = NULL, *prev = NULL;
	    struct line **nextp = &first_line;
	    struct func f;
	    int nr_rets = 0;
	    int can_opt = 1;

	    do {
		struct line *current;

		if (strcmp (tmp, "#APP") != 0 && strcmp (tmp, "#NO_APP") != 0) {
		    current = *nextp = (struct line *)malloc(sizeof (struct line));
		    nextp = &current->next;
		    current->prev = prev; prev = current;
		    current->next = NULL;
		    current->delet = 0;
		    current->data = strdup (tmp);
		    if (match (current, "movl %esp,%ebp") || match (current, "enter")) {
			fprintf (stderr, "GCC failed to eliminate fp: %s\n", first_line->data);
			can_opt = 0;
		    }

		    if (match (current, "ret"))
			nr_rets++;
		}
		if ((fgets(tmp, 4095, infile)) == NULL)
		    oops();
		s = strchr (tmp, '\n');
		if (s != NULL)
		    *s = 0;
	    } while (strncmp (tmp,".Lfe", 4) != 0);

	    f.first_line = first_line;
	    f.last_line = prev;

	    if (nr_rets == 1 && can_opt)
		do_function(&f);
	    /*else
		fprintf(stderr, "Too many RET instructions: %s\n", first_line->data);*/
	    output_function(&f);
	}
	printf("%s\n", tmp);
    }
    return 0;
}
