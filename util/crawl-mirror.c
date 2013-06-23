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
#include <ctype.h>
#include <liburi.h>
#include <libxml/HTMLparser.h>

#include "crawl.h"

#define QUEUE_BLOCK_SIZE               16

static URI *initial_uri;
static char *initial_uri_str;
static URI **queue;
size_t queue_count;
size_t queue_size;

/* Mirror a site using libcrawl and libxml2 */

static int push_str(CRAWL *crawl, const char *uristr);
static int push_uri(CRAWL *crawl, URI *uri);
static int next_callback(CRAWL *crawl, URI **next, void *userdata);
static int policy_callback(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);
static int updated_callback(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);
static int recurse_links(CRAWL *crawl, CRAWLOBJ *obj);
static int recurse_find_links(CRAWL *crawl, CRAWLOBJ *obj, xmlNodePtr node);

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
	crawl_set_accept(crawl, "text/html;q=1.0, */*;q=0.5");
	crawl_set_next(crawl, next_callback);
	crawl_set_updated(crawl, updated_callback);
	crawl_set_uri_policy(crawl, policy_callback);
	if(push_str(crawl, argv[1]))
	{
		fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
		exit(EXIT_FAILURE);
	}
	crawl_perform(crawl);
	crawl_destroy(crawl);
	return 0;
}

static int
next_callback(CRAWL *crawl, URI **next, void *userdata)
{	
	(void) crawl;
	(void) userdata;
	
	if(!queue_count)
	{
		return 0;
	}
	*next = uri_create_uri(queue[queue_count-1], NULL);
	if(!*next)
	{
		return -1;
	}
	/* URI will be freed by the crawler */
	queue[queue_count] = NULL;
	queue_count--;
	return 0;
}

static int
policy_callback(CRAWL *crawl, URI *uri, const char *uristr, void *userdata)
{
	char scheme[64];
	
	(void) crawl;
	(void) userdata;
	
	if(strncmp(uristr, initial_uri_str, strlen(initial_uri_str)))
	{
		fprintf(stderr, "Not fetching %s because it is outside of the mirror root\n", uristr);
		return 0;
	}
	uri_scheme(uri, scheme, sizeof(scheme));
	if(strcmp(scheme, "http") && strcmp(scheme, "https"))
	{
		fprintf(stderr, "Refusing to fetch non-HTTP/HTTPS URI: %s\n", uristr);
		return -1;
	}
	return 1;
}

static int
updated_callback(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata)
{
	int status;
	const char *str;
	char *type, *p;
	URI *uri;
	
	(void) crawl;
	(void) prevtime;
	(void) userdata;
	
	status = crawl_obj_status(obj);
	fprintf(stderr, "Fetched %s => %s (%d)\n", crawl_obj_uristr(obj), crawl_obj_key(obj), status);
	if(status == 200)
	{
		str = crawl_obj_type(obj);
		if(!str)
		{
			return 0;
		}
		type = strdup(str);
		if(!type)
		{
			return -1;
		}
		p = strchr(type, ';');
		if(p)
		{
			*p = 0;
		}
		while(p > type)
		{
			p--;
			if(!isspace(*p))
			{
				break;
			}
			*p = 0;
		}
		if(!strcmp(type, "text/html") || !strcmp(type, "application/xhtml+xml"))
		{
			recurse_links(crawl, obj);
		}
		free(type);
		return 0;
	}
	if(status >= 300 && status < 399)
	{
		str = crawl_obj_redirect(obj);
		if(!str)
		{
			/* Well, this is odd */
			return 0;
		}
		uri = uri_create_str(str, crawl_obj_uri(obj));
		if(!uri)
		{
			fprintf(stderr, "Failed to create a URI from a redirect target '%s'\n", str);
			return -1;
		}
		return push_uri(crawl, uri);
	}
	return 0;
}

static int
push_str(CRAWL *crawl, const char *str)
{
	URI *uri;
	
	uri = uri_create_str(str, NULL);
	if(!uri)
	{
		return -1;
	}
	return push_uri(crawl, uri);
}

/* Note that the URI passed will be freed eventually by crawl; pass a copy if
 * that's not desirable.
 */
static int
push_uri(CRAWL *crawl, URI *uri)
{
	CRAWLOBJ *obj;
	size_t needed;
	URI **p;
	
	obj = crawl_locate_uri(crawl, uri);
	if(obj)
	{
		/* Already fetched */
		return 0;
	}
	if(queue_count + 1 > queue_size)
	{
		p = (URI **) realloc(queue, sizeof(URI *) * (queue_size + QUEUE_BLOCK_SIZE));
		if(!p)
		{
			return -1;
		}
		queue = p;
		queue_size += QUEUE_BLOCK_SIZE;
	}
	queue[queue_count] = uri;
	queue_count++;
	if(!initial_uri)
	{
		initial_uri = uri_create_uri(uri, NULL);
		needed = uri_str(uri, NULL, 0);
		initial_uri_str = (char *) malloc(needed);
		if(!initial_uri_str)
		{
			return -1;
		}
		if(uri_str(uri, initial_uri_str, needed) != needed)
		{
			return -1;
		}
	}
	return 0;
}

static int
recurse_links(CRAWL *crawl, CRAWLOBJ *obj)
{
	htmlDocPtr doc;
	xmlNodePtr root;
	
	doc = htmlParseFile(crawl_obj_payload(obj), NULL);
	if(!doc)
	{
		fprintf(stderr, "Failed to parse HTML\n");
		return -1;
	}
	root = xmlDocGetRootElement(doc);
	recurse_find_links(crawl, obj, root);
	xmlFreeDoc(doc);
	return 0;
}

static int
recurse_find_links(CRAWL *crawl, CRAWLOBJ *obj, xmlNodePtr node)
{
}
