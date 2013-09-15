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

static int processor_handler(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);

static librdf_world *world;

struct processor_data_struct
{
	librdf_storage *storage;
	librdf_model *model;
	librdf_uri *uri;
	const char *parser_type;
	FILE *fobj;
};

static int rdf_begin(PROCESSOR *me, CRAWL *crawl, CRAWLOBJ *obj, const char *uri, const char *content_type, CRAWLDATA *data);
static int rdf_process(PROCESSOR *me, CRAWL *crawl, CRAWLOBJ *obj, const char *uri, const char *content_type, CRAWLDATA *data);
static int rdf_cleanup(PROCESSOR *me, CRAWL *crawl, CRAWLOBJ *obj, const char *uri, const char *content_type, CRAWLDATA *data);
static int rdf_process_node(PROCESSOR *me, CRAWL *crawl, CRAWLOBJ *obj, CRAWLDATA *data, librdf_node *node);

int
processor_init(void)
{
	world = librdf_new_world();
	return 0;
}

int
processor_cleanup(void)
{
	librdf_free_world(world);
	world = NULL;
	return 0;
}

int
processor_init_crawler(CRAWL *crawl, CRAWLDATA *data)
{
	PROCESSOR *pdata;
	
	pdata = (PROCESSOR *) calloc(1, sizeof(PROCESSOR));
	if(!pdata)
	{
		return -1;
	}
	pdata->storage = librdf_new_storage(world, "memory", NULL, "contexts='yes'");
	if(!pdata->storage)
	{
		return -1;
	}
	data->processor = pdata;
	crawl_set_accept(crawl, "text/turtle;q=1.0, application/rdf+xml;q=0.9, text/html;q=0.75");
	crawl_set_updated(crawl, processor_handler);
	return 0;
}

int
processor_cleanup_crawler(CRAWL *crawl, CRAWLDATA *data)
{
	(void) crawl;
	
	if(!data->processor)
	{
		return 0;
	}
	if(data->processor->storage)
	{
		librdf_free_storage(data->processor->storage);
	}
	free(data->processor);
	data->processor = NULL;
	return 0;
}

static int
processor_handler(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata)
{
	CRAWLDATA *data;
	PROCESSOR *pdata;
	const char *content_type, *uri;
	
	(void) prevtime;

	data = (CRAWLDATA *) userdata;
	pdata = data->processor;
	uri = crawl_obj_uristr(obj);
	content_type = crawl_obj_type(obj);
	fprintf(stderr, "[URI is '%s', Content-Type is '%s']\n", uri, content_type);

	if(!crawl_obj_fresh(obj))
	{
		fprintf(stderr, "[processor-handler: object has not been updated]\n");
		/* it's possible that the cache itself may be older than book-keeping which
		 * depends upon it. at the very least we need to inform the queue that
		 * the object has been processed: this should trigger some kind of state
		 * checking to ensure that whatever is consuming the cached objects
		 * knows about this resource.
		 */
//		return 0;
	}
	fprintf(stderr, "[processor_handler: object has been updated]\n");
	if(!rdf_begin(pdata, crawl, obj, uri, content_type, data))
	{
		rdf_process(pdata, crawl, obj, uri, content_type, data);
	}
	rdf_cleanup(pdata, crawl, obj, uri, content_type, data);
	return 0;
}

static int
rdf_begin(PROCESSOR *me, CRAWL *crawl, CRAWLOBJ *obj, const char *uri, const char *content_type, CRAWLDATA *data)
{
	(void) crawl;
	(void) obj;
	(void) data;

	if(!content_type)
	{
		/* We can't parse if we don't know what it is */
		errno = EINVAL;
		return -1;
	}
	me->model = librdf_new_model(world, me->storage, NULL);
	if(!me->model)
	{
		return -1;
	}
	me->uri = librdf_new_uri(world, (const unsigned char *) uri);
	if(!me->uri)
	{
		return -1;
	}
	if(!strcmp(content_type, "text/turtle"))
	{
		me->parser_type = "turtle";
	}
	if(!me->parser_type)
	{
		fprintf(stderr, "[rdf_begin: no suitable parser type found for '%s']\n", content_type);
		errno = EINVAL;
		return -1;
	}
	return 0;
}

static int
rdf_cleanup(PROCESSOR *me, CRAWL *crawl, CRAWLOBJ *obj, const char *uri, const char *content_type, CRAWLDATA *data)
{
	(void) crawl;
	(void) obj;
	(void) uri;
	(void) content_type;
	(void) data;
	
	if(me->fobj)
	{
		fclose(me->fobj);
		me->fobj = NULL;
	}
	if(me->uri)
	{
		librdf_free_uri(me->uri);
		me->uri = NULL;
	}
	if(me->model)
	{
		librdf_free_model(me->model);
		me->model = NULL;
	}
	return 0;
}

static int
rdf_process(PROCESSOR *me, CRAWL *crawl, CRAWLOBJ *obj, const char *uri, const char *content_type, CRAWLDATA *data)
{
	librdf_parser *parser;
	librdf_stream *stream;
	librdf_statement *st;
	
	(void) crawl;
	(void) uri;
	(void) content_type;
	(void) data;
	
	parser = librdf_new_parser(world, me->parser_type, NULL, NULL);
	if(!parser)
	{
		return -1;
	}
	me->fobj = fopen(crawl_obj_payload(obj), "rb");
	if(!me->fobj)
	{
		librdf_free_parser(parser);
		return -1;
	}
	if(librdf_parser_parse_file_handle_into_model(parser, me->fobj, 0, me->uri, me->model))
	{
		fprintf(stderr, "[rdf_process: parse as '%s' failed]\n", me->parser_type);
		librdf_free_parser(parser);
		return -1;		
	}
	librdf_free_parser(parser);
	stream = librdf_model_as_stream(me->model);
	if(!stream)
	{
		return -1;
	}
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		
		rdf_process_node(me, crawl, obj, data, librdf_statement_get_subject(st));
		rdf_process_node(me, crawl, obj, data, librdf_statement_get_predicate(st));
		rdf_process_node(me, crawl, obj, data, librdf_statement_get_object(st));
		
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	return 0;
}

static int
rdf_process_node(PROCESSOR *me, CRAWL *crawl, CRAWLOBJ *obj, CRAWLDATA *data, librdf_node *node)
{
	librdf_uri *uri;
	
	if(!librdf_node_is_resource(node))
	{
		return 0;
	}
	uri = librdf_node_get_uri(node);
	if(!uri)
	{
		return -1;
	}
	return queue_add_uristr(crawl, (const char *) librdf_uri_as_string(uri));
}
