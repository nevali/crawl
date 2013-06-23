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

static size_t crawl_fetch_header_(char *ptr, size_t size, size_t nmemb, void *userdata);
static size_t crawl_fetch_payload_(char *ptr, size_t size, size_t nmemb, void *userdata);
static int crawl_generate_info_(struct crawl_fetch_data_struct *data, jd_var *dict);

int
crawl_fetch(CRAWL *crawl, const char *uri)
{
	struct crawl_fetch_data_struct data;
	struct curl_slist *headers;
	struct tm tp;
	char modified[64];
	int rollback, error;
	time_t now;
	jd_var infoblock = JD_INIT, json = JD_INIT;
	size_t len;
	const char *p;
	
	/* XXX thread-safe curl_global_init() */
	/* XXX apply URI policy */
	now = time(NULL);
	memset(&data, 0, sizeof(data));
	headers = NULL;

	data.crawl = crawl;
	data.uri = uri;
	data.ch = curl_easy_init();
	crawl_cache_key_(crawl, data.cachekey, uri);
	fprintf(stderr, "URI: %s\nKey: %s\n", uri, data.cachekey);
	/* XXX cache lookup */
	if(crawl->accept)
	{
		headers = curl_slist_append(headers, crawl->accept);	
	}
	if(crawl->ua)
	{
		curl_slist_append(headers, crawl->ua);
	}
	if(data.cachetime)
	{
		/* XXX cachetime is less than threshold */
		if(now - data.cachetime < crawl->cache_min)
		{
		    return 0;
		}
		gmtime_r(&(data.cachetime), &tp);
		strftime(modified, sizeof(modified), "If-Modified-Since: %a, %e %b %Y %H:%M:%S %z", &tp);
		headers = curl_slist_append(headers, modified);
	}
	curl_easy_setopt(data.ch, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(data.ch, CURLOPT_URL, uri);
	curl_easy_setopt(data.ch, CURLOPT_WRITEFUNCTION, crawl_fetch_payload_);
	curl_easy_setopt(data.ch, CURLOPT_WRITEDATA, (void *) &data);
	curl_easy_setopt(data.ch, CURLOPT_HEADERFUNCTION, crawl_fetch_header_);
	curl_easy_setopt(data.ch, CURLOPT_HEADERDATA, (void *) &data);
	curl_easy_setopt(data.ch, CURLOPT_FOLLOWLOCATION, 0);
	curl_easy_setopt(data.ch, CURLOPT_VERBOSE, 1);
	rollback = 0;
	error = 0;
	if(curl_easy_perform(data.ch))
	{
		rollback = 1;
		error = -1;
	}
	else
	{
		curl_easy_getinfo(data.ch, CURLINFO_RESPONSE_CODE, &(data.status));
	}
	if(data.status == 304)
	{
		/* Not modified; rollback with successful return */
		rollback = 1;
	}
	else if(data.status >= 500)
	{
		/* rollback if there's already a cached version */
		error = -1;
		if(data.cachetime)
		{
			rollback = 1;
		}
	}
	else if(data.status != 200)
	{
		error = -1;
	}	
	if(!rollback)
	{
		JD_SCOPE
		{
			data.info = cache_open_info_write_(data.crawl, data.cachekey);
			if(crawl_generate_info_(&data, &infoblock))
			{
				rollback = 1;
				error = -1;
			}
			else
			{
				jd_to_json(&json, &infoblock);
				p = jd_bytes(&json, &len);
				len--;				
				if(!p || (fwrite(p, len, 1, data.info) != 1))
				{
					rollback = 1;
					error = -1;
				}
			}
			jd_release(&infoblock);			
		}
	}
	free(data.headers);
	if(rollback)
	{
		cache_close_info_rollback_(crawl, data.cachekey, data.info);
		cache_close_payload_rollback_(crawl, data.cachekey, data.payload);
	}
	else
	{
		cache_close_info_commit_(crawl, data.cachekey, data.info);
		cache_close_payload_commit_(crawl, data.cachekey, data.payload);	
	}
	curl_slist_free_all(headers);
	curl_easy_cleanup(data.ch);
	return error;
}

static size_t
crawl_fetch_header_(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct crawl_fetch_data_struct *data;
	size_t n;
	char *p;
	
	data = (struct crawl_fetch_data_struct *) userdata;
	size *= nmemb;
	n = data->headers_size;
	while(n < data->headers_len + size + 1)
	{
		n += HEADER_ALLOC_BLOCK;
	}
	if(n != data->headers_size)
	{
		/* XXX check data->headers_size against maximum and abort if too large */
		p = (char *) realloc(data->headers, n);
		if(!p)
		{
			return 0;
		}
		data->headers_size = n;
		data->headers = p;
	}
	memcpy(&(data->headers[data->headers_len]), ptr, size);
	data->headers_len += size;
	data->headers[data->headers_len] = 0;
	return size;
}

static size_t
crawl_fetch_payload_(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct crawl_fetch_data_struct *data;
    
	data = (struct crawl_fetch_data_struct *) userdata;    
	if(!data->payload)
	{
		data->payload = cache_open_payload_write_(data->crawl, data->cachekey);
		if(!data->payload)
		{
			fprintf(stderr, "failed to open payload for writing: %s\n", strerror(errno));
			return 0;
		}
	}
	if(fwrite(ptr, size, nmemb, data->payload) != nmemb)
	{
		return 0;
	}
	size *= nmemb;
	return size;
}

static int
crawl_generate_info_(struct crawl_fetch_data_struct *data, jd_var *dict)
{
	jd_var *key, *value, *headers;
	char *ptr, *s, *p;
	
	jd_set_hash(dict, 8);
	JD_SCOPE
	{
		key = jd_get_ks(dict, "status", 1);
		value = jd_niv(data->status);
		jd_assign(key, value);
		jd_retain(dict);
		ptr = NULL;
		curl_easy_getinfo(data->ch, CURLINFO_REDIRECT_URL, &ptr);
		if(ptr)
		{
			key = jd_get_ks(dict, "location", 1);
			value = jd_nsv(ptr);
			jd_assign(key, value);
		}
		ptr = NULL;
		curl_easy_getinfo(data->ch, CURLINFO_CONTENT_TYPE, &ptr);
		if(ptr)
		{
			key = jd_get_ks(dict, "type", 1);
			value = jd_nsv(ptr);
			jd_assign(key, value);	
		}
		headers = jd_nhv(32);
		s = data->headers;
		for(;;)
		{
			p = strchr(s, '\n');
			if(!p)
			{
				break;
			}
			if(s == p)
			{
				s = p + 1;
				continue;
			}
			*p = 0;
			p--;
			if(*p == '\r')
			{
				*p = 0;
			}
			if(s == data->headers)
			{
				jd_assign(jd_get_ks(headers, ":", 1), jd_nsv(s));
				s = p + 2;
				continue;
			}
			ptr = strchr(s, ':');
			if(!ptr)
			{
				s = p + 2;
				continue;
			}			
			*ptr = 0;
			ptr++;
			if(isspace(*ptr))
			{
				ptr++;
			}
			key = jd_get_ks(headers, s, 1);
			if(key->type == VOID)
			{
				jd_assign(key, jd_nav(1));
			}
			value = jd_push(key, 1);
			jd_assign(value, jd_nsv(ptr));
			s = p + 2;
		}
		key = jd_get_ks(dict, "headers", 1);
		jd_assign(key, headers);
	}
	return 0;
}
