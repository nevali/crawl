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

#include "p_crawld.h"

static int policy_checkpoint(CRAWL *crawl, CRAWLOBJ *obj, int *status, void *userdata);
static char **policy_create_list(const char *name, const char *defval);
static int policy_destroy_list(char **list);
static int policy_uri(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);

static char **types_whitelist;
static char **types_blacklist;
static char **schemes_whitelist;
static char **schemes_blacklist;

int
policy_init(void)
{
	types_whitelist = policy_create_list("content-types:whitelist", NULL);
	types_blacklist = policy_create_list("content-types:blacklist", NULL);
	schemes_whitelist = policy_create_list("schemes:whitelist", NULL);
	schemes_blacklist = policy_create_list("schemes:blacklist", NULL);
	return 0;
}

int
policy_cleanup(void)
{
	policy_destroy_list(types_whitelist);
	types_whitelist = NULL;
	policy_destroy_list(types_blacklist);
	types_blacklist = NULL;
	policy_destroy_list(schemes_whitelist);
	schemes_whitelist = NULL;
	policy_destroy_list(schemes_blacklist);
	schemes_blacklist = NULL;	
	return 0;
}

int
policy_init_crawler(CRAWL *crawl, CONTEXT *data)
{
	(void) data;
	
	crawl_set_uri_policy(crawl, policy_uri);
	crawl_set_checkpoint(crawl, policy_checkpoint);
	return 0;
}

static char **
policy_create_list(const char *key, const char *defval)
{
	char **list;
	char *s, *t, *p;
	size_t count;
	int prevspace;
	
	if(!defval)
	{
		defval = "";
	}
	s = config_geta(key, defval);
	if(!s)
	{
		return NULL;
	}
	count = 0;
	prevspace = 1;
	for(t = s; *t; t++)
	{
		if(isspace(*t) || *t == ';' || *t == ',')
		{
			if(!prevspace)
			{
				count++;
			}
			prevspace = 1;
			continue;
		}
		prevspace = 0;
	}
	if(!prevspace)
	{
		count++;
	}
	list = (char **) calloc(count + 1, sizeof(char *));
	if(!list)
	{
		return NULL;
	}
	count = 0;
	p = NULL;
	for(t = s; *t; t++)
	{
		if(isspace(*t) || *t == ';' || *t == ',')
		{
			*t = 0;
			if(p)
			{
				list[count] = strdup(p);
				if(!list[count])
				{
					policy_destroy_list(list);
					return NULL;
				}
				count++;
				p = NULL;				
			}
			continue;
		}
		if(!p)
		{
			p = t;
		}
	}
	if(p)
	{
		list[count] = strdup(p);
		if(!list[count])
		{
			policy_destroy_list(list);
			return NULL;
		}		
		count++;		
	}
	list[count] = NULL;
	for(count = 0; list[count]; count++)
	{
		log_printf(LOG_DEBUG, "%s => '%s'\n", key, list[count]);
	}
	return list;
}

static int
policy_destroy_list(char **list)
{
	size_t c;
	
	for(c = 0; list && list[c]; c++)
	{
		free(list[c]);
	}
	free(list);
	return 0;
}

static int
policy_uri(CRAWL *crawl, URI *uri, const char *uristr, void *userdata)
{
	char scheme[64];
	size_t c;
	int n;
	
	(void) userdata;
	(void) crawl;
	
	uri_scheme(uri, scheme, sizeof(scheme));
	if(schemes_whitelist && schemes_whitelist[0])
	{
		n = 0;
		for(c = 0; schemes_whitelist[c]; c++)
		{
			if(!strcasecmp(schemes_whitelist[c], scheme))
			{
				n = 1;
				break;
			}
		}
		if(!n)
		{
			log_printf(LOG_NOTICE, "Policy: URI '%s' has a scheme (%s) which is not whitelisted\n", uristr, scheme);
			return 0;
		}
	}
	if(schemes_blacklist)
	{
		for(c = 0; schemes_blacklist[c]; c++)
		{
			if(!strcasecmp(schemes_blacklist[c], scheme))
			{
				log_printf(LOG_NOTICE, "Policy: URI '%s' has a scheme (%s) which is blacklisted\n", uristr, scheme);
				return 0;
			}
		}
	}
	return 1;
}

static int
policy_checkpoint(CRAWL *crawl, CRAWLOBJ *obj, int *status, void *userdata)
{
	int n, c;
	const char *type;
	char *s, *t;
	
	(void) crawl;
	(void) userdata;
	
	if(*status >= 300 && *status < 400)
	{
		return 0;
	}
	type = crawl_obj_type(obj);
	if(!type)
	{
		type = "";
	}
	s = strdup(type);
	if(!s)
	{
		return -1;
	}
	t = strchr(s, ';');
	if(t)
	{
		*t = 0;
		t--;
		while(t > s)
		{
			if(!isspace(*t))
			{
				break;
			}
			*t = 0;
			t--;
		}
	}
	log_printf(LOG_DEBUG, "Policy: content type is '%s', status is '%d'\n", s, *status);
	if(types_whitelist && types_whitelist[0])
	{
		n = 0;
		for(c = 0; types_whitelist[c]; c++)
		{
			if(!strcasecmp(types_whitelist[c], s))
			{
				n = c;
				break;
			}
		}
		if(!n)
		{
			log_printf(LOG_DEBUG, "Policy: type '%s' not matched by whitelist\n", s);
			free(s);
			*status = 406;
			return 1;
		}
	}
	if(types_blacklist)
	{
		for(c = 0; types_blacklist[c]; c++)
		{
			if(!strcasecmp(types_blacklist[c], s))
			{
				log_printf(LOG_DEBUG, "Policy: type '%s' is blacklisted\n", s);
				free(s);
				*status = 406;
				return 1;
			}
		}
	}
	free(s);
	return 0;
}
