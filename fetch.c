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

int
crawl_fetch(CRAWL *crawl, const char *uri)
{
	struct crawl_fetch_data_struct data;
	struct curl_slist *headers;
	struct tm tp;
	char modified[64];
	long status;
	int rollback, error;
	time_t now;

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
	status = 0;
	if(curl_easy_perform(data.ch))
	{
		rollback = 1;
		error = -1;
	}
	else
	{
		curl_easy_getinfo(data.ch, CURLINFO_RESPONSE_CODE, &status);
	}
	if(status == 304)
	{
		/* Not modified; rollback with successful return */
		rollback = 1;
	}
	else if(status >= 500)
	{
		/* rollback if there's already a cached version */
		error = -1;
		if(data.cachetime)
		{
			rollback = 1;
		}
	}
	else if(status != 200)
	{
		error = -1;
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
		/* If this is the first invocation, write the headers */
		data->info = cache_open_info_write_(data->crawl, data->cachekey);
		if(!data->info)
		{
			fprintf(stderr, "failed to open cache info file for writing: %s\n", strerror(errno));
			return 0;
		}
		if(data->headers)
		{
			if(fwrite(data->headers, data->headers_size, 1, data->info) != 1)
			{
				return 0;
			}
		}
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
	fprintf(stderr, "received %lu bytes of payload\n", (unsigned long) size);
	return size;
}
