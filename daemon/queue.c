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

struct queue_data_struct
{
	SQL *db;
	char *qbuf;
	size_t qbuflen;
};

static int queue_handler(CRAWL *crawl, URI **next, void *userdata);
static int queue_insert_resource(CRAWLDATA *data, const char *cachekey, uint32_t shortkey, const char *uri, const char *rootkey);
static int queue_insert_root(CRAWLDATA *data, const char *rootkey, const char *uri);

/* Global initialisation */
int
queue_init(void)
{
	return 0;
}

/* Global cleanup */
int
queue_cleanup(void)
{
	return 0;
}

/* Initialise a crawl thread */
int
queue_init_crawler(CRAWL *crawler, CRAWLDATA *data)
{
	QUEUE *p;
	
	p = (QUEUE *) calloc(1, sizeof(struct queue_data_struct));
	if(!p)
	{
		return -1;
	}
	p->qbuflen = 1024;
	p->qbuf = (char *) malloc(p->qbuflen);
	if(!p->qbuf)
	{
		free(p);
		return -1;
	}
	p->db = sql_connect("mysql://root@localhost/anansi");
	if(!p->db)
	{
		free(p->qbuf);
		free(p);
		return -1;
	}
	data->queue = p;
	queue_add_uristr(crawler, "http://sample1.acropolis.org.uk/2380649#id");
	crawl_set_next(crawler, queue_handler);
	return 0;
}

/* Clean up a crawl thread */
int
queue_cleanup_crawler(CRAWL *crawler, CRAWLDATA *data)
{
	if(data->queue)
	{
		if(data->queue->db)
		{
			sql_disconnect(data->queue->db);
		}
		free(data->queue->qbuf);
		free(data->queue);
		data->queue = NULL;
	}
	return 0;
}

/* Add a URI (as a string) to the crawl queue */
int
queue_add_uristr(CRAWL *crawler, const char *uristr)
{
	CRAWLDATA *data;
	URI *uri, *rooturi;
	char *str, *root, *t;
	char cachekey[48], rootkey[48];
	char skey[9];
	uint32_t shortkey;	
	str = strdup(uristr);
	if(!str)
	{
		free(str);
		return -1;
	}
	t = strchr(str, '#');
	if(t)
	{
		*t = 0;
	}
	uri = uri_create_str(str, NULL);
	
	data = (CRAWLDATA *) crawl_userdata(crawler);
	if(crawl_cache_key(crawler, str, cachekey, sizeof(cachekey)))
	{
		uri_destroy(uri);
		free(str);
		return -1;
	}
	strncpy(skey, cachekey, 8);
	skey[8] = 0;
	shortkey = (uint32_t) strtoul(skey, NULL, 16);

	rooturi = uri_create_str("/", uri);
	if(crawl_cache_key_uri(crawler, rooturi, rootkey, sizeof(rootkey)))
	{
		uri_destroy(uri);
		uri_destroy(rooturi);
		free(str);
		return -1;		
	}
	root = uri_stralloc(rooturi);
	
	queue_insert_resource(data, cachekey, shortkey, str, rootkey);
	queue_insert_root(data, rootkey, root);
	
	uri_destroy(uri);	
	uri_destroy(rooturi);
	free(root);
	free(str);
	return 0;
}

int
queue_add_uri(CRAWL *crawler, URI *uri)
{
	char *uristr;
	int r;
	
	uristr = uri_stralloc(uri);
	if(!uristr)
	{
		return -1;
	}
	r = queue_add_uristr(crawler, uristr);
	free(uristr);
	return r;
}

static int
queue_handler(CRAWL *crawl, URI **next, void *userdata)
{
	SQL_STATEMENT *rs;
	CRAWLDATA *data;
	
	data = (CRAWLDATA *) crawl_userdata(crawl);
	rs = sql_queryf(data->queue->db,
		"SELECT \"res\".* "
		" FROM "
		" \"crawl_resource\" \"res\", \"crawl_root\" \"root\" "
		" WHERE "
		" \"res\".\"crawl_bucket\" = %d AND "
		" \"root\".\"hash\" = \"res\".\"root\" AND "
		" \"root\".\"earliest_update\" <= NOW() AND "
		" \"res\".\"next_fetch\" <= NOW()", data->crawler_id);
	
}

static int
queue_insert_resource(CRAWLDATA *data, const char *cachekey, uint32_t shortkey, const char *uri, const char *rootkey)
{
	SQL_STATEMENT *rs;
	
	/* BEGIN */
	rs = sql_queryf(data->queue->db, "SELECT * FROM \"crawl_resource\" WHERE \"hash\" = %Q", cachekey);
	if(!rs)
	{
		fprintf(stderr, "%s\n", sql_error(data->queue->db));
		exit(1);
	}
	if(sql_stmt_eof(rs))
	{
		if(sql_executef(data->queue->db, "INSERT INTO \"crawl_resource\" (\"hash\", \"shorthash\", \"crawl_bucket\", \"cache_bucket\", \"root\", \"uri\", \"next_fetch\") VALUES (%Q, %lu, %d, %d, %Q, %Q, NOW())", cachekey, shortkey, (shortkey % data->ncrawlers) + 1, (shortkey % data->ncaches) + 1, rootkey, uri))
		{
			fprintf(stderr, "%s\n", sql_error(data->queue->db));
			exit(1);
		}
	}
	else
	{
		/* XXX only update if values differ */
		if(sql_executef(data->queue->db, "UPDATE \"crawl_resource\" SET \"crawl_bucket\" = %d, \"cache_bucket\" = %d WHERE \"hash\" = %Q", (shortkey % data->ncrawlers) + 1, (shortkey % data->ncaches) + 1, cachekey))
		{
			fprintf(stderr, "%s\n", sql_error(data->queue->db));
			exit(1);		
		}
	}
	/* COMMIT */
	sql_stmt_destroy(rs);
	return 0;
}

static int
queue_insert_root(CRAWLDATA *data, const char *rootkey, const char *uri)
{
	SQL_STATEMENT *rs;
	
	/* BEGIN */
	rs = sql_queryf(data->queue->db, "SELECT * FROM \"crawl_root\" WHERE \"hash\" = %Q", rootkey);
	if(!rs)
	{
		fprintf(stderr, "%s\n", sql_error(data->queue->db));
		exit(1);
	}
	if(!sql_stmt_eof(rs))
	{
		/* ROLLBACK */
	}
	else
	{
		if(sql_executef(data->queue->db, "INSERT INTO \"crawl_root\" (\"hash\", \"uri\", \"earliest_update\", \"rate\") VALUES (%Q, %Q, NOW(), 1)", rootkey, uri))
		{
			fprintf(stderr, "%s\n", sql_error(data->queue->db));
			exit(1);		
		}
	}
	/* COMMIT */
	sql_stmt_destroy(rs);
	return 0;
}
