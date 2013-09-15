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

static int queue_handler(CRAWL *crawl, URI **next, void *userdata);

/* Global initialisation */
int
queue_init(void)
{
	return 0;
}

/* Global cleanup */
int
queue_cleanup(void)
{
	return 0;
}

/* Initialise a crawl thread */
int
queue_init_crawler(CRAWL *crawler, CRAWLDATA *data)
{
	data->queue = db_create(crawler);
	if(!data->queue)
	{
		return -1;
	}
	crawl_set_next(crawler, queue_handler);
	return 0;
}

/* Clean up a crawl thread */
int
queue_cleanup_crawler(CRAWL *crawler, CRAWLDATA *data)
{
	(void) crawler;
	
	if(!data->queue)
	{
		return 0;
	}
	data->queue->api->release(data->queue);
	data->queue = NULL;
	return 0;
}

int
queue_add_uristr(CRAWL *crawl, const char *uristr)
{
	CRAWLDATA *data;
	
	data = crawl_userdata(crawl);	
	return data->queue->api->add_uristr(data->queue, uristr);
}

int
queue_add_uri(CRAWL *crawl, URI *uri)
{
	CRAWLDATA *data;
	
	data = crawl_userdata(crawl);	
	return data->queue->api->add_uri(data->queue, uri);
}

static int
queue_handler(CRAWL *crawl, URI **next, void *userdata)
{
	CRAWLDATA *data;
	
	data = crawl_userdata(crawl);	
	return data->queue->api->next(data->queue, next);
}
