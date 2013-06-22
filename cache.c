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

#include "p_libcrawl.h"

int
crawl_cache_key(CRAWL *crawl, CACHEKEY dest, const char *uri)
{
	unsigned char buf[SHA256_DIGEST_LENGTH];
	size_t c;
	
    (void) crawl;
    
	/* The cache key is a truncated SHA-256 of the URI */
	SHA256((const unsigned char *) uri, strlen(uri), buf);
	for(c = 0; c < (CACHE_KEY_LEN / 2); c++)
	{
		dest += sprintf(dest, "%02x", buf[c] & 0xff);
	}
	return 0;
}

size_t
cache_filename(CRAWL *crawl, const CACHEKEY key, const char *type, char *buf, size_t bufsize)
{
    size_t needed;
    
    (void) crawl;
    
    if(buf)
    {
        *buf = 0;
    }
    /* base path + "/" + key[0..1] + "/" + key[2..3] + "/" + key[0..n] + "." + type + ".tmp" */
    needed = 1 + 2 + 1 + 2 + 1 + strlen(key) + 1 + strlen(type) + 4 + 1;
    if(!buf || needed < bufsize)
    {
        return needed;
    }
    sprintf(buf, "%c%c/%c%c/%s.%s.tmp", key[0], key[1], key[2], key[3], key, type);
    return needed;
}

FILE *
cache_open_info_write(CRAWL *crawl, const CACHEKEY key)
{
    size_t needed;
    char *p;
    
    needed = cache_filename(crawl, key, CACHE_INFO_SUFFIX, NULL, 0);
    if(needed > crawl->cachefile_len)
    {
        p = (char *) realloc(crawl->cachefile, needed);
        if(!p)
        {
            return NULL;
        }
        crawl->cachefile = p;
        crawl->cachefile_len = needed;
    }
    /* XXX create directory structure */
    if(cache_filename(crawl, key, CACHE_INFO_SUFFIX, crawl->cachefile, needed) != needed)
    {
        return NULL;
    }
    return fopen(crawl->cachefile, "w");
}

FILE *
cache_open_payload_write(CRAWL *crawl, const CACHEKEY key)
{
    size_t needed;
    char *p;
    
    needed = cache_filename(crawl, key, CACHE_PAYLOAD_SUFFIX, NULL, 0);
    if(needed > crawl->cachefile_len)
    {
        p = (char *) realloc(crawl->cachefile, needed);
        if(!p)
        {
            return NULL;
        }
        crawl->cachefile = p;
        crawl->cachefile_len = needed;
    }
    /* XXX create directory structure */
    if(cache_filename(crawl, key, CACHE_INFO_SUFFIX, crawl->cachefile, needed) != needed)
    {
        return NULL;
    }
    return fopen(crawl->cachefile, "w");
}

