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
 *  See the License for the specific language governing permissions and23
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libcrawl.h"

int
crawl_perform(CRAWL *crawl)
{
	CRAWLOBJ *obj;
	URI *uri;
	int r;
	
	for(;;)
	{
		uri = NULL;
		if(!crawl->next)
		{
			errno = EINVAL;
			return -1;
		}
		r = crawl->next(crawl, &uri, crawl->userdata);
		if(r < 0)
		{
			return -1;
		}
		if(!uri)
		{
			break;
		}
		obj = crawl_fetch_uri(crawl, uri);
		if(!obj)
		{
			/* Check whether there was a failed callback that would have been
			 * invoked by crawl_fetch_uri()
			 */
			if(!crawl->failed)
			{
				/* there was no callback to invoke, so simply break */
				uri_destroy(uri);
				return -1;				   
			}
		}
		crawl_obj_destroy(obj);
		uri_destroy(uri);
	}
	return 0;
}
