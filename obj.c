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

static int crawl_obj_update_(CRAWLOBJ *obj);

CRAWLOBJ *
crawl_obj_create_(CRAWL *crawl, URI *uri)
{
	CRAWLOBJ *p;
	size_t needed;
	
	p = (CRAWLOBJ *) calloc(1, sizeof(CRAWLOBJ));
	if(!p)
	{
		return NULL;
	}
	p->crawl = crawl;
	p->uri = uri_create_uri(uri, NULL);
	if(!p->uri)
	{
		crawl_obj_destroy(p);
		return NULL;
	}
	needed = uri_str(p->uri, NULL, 0);
	if(!needed ||
		(p->uristr = (char *) malloc(needed)) == NULL ||
		uri_str(p->uri, p->uristr, needed) != needed ||
		crawl_cache_key_(crawl, p->key, p->uristr))
	{
		crawl_obj_destroy(p);
		return NULL;
	}
	return p;
}

/* Open a JSON sidecar and read the information from it */
int
crawl_obj_locate_(CRAWLOBJ *obj)
{
	FILE *f;
	char *buf, *p;
	size_t bufsize;
	ssize_t count;
	
	f = cache_open_info_read_(obj->crawl, obj->key);
	if(!f)
	{
		return -1;
	}
	/* Read JSON */
	buf = 0;
	bufsize = 0;
	for(;;)
	{
		p = realloc(buf, bufsize + OBJ_READ_BLOCK + 1);
		if(!p)
		{
			fclose(f);
			return -1;
		}
		buf = p;
		p = &(buf[bufsize]);
		count = fread(p, 1, OBJ_READ_BLOCK, f);
		if(count < 0)
		{
			fclose(f);
			free(buf);
			return -1;
		}
		p[count] = 0;
		bufsize += count;
		if(count == 0)
		{
			break;
		}
	}	
	fclose(f);
	jd_release(&(obj->info));
	jd_from_jsons(&(obj->info), buf);
	free(buf);
	crawl_obj_update_(obj);
	return 0;
}

int
crawl_obj_destroy(CRAWLOBJ *obj)
{
	if(obj)
	{
		if(obj->uri)
		{
			uri_destroy(obj->uri);
		}
		free(obj->uristr);
		free(obj->payload);
		jd_release(&(obj->info));
		free(obj);
	}
	return 0;
}

const char *
crawl_obj_key(CRAWLOBJ *obj)
{
	return obj->key;
}

int
crawl_obj_status(CRAWLOBJ *obj)
{
	return obj->status;
}

time_t
crawl_obj_updated(CRAWLOBJ *obj)
{
	return obj->updated;
}

/* Replace the information in a crawl object with a new dictionary */
int
crawl_obj_replace_(CRAWLOBJ *obj, jd_var *dict)
{
	jd_release(&(obj->info));
	jd_clone(&(obj->info), dict, 1);
	return crawl_obj_update_(obj);
}

/* Update internal members of the structured based on the info jd_var */
static int
crawl_obj_update_(CRAWLOBJ *obj)
{
	jd_var *key;
	
	obj->updated = 0;
	if(obj->info.type == VOID)
	{
		return -1;
	}
	JD_SCOPE
	{
		key = jd_get_ks(&(obj->info), "updated", 1);
		if(key->type != VOID)
		{
			obj->updated = jd_get_int(key);
		}
		key = jd_get_ks(&(obj->info), "status", 1);
		if(key->type != VOID)
		{
			obj->status = jd_get_int(key);
		}
	}
	return 0;
}
