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

# include <stdint.h>
# include <time.h>
# include <jsondata.h>
# include <liburi.h>

/* A crawl context.
 *
 * Note that while libcrawl is thread-safe, a single crawl context cannot be
 * used in multiple threads concurrently. You must either protect the context
 * with a lock, or create separate contexts for each thread which will invoke
 * libcrawl methods.
 */
typedef struct crawl_struct CRAWL;

/* A crawled object, returned by a cache look-up (crawl_locate) or fetch
 * (crawl_fetch). The same thread restrictions apply to crawled objects
 * as to the context.
 */
typedef struct crawl_object_struct CRAWLOBJ;

/* URI policy callback: invoked before a URI is fetched; returns 1 to proceed,
 * 0 to skip, -1 on error.
 */
typedef int (*crawl_uri_policy_cb)(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);

/* Updated callback: invoked after a resource has been fetched and stored in
 * the cache.
 */
typedef int (*crawl_updated_cb)(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);

/* Next callback: invoked to obtain the next URI to crawl; if *next is NULL on
 * return, crawling ends. The URI returned via *next will be freed by libcrawl.
 */
typedef int (*crawl_next_cb)(CRAWL *crawl, URI **next, void *userdata);

/* Create a crawl context */
CRAWL *crawl_create(void);
/* Destroy a context created with crawl_create() */
void crawl_destroy(CRAWL *p);
/* Set the Accept header sent in subsequent requests */
int crawl_set_accept(CRAWL *crawl, const char *accept);
/* Set the User-Agent header sent in subsequent requests */
int crawl_set_ua(CRAWL *crawl, const char *ua);
/* Set the private user-data pointer passed to callback functions */
int crawl_set_userdata(CRAWL *crawl, void *userdata);
/* Set the verbose flag */
int crawl_set_verbose(CRAWL *crawl, int verbose);
/* Set the cache path */
int crawl_set_cache(CRAWL *crawl, const char *path);
/* Retrieve the private user-data pointer previously set with crawl_set_userdata() */
void *crawl_userdata(CRAWL *crawl);
/* Set the callback function used to apply a URI policy */
int crawl_set_uri_policy(CRAWL *crawl, crawl_uri_policy_cb cb);
/* Set the callback function invoked when an object is updated */
int crawl_set_updated(CRAWL *crawl, crawl_updated_cb cb);
/* Set the callback function invoked to get the next URI to crawl */
int crawl_set_next(CRAWL *crawl, crawl_next_cb cb);

/* Open the payload file for a crawl object */
FILE *crawl_obj_open(CRAWLOBJ *obj);
/* Destroy an (in-memory) crawl object */
int crawl_obj_destroy(CRAWLOBJ *obj);
/* Obtain the cache key for a crawl object */
const char *crawl_obj_key(CRAWLOBJ *obj);
/* Obtain the HTTP status for a crawl object */
int crawl_obj_status(CRAWLOBJ *obj);
/* Obtain the storage timestamp for a crawl object */
time_t crawl_obj_updated(CRAWLOBJ *obj);
/* Obtain the headers for a crawl object */
int crawl_obj_headers(CRAWLOBJ *obj, jd_var *out, int clone);
/* Obtain the path to the payload for the crawl object */
const char *crawl_obj_payload(CRAWLOBJ *obj);
/* Obtain the size of the payload */
uint64_t crawl_obj_size(CRAWLOBJ *obj);
/* Obtain the crawl object URI */
const URI *crawl_obj_uri(CRAWLOBJ *obj);
/* Obtain the crawl object URI as a string */
const char *crawl_obj_uristr(CRAWLOBJ *obj);
/* Obtain the MIME type of the payload */
const char *crawl_obj_type(CRAWLOBJ *obj);
/* Obtain the redirect target of the resource */
const char *crawl_obj_redirect(CRAWLOBJ *obj);

/* Fetch a resource specified as a string containing a URI */
CRAWLOBJ *crawl_fetch(CRAWL *crawl, const char *uri);
/* Fetch a resource specified as a URI */
CRAWLOBJ *crawl_fetch_uri(CRAWL *crawl, URI *uri);

/* Locate a cached resource specified as a string */
CRAWLOBJ *crawl_locate(CRAWL *crawl, const char *uristr);
/* Locate a cached resource specified as a URI */
CRAWLOBJ *crawl_locate_uri(CRAWL *crawl, URI *uri);

/* Perform a crawling cycle */
int crawl_perform(CRAWL *crawl);

#endif /*!CRAWL_H_*/