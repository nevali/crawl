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

#ifndef P_LIBCRAWL_H_
# define P_LIBCRAWL_H_                 1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>

# include <curl/curl.h>
# include <openssl/sha.h>

# include "crawl.h"

/* Cached object has two parts:
 *
 * JSON metadata blob <key>.meta, containing:
 *
 * 'headers':{ } -- parsed HTTP headers (for headers appearing >1, value is an array)
 * 
 */

# define CACHE_KEY_LEN             32
# define CACHE_INFO_SUFFIX         "info"
# define CACHE_PAYLOAD_SUFFIX      "payload"

typedef char CACHEKEY[CACHE_KEY_LEN+1];

struct crawl_struct
{
    char *cachefile;
    size_t cachefile_len;
	char *accept;
	char *ua;
};

struct crawl_fetch_data_struct
{
    CRAWL *crawl;
	CURL *ch;
	const char *uri;
	CACHEKEY cachekey;
	char *headers;
	size_t headers_size;
	size_t headers_len;
	time_t cachetime;
	FILE *info;
	FILE *payload;
};

int crawl_cache_key(CRAWL *crawl, CACHEKEY dest, const char *uri);
size_t cache_filename(CRAWL *crawl, const CACHEKEY key, const char *type, char *buf, size_t bufsize);
FILE *cache_open_info_write(CRAWL *crawl, const CACHEKEY key);
FILE *cache_open_payload_write(CRAWL *crawl, const CACHEKEY key);
int cache_close_info_commit(CRAWL *crawl, const CACHEKEY key, FILE *f);
int cache_close_payload_commit(CRAWL *crawl, const CACHEKEY key, FILE *f);

#endif /*!P_LIBCRAWL_H_*/