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

# include <jsondata.h>
# include <liburi.h>

typedef struct crawl_struct CRAWL;

typedef int (*crawl_uri_policy_cb)(URI *uri, const char *uristr, void *userdata);

CRAWL *crawl_create(void);
void crawl_destroy(CRAWL *p);
int crawl_set_accept(CRAWL *crawl, const char *accept);
int crawl_set_userdata(CRAWL *crawl, void *userdata);
int crawl_set_uri_policy(CRAWL *crawl, crawl_uri_policy_cb cb);

int crawl_fetch(CRAWL *crawl, const char *uri);
int crawl_fetch_uri(CRAWL *crawl, URI *uri);
int crawl_perform(CRAWL *crawl);

#endif /*!CRAWL_H_*/