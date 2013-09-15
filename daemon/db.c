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

/* crawld module for using a SQL database as a queue */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define QUEUE_STRUCT_DEFINED           1

#include "p_crawld.h"

#include <libsql.h>

static unsigned long db_addref(QUEUE *me);
static unsigned long db_release(QUEUE *me);

static struct queue_api_struct db_api = {
	NULL,
	db_addref,
	db_release
};

struct queue_struct
{
	struct queue_api_struct *api;
	unsigned long refcount;
	CRAWL *crawl;
};

QUEUE *
db_create(CRAWL *crawler)
{
	QUEUE *p;
	
	p = (QUEUE *) calloc(1, sizeof(QUEUE));
	if(!p)
	{
		return NULL;
	}
	p->api = &db_api;
	p->refcount = 1;
	p->crawl = crawler;
	return p;	
}

static unsigned long
db_addref(QUEUE *me)
{
	me->refcount++;
	return me->refcount;
}

static unsigned long
db_release(QUEUE *me)
{
	me->refcount--;
	if(!me->refcount)
	{
		free(me);
		return 0;
	}
	return me->refcount;
}
