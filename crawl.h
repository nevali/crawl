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

#ifndef CRAWL_H_
# define CRAWL_H_                      1

# include <time.h>
# include <jsondata.h>
# include <liburi.h>

typedef struct crawl_struct CRAWL;
typedef struct crawl_object_struct CRAWLOBJ;

typedef int (*crawl_uri_policy_cb)(URI *uri, const char *uristr, void *userdata);

/* Create a crawl context */
CRAWL *crawl_create(void);
/* Destroy a context created with crawl_create() */
void crawl_destroy(CRAWL *p);
/* Set the Accept header sent in subsequent requests */
int crawl_set_accept(CRAWL *crawl, const char *accept);
/* Set the private user-data pointer passed to callback functions */
int crawl_set_userdata(CRAWL *crawl, void *userdata);
/* Set the verbose flag */
int crawl_set_verbose(CRAWL *crawl, int verbose);
/* Retrieve the private user-data pointer previously set with crawl_set_userdata() */
void *crawl_userdata(CRAWL *crawl);
/* Set the callback function used to apply a URI policy */
int crawl_set_uri_policy(CRAWL *crawl, crawl_uri_policy_cb cb);

/* Open the payload file for a crawl object */
FILE *crawl_obj_open(CRAWLOBJ *obj);
/* Destroy an in-memory crawl object */
int crawl_obj_destroy(CRAWLOBJ *obj);
/* Obtain the cache key for a crawl object */
const char *crawl_obj_key(CRAWLOBJ *obj);
/* Obtain the HTTP status for a crawl object */
int crawl_obj_status(CRAWLOBJ *obj);
/* Obtain the storage timestamp for a crawl object */
time_t crawl_obj_updated(CRAWLOBJ *obj);

/* Fetch a resource specified as a string containing a URI */
CRAWLOBJ *crawl_fetch(CRAWL *crawl, const char *uri);
/* Fetch a resource specified as a URI */
CRAWLOBJ *crawl_fetch_uri(CRAWL *crawl, URI *uri);
/* Perform a crawling cycle */
int crawl_perform(CRAWL *crawl);

#endif /*!CRAWL_H_*/