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

typedef struct crawl_data_struct CRAWLDATA;
typedef struct processor_struct PROCESSOR;
typedef struct queue_struct QUEUE;

struct crawl_data_struct
{
	int crawler_id;
	int cache_id;
	int ncrawlers;
	int ncaches;
	PROCESSOR *processor;
	QUEUE *queue;
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

void *thread_handler(void *arg);

int processor_init(void);
int processor_cleanup(void);
int processor_init_crawler(CRAWL *crawler, CRAWLDATA *data);
int processor_cleanup_crawler(CRAWL *crawl, CRAWLDATA *data);

int queue_init(void);
int queue_cleanup(void);
int queue_init_crawler(CRAWL *crawler, CRAWLDATA *data);
int queue_cleanup_crawler(CRAWL *crawler, CRAWLDATA *data);
int queue_add_uristr(CRAWL *crawler, const char *str);
int queue_add_uri(CRAWL *crawler, URI *uri);

int policy_init_crawler(CRAWL *crawler);

PROCESSOR *rdf_create(CRAWL *crawler);

QUEUE *db_create(CRAWL *crawler);

#endif /*!P_CRAWLD_H_*/
