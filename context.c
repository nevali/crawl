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

#include "p_libcrawl.h"

CRAWL *
crawl_create(void)
{
	CRAWL *p;

	p = (CRAWL *) calloc(1, sizeof(CRAWL));
	p->cache = strdup("cache");
	p->ua = strdup("User-Agent: Mozilla/5.0 (compatible; libcrawl; +https://github.com/nevali/crawl)");
	p->accept = strdup("Accept: */*");
	if(!p->cache || !p->ua || !p->accept)
	{
		crawl_destroy(p);
		return NULL;
	}
	return p;
}

void
crawl_destroy(CRAWL *p)
{
	if(p)
	{
		free(p->cache);
		free(p->cachefile);
		free(p->cachetmp);
		free(p->accept);
		free(p);
	}
}

/* Set the Accept header used in requests */
int
crawl_set_accept(CRAWL *crawl, const char *accept)
{
	char *p;
	
	/* Accept: ... - 9 + strlen(accept) */
	p = (char *) malloc(9 + strlen(accept));
	if(!p)
	{
		return -1;
	}
	sprintf(p, "Accept: %s", accept);
	free(crawl->accept);
	crawl->accept = p;
	return 0;
}
