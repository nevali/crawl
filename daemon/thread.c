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

#include "p_crawld.h"

static int next_callback(CRAWL *crawl, URI **next, void *userdata);
static int policy_callback(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);
static int updated_callback(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);

void *
crawl_thread(void *dummy)
{
	CRAWL *crawl;
	
	(void) dummy;
	
	crawl = crawl_create();
	crawl_set_accept(crawl, config_get("crawl:accept", "*/*"));
	crawl_set_next(crawl, next_callback);
	crawl_set_updated(crawl, updated_callback);
	crawl_set_uri_policy(crawl, policy_callback);
	log_printf(LOG_DEBUG, "crawl thread 0x%08x created\n", (unsigned long) pthread_self());
	crawl_perform(crawl);
	crawl_destroy(crawl);
	return NULL;
}

static int 
next_callback(CRAWL *crawl, URI **next, void *userdata)
{
}

static int
policy_callback(CRAWL *crawl, URI *uri, const char *uristr, void *userdata)
{
}

static int
updated_callback(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata)
{
}
