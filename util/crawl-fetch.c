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
#include <errno.h>
#include <jsondata.h>

#include "crawl.h"

/* Immediately fetch the specified URI using libcrawl */
int
main(int argc, char **argv)
{
	CRAWL *crawl;
	CRAWLOBJ *obj;
	jd_var headers = JD_INIT;
	
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s URI\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	crawl = crawl_create();
	crawl_set_accept(crawl, "text/turtle, application/rdf+xml, text/n3, */*");
	obj = crawl_fetch(crawl, argv[1]);
	if(!obj)
	{
		fprintf(stderr, "%s: failed to fetch resource: %s\n", argv[0], strerror(errno));
		crawl_destroy(crawl);
		return 1;
	}
	printf("status: %d\n", crawl_obj_status(obj));
	printf("updated: %ld\n", (long) crawl_obj_updated(obj));
	printf("key: %s\n", crawl_obj_key(obj));
	if(!crawl_obj_headers(obj, &headers, 0))
	{
		jd_printf("headers: %lJ\n", &headers);
	}
	crawl_obj_destroy(obj);
	crawl_destroy(crawl);
	return 0;
}
