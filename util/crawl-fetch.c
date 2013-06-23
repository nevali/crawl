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

#include "crawl.h"

/* Immediately fetch the specified URI using libcrawl */
int
main(int argc, char **argv)
{
	CRAWL *crawl;

	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s URI\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	crawl = crawl_create();
	crawl_set_accept(crawl, "text/turtle, application/rdf+xml, text/n3, */*");
	crawl_fetch(crawl, argv[1]);
	return 0;
}
