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

int
thread_create(int crawler_offset)
{
	CONTEXT *context;
	
	context = context_create(crawler_offset);
	if(!context)
	{
		return -1;
	}
	thread_handler(context);
	return 0;
}

void *
thread_handler(void *arg)
{
	CONTEXT *context;
	CRAWL *crawler;
	
	/* no addref() of the context because this thread is given ownership of the
	 * object.
	 */
	context = (CONTEXT *) arg;
	crawler = context->crawl;
	log_printf(LOG_DEBUG, "thread_handler: crawler=%d, cache=%d\n", context->crawler_id, context->cache_id);
	crawl_set_verbose(crawler, config_get_int("crawl:verbose", 0));
	processor_init_crawler(crawler, context);
	queue_init_crawler(crawler, context);
	policy_init_crawler(crawler);
	
	while(1)
	{
		if(crawl_perform(crawler))
		{
			log_printf(LOG_CRIT, "%s\n", strerror(errno));
			break;
		}
		sleep(1);
	}

	queue_cleanup_crawler(crawler, context);
	processor_cleanup_crawler(crawler, context);

	context->api->release(context);
	return NULL;
}