/*
 * Copyright 2013 Mo McRoberts.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHOW_PREFIX                    (1<<0)
#define SHOW_EXEC_PREFIX               (1<<1)
#define SHOW_CFLAGS                    (1<<2)
#define SHOW_LIBS                      (1<<3)
#define SHOW_ALL                       (SHOW_PREFIX|SHOW_EXEC_PREFIX|SHOW_CFLAGS|SHOW_LIBS)

static int show_options;

static int show_item(int show, int flag, const char *name, const char *value);

int
main(int argc, char **argv)
{
	if(!show_options)
	{
		show_options = SHOW_ALL;
	}
	show_item(show_options, SHOW_PREFIX, "prefix", PREFIX);
	show_item(show_options, SHOW_EXEC_PREFIX, "exec_prefix", EXEC_PREFIX);
	show_item(show_options, SHOW_CFLAGS, "cflags", "-I" INCLUDEDIR);
	show_item(show_options, SHOW_LIBS, "libs", "-L" LIBDIR " -lcrawl " LIBCRAWL_EXTRA_LIBS);
	return 0;
}

static int
show_item(int show, int flag, const char *name, const char *value)
{
	if(!(show & flag))
	{
		return 0;
	}
	if(show == flag)
	{
		puts(value);
	}
	else
	{
		printf("%s: %s\n", name, value);
	}
	return 0;
}
