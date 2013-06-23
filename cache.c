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

static int cache_create_dirs_(CRAWL *crawl, const char *path);
static int cache_copy_filename_(CRAWL *crawl, const CACHEKEY key, const char *type, int temporary);

CRAWLOBJ *
crawl_locate(CRAWL *crawl, const char *uristr)
{
	URI *uri;
	CRAWLOBJ *r;
	
	uri = uri_create_str(uristr, NULL);
	if(!uri)
	{
		return NULL;
	}
	r = crawl_locate_uri(crawl, uri);
	uri_destroy(uri);
	return r;
}

CRAWLOBJ *
crawl_locate_uri(CRAWL *crawl, URI *uri)
{
	CRAWLOBJ *obj;
	
	obj = crawl_obj_create_(crawl, uri);
	if(!obj)
	{
		return NULL;
	}
	if(crawl_obj_locate_(obj))
	{
		crawl_obj_destroy(obj);
		return NULL;
	}
	return obj;
}

int
crawl_cache_key_(CRAWL *crawl, CACHEKEY dest, const char *uri)
{
	unsigned char buf[SHA256_DIGEST_LENGTH];
	size_t c;
	char *t;
	
	(void) crawl;
    
	/* The cache key is a truncated SHA-256 of the URI */
	c = strlen(uri);
	/* If there's a fragment, remove it */
	t = strchr(uri, '#');
	if(t)
	{
		c = t - uri;
	}
	SHA256((const unsigned char *) uri, c, buf);
	for(c = 0; c < (CACHE_KEY_LEN / 2); c++)
	{
		dest += sprintf(dest, "%02x", buf[c] & 0xff);
	}
	return 0;
}

size_t
cache_filename_(CRAWL *crawl, const CACHEKEY key, const char *type, char *buf, size_t bufsize, int temporary)
{
	size_t needed;
	const char *suffix;

	if(buf)
	{
		*buf = 0;
	}
	/* base path + "/" + key[0..1] + "/" + key[2..3] + "/" + key[0..n] + "." + type + ".tmp" */
	needed = strlen(crawl->cache) + 1 + 2 + 1 + 2 + 1 + strlen(key) + 1 + strlen(type) + 4 + 1;
	if(!buf || needed > bufsize)
	{
		return needed;
	}
	if(temporary)
	{
		suffix = CACHE_TMP_SUFFIX;
	}
	else
	{
		suffix = "";
	}
	sprintf(buf, "%s/%c%c/%c%c/%s.%s%s", crawl->cache, key[0], key[1], key[2], key[3], key, type, suffix);
	return needed;
}

FILE *
cache_open_info_write_(CRAWL *crawl, const CACHEKEY key)
{
	if(cache_copy_filename_(crawl, key, CACHE_INFO_SUFFIX, 1))
	{
		return NULL;
	}
	if(cache_create_dirs_(crawl, crawl->cachefile))
	{
		return NULL;
	}
	return fopen(crawl->cachefile, "w");
}

FILE *
cache_open_info_read_(CRAWL *crawl, const CACHEKEY key)
{
	if(cache_copy_filename_(crawl, key, CACHE_INFO_SUFFIX, 0))
	{
		return NULL;
	}
	return fopen(crawl->cachefile, "r");
}

FILE *
cache_open_payload_write_(CRAWL *crawl, const CACHEKEY key)
{
	if(cache_copy_filename_(crawl, key, CACHE_PAYLOAD_SUFFIX, 1))
	{
		return NULL;
	}
	if(cache_create_dirs_(crawl, crawl->cachefile))
	{
		return NULL;
	}
	return fopen(crawl->cachefile, "w");
}

int
cache_close_info_rollback_(CRAWL *crawl, const CACHEKEY key, FILE *f)
{
	if(f)
	{
		fclose(f);
		if(cache_copy_filename_(crawl, key, CACHE_INFO_SUFFIX, 1))
		{
			return -1;
		}
	}
	return unlink(crawl->cachefile);
}

int
cache_close_payload_rollback_(CRAWL *crawl, const CACHEKEY key, FILE *f)
{
	if(f)
	{
		fclose(f);
		if(cache_copy_filename_(crawl, key, CACHE_PAYLOAD_SUFFIX, 1))
		{
			return -1;
		}
	}
	return unlink(crawl->cachefile);
}

int
cache_close_info_commit_(CRAWL *crawl, const CACHEKEY key, FILE *f)
{
	if(!f)
	{
		errno = EINVAL;
		return -1;
	}	
	fclose(f);	
	if(cache_copy_filename_(crawl, key, CACHE_INFO_SUFFIX, 1))
	{
		return -1;
	}
	if(cache_filename_(crawl, key, CACHE_INFO_SUFFIX, crawl->cachetmp, crawl->cachefile_len, 0) > crawl->cachefile_len)
	{
		errno = ENOMEM;
		return -1;
	}
	return rename(crawl->cachefile, crawl->cachetmp);
}

int
cache_close_payload_commit_(CRAWL *crawl, const CACHEKEY key, FILE *f)
{
	if(!f)
	{
		errno = EINVAL;
		return -1;
	}	
	fclose(f);	
	if(cache_copy_filename_(crawl, key, CACHE_PAYLOAD_SUFFIX, 1))
	{
		return -1;
	}
	if(cache_filename_(crawl, key, CACHE_PAYLOAD_SUFFIX, crawl->cachetmp, crawl->cachefile_len, 0) > crawl->cachefile_len)
	{
		errno = ENOMEM;
		return -1;
	}
	return rename(crawl->cachefile, crawl->cachetmp);
}

static int
cache_create_dirs_(CRAWL *crawl, const char *path)
{
	char *t;
	struct stat sbuf;
	
	t = NULL;
	for(;;)
	{		
		t = strchr((t ? t + 1 : path), '/');
		if(!t)
		{
			break;
		}
		if(t == path)
		{
			/* absolute path: skip the leading slash */
			continue;
		}
		strncpy(crawl->cachetmp, path, (t - path));
		crawl->cachetmp[t - path] = 0;
		if(stat(crawl->cachetmp, &sbuf))
		{
			if(errno != ENOENT)
			{
				return -1;
			}
		}
		if(mkdir(crawl->cachetmp, 0777))
		{
			if(errno == EEXIST)
			{
				continue;
			}
		}
	}
	return 0;
}

static int
cache_copy_filename_(CRAWL *crawl, const CACHEKEY key, const char *type, int temporary)
{
	size_t needed;
	char *p;

	needed = cache_filename_(crawl, key, type, NULL, 0, 1);
	if(needed > crawl->cachefile_len)
	{
		p = (char *) realloc(crawl->cachefile, needed);
		if(!p)
		{
		    return -1;
		}
		crawl->cachefile = p;
		p = (char *) realloc(crawl->cachetmp, needed);
		if(!p)
		{
		    return -1;
		}
		crawl->cachetmp = p;
		crawl->cachefile_len = needed;
	}
	if(cache_filename_(crawl, key, type, crawl->cachefile, needed, temporary) > needed)
	{
		return -1;
	}
	return 0;
}
