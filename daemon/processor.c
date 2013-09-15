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
processor_init_crawler(CRAWL *crawl, CRAWLDATA *data)
{
	data->processor = rdf_create(crawl);
	if(!data->processor)
	{
		return -1;
	}
	crawl_set_updated(crawl, processor_handler);
	return 0;
	return 0;
}

int
processor_cleanup_crawler(CRAWL *crawl, CRAWLDATA *data)
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
	CRAWLDATA *data;
	PROCESSOR *pdata;
	const char *content_type, *uri;
	
	(void) prevtime;

	data = (CRAWLDATA *) userdata;
	pdata = data->processor;
	uri = crawl_obj_uristr(obj);
	content_type = crawl_obj_type(obj);
	fprintf(stderr, "[URI is '%s', Content-Type is '%s']\n", uri, content_type);

	if(!crawl_obj_fresh(obj))
	{
		fprintf(stderr, "[processor-handler: object has not been updated]\n");
		/* it's possible that the cache itself may be older than book-keeping which
		 * depends upon it. at the very least we need to inform the queue that
		 * the object has been processed: this should trigger some kind of state
		 * checking to ensure that whatever is consuming the cached objects
		 * knows about this resource.
		 */
//		return 0;
	}
	fprintf(stderr, "[processor_handler: object has been updated]\n");
	return pdata->api->process(pdata, obj, uri, content_type);
}
