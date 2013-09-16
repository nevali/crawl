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

static unsigned long context_addref(CONTEXT *me);
static unsigned long context_release(CONTEXT *me);
static int context_crawler_id(CONTEXT *me);
static int context_crawler_count(CONTEXT *me);
static int context_cache_id(CONTEXT *me);
static int context_cache_count(CONTEXT *me);
static CRAWL *context_crawler(CONTEXT *me);
static const char *context_config_get(CONTEXT *me, const char *key, const char *defval);
static int context_config_get_int(CONTEXT *me, const char *key, int defval);

static struct context_api_struct context_api = {
	NULL,
	context_addref,
	context_release,
	context_crawler_id,
	context_crawler_count,
	context_cache_id,
	context_cache_count,
	context_crawler,
	context_config_get,
	context_config_get_int
};

CONTEXT *
context_create(int crawler_offset)
{
	CONTEXT *p;
	int e;
	
	e = 0;
	p = (CONTEXT *) calloc(1, sizeof(CONTEXT));
	p->api = &context_api;
	p->refcount = 1;
	p->crawler_id = config_get_int("instance:crawler", 0);
	if(!p->crawler_id)
	{
		log_printf(LOG_CRIT, "No crawler ID has been specified in [instance] section of the configuration file\n");
		e = 1;
	}
	p->crawler_id += crawler_offset;
	p->cache_id = config_get_int("instance:cache", 0);
	if(!p->cache_id)
	{
		log_printf(LOG_CRIT, "No cache ID has been specified in [instance] section of the configuration file\n");
		e = 1;
	}
	p->ncrawlers = config_get_int("instance:crawlercount", 0);
	if(!p->ncrawlers)
	{
		log_printf(LOG_CRIT, "No crawlercount has been specified in [instance] section of the configuration file\n");
		e = 1;
	}	
	p->ncaches = config_get_int("instance:cachecount", 0);
	if(!p->ncaches)
	{
		log_printf(LOG_CRIT, "No cachecount has been specified in [instance] section of the configuration file\n");
		e = 1;
	}	
	if(e)
	{
		free(p);
		return NULL;
	}
	p->crawl = crawl_create();
	if(!p->crawl)
	{
		free(p);
		return NULL;
	}
	crawl_set_userdata(p->crawl, p);
	return p;
}

static unsigned long
context_addref(CONTEXT *me)
{
	me->refcount++;
	return me->refcount;
}

static unsigned long
context_release(CONTEXT *me)
{
	me->refcount--;
	if(!me->refcount)
	{
		if(me->crawl)
		{
			crawl_destroy(me->crawl);
		}
		free(me->cfgbuf);
		free(me);
		return 0;
	}
	return me->refcount;
}

static int
context_crawler_id(CONTEXT *me)
{
	return me->crawler_id;
}

static int
context_crawler_count(CONTEXT *me)
{
	return me->ncrawlers;
}

static int
context_cache_id(CONTEXT *me)
{
	return me->cache_id;
}

static int
context_cache_count(CONTEXT *me)
{
	return me->ncaches;
}

static CRAWL *
context_crawler(CONTEXT *me)
{
	return me->crawl;
}

static const char *
context_config_get(CONTEXT *me, const char *key, const char *defval)
{
	size_t needed;
	char *p;
	
	(void) me;
	
	needed = config_get(key, defval, me->cfgbuf, me->cfgbuflen);
	if(needed)
	if(needed > me->cfgbuflen)
	{
		p = (char *) realloc(me->cfgbuf, needed);
		if(!p)
		{
			return NULL;
		}
		me->cfgbuf = p;
		me->cfgbuflen = needed;
		if(config_get(key, defval, me->cfgbuf, me->cfgbuflen) != needed)
		{
			return NULL;
		}		
	}
	return me->cfgbuf;
}

static int
context_config_get_int(CONTEXT *me, const char *key, int defval)
{
	(void) me;
	
	return config_get_int(key, defval);
}
