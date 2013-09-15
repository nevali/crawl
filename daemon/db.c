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

/* crawld module for using a SQL database as a queue */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define QUEUE_STRUCT_DEFINED           1

#include "p_crawld.h"

#include <libsql.h>

static unsigned long db_addref(QUEUE *me);
static unsigned long db_release(QUEUE *me);
static int db_next(QUEUE *me, URI **next);
static int db_add_uri(QUEUE *me, URI *uristr);
static int db_add_uristr(QUEUE *me, const char *uristr);

static int db_insert_resource(QUEUE *me, const char *cachekey, uint32_t shortkey, const char *uri, const char *rootkey);
static int db_insert_root(QUEUE *me, const char *rootkey, const char *uri);

static struct queue_api_struct db_api = {
	NULL,
	db_addref,
	db_release,
	db_next,
	db_add_uri,
	db_add_uristr
};

struct queue_struct
{
	struct queue_api_struct *api;
	unsigned long refcount;
	CRAWL *crawl;
	SQL *db;
	int crawler_id;
	int cache_id;
	int ncrawlers;
	int ncaches;
	char *buf;
	size_t buflen;
};

QUEUE *
db_create(CRAWL *crawler)
{
	QUEUE *p;
	
	p = (QUEUE *) calloc(1, sizeof(QUEUE));
	if(!p)
	{
		return NULL;
	}
	p->api = &db_api;
	p->refcount = 1;
	p->crawl = crawler;
	p->crawler_id = 1;
	p->cache_id = 1;
	p->ncrawlers = 1;
	p->ncaches = 1;
	p->db = sql_connect("mysql://root@localhost/anansi");
	if(!p->db)
	{
		free(p);
		return NULL;
	}
	return p;	
}

static unsigned long
db_addref(QUEUE *me)
{
	me->refcount++;
	return me->refcount;
}

static unsigned long
db_release(QUEUE *me)
{
	me->refcount--;
	if(!me->refcount)
	{
		if(me->db)
		{
			sql_disconnect(me->db);
		}
		free(me->buf);
		free(me);
		return 0;
	}
	return me->refcount;
}

static int
db_next(QUEUE *me, URI **next)
{
	SQL_STATEMENT *rs;
	size_t needed;
	char *p;
	
	*next = NULL;
	rs = sql_queryf(me->db,
		"SELECT \"res\".\"uri\" "
		" FROM "
		" \"crawl_resource\" \"res\", \"crawl_root\" \"root\" "
		" WHERE "
		" \"res\".\"crawl_bucket\" = %d AND "
		" \"root\".\"hash\" = \"res\".\"root\" AND "
		" \"root\".\"earliest_update\" <= NOW() AND "
		" \"res\".\"next_fetch\" <= NOW()", me->crawler_id);
	if(sql_stmt_eof(rs))
	{
		fprintf(stderr, "no results\n");
		sql_stmt_destroy(rs);
		return 0;
	}
	needed = sql_stmt_value(rs, 0, NULL, 0);
	fprintf(stderr, "needed = %d\n", (int) needed);
	if(needed > me->buflen)
	{
		p = (char *) realloc(me->buf, needed + 1);
		if(!p)
		{
			sql_stmt_destroy(rs);
			return -1;
		}
		me->buf = p;
		me->buflen = needed + 1;
	}
	if(sql_stmt_value(rs, 0, me->buf, me->buflen) != needed)
	{
		sql_stmt_destroy(rs);
		return -1;
	}
	*next = uri_create_str(me->buf, NULL);
	sql_stmt_destroy(rs);
	if(!*next)
	{
		return -1;
	}
	return 0;
}

static int
db_add_uri(QUEUE *me, URI *uri)
{
	char *uristr;
	int r;
	
	uristr = uri_stralloc(uri);
	if(!uristr)
	{
		return -1;
	}
	r = db_add_uristr(me, uristr);
	free(uristr);
	return r;
}

static int
db_add_uristr(QUEUE *me, const char *uristr)
{
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
	
	if(crawl_cache_key(me->crawl, str, cachekey, sizeof(cachekey)))
	{
		uri_destroy(uri);
		free(str);
		return -1;
	}
	strncpy(skey, cachekey, 8);
	skey[8] = 0;
	shortkey = (uint32_t) strtoul(skey, NULL, 16);

	rooturi = uri_create_str("/", uri);
	if(crawl_cache_key_uri(me->crawl, rooturi, rootkey, sizeof(rootkey)))
	{
		uri_destroy(uri);
		uri_destroy(rooturi);
		free(str);
		return -1;		
	}
	root = uri_stralloc(rooturi);
	
	db_insert_resource(me, cachekey, shortkey, str, rootkey);
	db_insert_root(me, rootkey, root);
	
	uri_destroy(uri);	
	uri_destroy(rooturi);
	free(root);
	free(str);
	return 0;
	
}

static int
db_insert_resource(QUEUE *me, const char *cachekey, uint32_t shortkey, const char *uri, const char *rootkey)
{
	SQL_STATEMENT *rs;
	
	/* BEGIN */
	rs = sql_queryf(me->db, "SELECT * FROM \"crawl_resource\" WHERE \"hash\" = %Q", cachekey);
	if(!rs)
	{
		fprintf(stderr, "%s\n", sql_error(me->db));
		exit(1);
	}
	if(sql_stmt_eof(rs))
	{
		if(sql_executef(me->db, "INSERT INTO \"crawl_resource\" (\"hash\", \"shorthash\", \"crawl_bucket\", \"cache_bucket\", \"root\", \"uri\", \"next_fetch\") VALUES (%Q, %lu, %d, %d, %Q, %Q, NOW())", cachekey, shortkey, (shortkey % me->ncrawlers) + 1, (shortkey % me->ncaches) + 1, rootkey, uri))
		{
			fprintf(stderr, "%s\n", sql_error(me->db));
			exit(1);
		}
	}
	else
	{
		/* XXX only update if values differ */
		if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"crawl_bucket\" = %d, \"cache_bucket\" = %d WHERE \"hash\" = %Q", (shortkey % me->ncrawlers) + 1, (shortkey % me->ncaches) + 1, cachekey))
		{
			fprintf(stderr, "%s\n", sql_error(me->db));
			exit(1);		
		}
	}
	/* COMMIT */
	sql_stmt_destroy(rs);
	return 0;
}

static int
db_insert_root(QUEUE *me, const char *rootkey, const char *uri)
{
	SQL_STATEMENT *rs;
	
	/* BEGIN */
	rs = sql_queryf(me->db, "SELECT * FROM \"crawl_root\" WHERE \"hash\" = %Q", rootkey);
	if(!rs)
	{
		fprintf(stderr, "%s\n", sql_error(me->db));
		exit(1);
	}
	if(!sql_stmt_eof(rs))
	{
		/* ROLLBACK */
	}
	else
	{
		if(sql_executef(me->db, "INSERT INTO \"crawl_root\" (\"hash\", \"uri\", \"earliest_update\", \"rate\") VALUES (%Q, %Q, NOW(), 1)", rootkey, uri))
		{
			fprintf(stderr, "%s\n", sql_error(me->db));
			exit(1);		
		}
	}
	/* COMMIT */
	sql_stmt_destroy(rs);
	return 0;
}
