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

static int processor_handler(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);
static int processor_unchanged_handler(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);
static int processor_failed_handler(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);

int
processor_init(void)
{
	return 0;
}

int
processor_cleanup(void)
{
	return 0;
}

int
processor_init_crawler(CRAWL *crawl, CONTEXT *data)
{
	data->processor = rdf_create(crawl);
	if(!data->processor)
	{
		return -1;
	}
	crawl_set_updated(crawl, processor_handler);
	crawl_set_unchanged(crawl, processor_unchanged_handler);
	crawl_set_failed(crawl, processor_failed_handler);
	return 0;
	return 0;
}

int
processor_cleanup_crawler(CRAWL *crawl, CONTEXT *data)
{
	(void) crawl;
	
	if(!data->processor)
	{
		return 0;
	}
	data->processor->api->release(data->processor);
	data->processor = NULL;
	return 0;
}

static int
processor_handler(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata)
{
	CONTEXT *data;
	PROCESSOR *pdata;
	const char *content_type, *uri, *location;
	int r, status;
	
	(void) prevtime;
	
	data = (CONTEXT *) userdata;
	pdata = data->processor;
	uri = crawl_obj_uristr(obj);
	location = crawl_obj_redirect(obj);
	content_type = crawl_obj_type(obj);
	log_printf(LOG_DEBUG, "processor_handler: URI is '%s', Content-Type is '%s'\n", uri, content_type);
	status = crawl_obj_status(obj);
	/* If there's a redirect, ensure the redirect target will be crawled */
	if(status >= 300 && status < 400 && location && strcmp(location, uri))
	{
		queue_add_uristr(crawl, location);
	}
	log_printf(LOG_DEBUG, "processor_handler: object has been updated\n");
	r = pdata->api->process(pdata, obj, uri, content_type);
	queue_updated_uristr(crawl, uri, crawl_obj_updated(obj), crawl_obj_updated(obj), crawl_obj_status(obj), 3600);
	return r;
}

static int
processor_unchanged_handler(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata)
{
	const char *uri;
	
	(void) obj;
	(void) prevtime;
	
	uri = crawl_obj_uristr(obj);
	
	log_printf(LOG_DEBUG, "processor_unchanged_handler: object has not been updated\n");
	return queue_unchanged_uristr(crawl, uri, 0);
}

static int
processor_failed_handler(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata)
{
	const char *uri;
	
	(void) prevtime;
	
	uri = crawl_obj_uristr(obj);
	return queue_unchanged_uristr(crawl, uri, 1);
}
