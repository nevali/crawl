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

#ifndef P_CRAWLD_H_
# define P_CRAWLD_H_                   1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <ctype.h>
# include <errno.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <syslog.h>
# include <pthread.h>

# include "crawl.h"
# include "libsupport.h"

typedef struct context_struct CONTEXT;
typedef struct processor_struct PROCESSOR;
typedef struct queue_struct QUEUE;

struct context_struct
{
	struct context_api_struct *api;
	/* The below should be considered private by would-be modules */
	unsigned long refcount;
	CRAWL *crawl;
	int crawler_id;
	int cache_id;
	PROCESSOR *processor;
	QUEUE *queue;
	size_t cfgbuflen;
	char *cfgbuf;
};

struct context_api_struct
{
	void *reserved;
	unsigned long (*addref)(CONTEXT *me);
	unsigned long (*release)(CONTEXT *me);
	int (*crawler_id)(CONTEXT *me);
	int (*cache_id)(CONTEXT *me);
	CRAWL *(*crawler)(CONTEXT *me);	
	const char *(*config_get)(CONTEXT *me, const char *key, const char *defval);
	int (*config_get_int)(CONTEXT *me, const char *key, int defval);
};

#ifndef QUEUE_STRUCT_DEFINED
struct queue_struct
{
	struct queue_api_struct *api;
};
#endif

struct queue_api_struct
{
	void *reserved;
	unsigned long (*addref)(QUEUE *me);
	unsigned long (*release)(QUEUE *me);
	int (*next)(QUEUE *me, URI **next);
	int (*add_uri)(QUEUE *me, URI *uri);
	int (*add_uristr)(QUEUE *me, const char *uristr);
	int (*updated_uristr)(QUEUE *me, const char *uri, time_t updated, time_t last_modified, int status, time_t ttl);
};

#ifndef PROCESSOR_STRUCT_DEFINED
struct processor_struct
{
	struct processor_api_struct *api;
};
#endif

struct processor_api_struct
{
	void *reserved;
	unsigned long (*addref)(PROCESSOR *me);
	unsigned long (*release)(PROCESSOR *me);
	int (*process)(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type);
};

CONTEXT *context_create(int crawler_offset);

int thread_create(int crawler_offset);
void *thread_handler(void *arg);

int processor_init(void);
int processor_cleanup(void);
int processor_init_crawler(CRAWL *crawler, CONTEXT *data);
int processor_cleanup_crawler(CRAWL *crawl, CONTEXT *data);

int queue_init(void);
int queue_cleanup(void);
int queue_init_crawler(CRAWL *crawler, CONTEXT *data);
int queue_cleanup_crawler(CRAWL *crawler, CONTEXT *data);
int queue_add_uristr(CRAWL *crawler, const char *str);
int queue_add_uri(CRAWL *crawler, URI *uri);
int queue_updated_uristr(CRAWL *crawl, const char *uristr, time_t updated, time_t last_modified, int status, time_t ttl);

int policy_init_crawler(CRAWL *crawler);

PROCESSOR *rdf_create(CRAWL *crawler);

QUEUE *db_create(CONTEXT *ctx);

#endif /*!P_CRAWLD_H_*/
