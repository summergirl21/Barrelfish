/** \file
 *  \brief Hello World application
 */

/*
 * Copyright (c) 2010, 2011, 2012, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) 
{
	printf("Hello World\n");
	// allocate  4Mb worth of stack memory, expecting page fault
	volatile char buffer[4096*1024];
	buffer[512*4096] = 'c';
	buffer[4096*100] = 'b';

	printf("done writing \n");
	return EXIT_SUCCESS;
}
