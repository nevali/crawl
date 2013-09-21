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
#define TXN_MAX_RETRIES                10

#include "p_crawld.h"

#include <libsql.h>

static int db_migrate(SQL *restrict, const char *identifier, int newversion, void *restrict userdata);
static unsigned long db_addref(QUEUE *me);
static unsigned long db_release(QUEUE *me);
static int db_next(QUEUE *me, URI **next);
static int db_add_uri(QUEUE *me, URI *uristr);
static int db_add_uristr(QUEUE *me, const char *uristr);
static int db_updated_uristr(QUEUE *me, const char *uri, time_t updated, time_t last_modified, int status, time_t ttl);
static int db_insert_resource(QUEUE *me, const char *cachekey, uint32_t shortkey, const char *uri, const char *rootkey);
static int db_insert_root(QUEUE *me, const char *rootkey, const char *uri);
static int db_insert_resource_txn(SQL *db, void *userdata);
static int db_insert_root_txn(SQL *db, void *userdata);
static int db_log_query(SQL *restrict db, const char *restrict statement);
static int db_log_error(SQL *restrict db, const char *restrict sqlstate, const char *restrict message);

static struct queue_api_struct db_api = {
	NULL,
	db_addref,
	db_release,
	db_next,
	db_add_uri,
	db_add_uristr,
	db_updated_uristr
};

struct queue_struct
{
	struct queue_api_struct *api;
	unsigned long refcount;
	CONTEXT *ctx;
	CRAWL *crawl;
	SQL *db;
	int crawler_id;
	int cache_id;
	int ncrawlers;
	int ncaches;
	char *buf;
	size_t buflen;
};

struct resource_insert
{
	QUEUE *me;
	const char *cachekey;
	uint32_t shortkey;
	const char *uri;
	const char *rootkey;
};

struct root_insert
{
	QUEUE *me;
	const char *rootkey;
	const char *uri;
};

QUEUE *
db_create(CONTEXT *ctx)
{
	QUEUE *p;
	
	p = (QUEUE *) calloc(1, sizeof(QUEUE));
	if(!p)
	{
		return NULL;
	}
	p->api = &db_api;
	p->refcount = 1;
	p->ctx = ctx;
	p->crawl = ctx->api->crawler(ctx);
	p->crawler_id = ctx->api->crawler_id(ctx);
	p->cache_id = ctx->api->cache_id(ctx);
	p->ncrawlers = config_get_int("db:crawlercount", 0);
	if(!p->ncrawlers)
	{
		log_printf(LOG_CRIT, "DB: No crawlercount has been specified in [db] section of the configuration file\n");
		free(p);
		return NULL;
	}	
	p->ncaches = config_get_int("db:cachecount", 0);
	if(!p->ncaches)
	{
		log_printf(LOG_CRIT, "DB: No cachecount has been specified in [db] section of the configuration file\n");
		free(p);
		return NULL;
	}	
	p->db = sql_connect(ctx->api->config_get(ctx, "db:uri", "mysql://localhost/crawl"));
	if(!p->db)
	{
		free(p);
		return NULL;
	}
	sql_set_querylog(p->db, db_log_query);
	sql_set_errorlog(p->db, db_log_error);
	if(sql_migrate(p->db, "com.github.nevali.crawl.db", db_migrate, NULL))
	{
		log_printf(LOG_CRIT, "DB: Database migration failed\n");
		sql_disconnect(p->db);
		free(p);
		return NULL;
	}
	return p;
}

static int
db_log_query(SQL *restrict db, const char *restrict statement)
{
	(void) db;
	
	log_printf(LOG_DEBUG, "DB: %s\n", statement);
	return 0;
}

static int
db_log_error(SQL *restrict db, const char *restrict sqlstate, const char *restrict message)
{
	(void) db;
	
	log_printf(LOG_DEBUG, "DB Error: %s: %s\n", sqlstate, message);
	return 0;
}
	

static int
db_migrate(SQL *restrict sql, const char *identifier, int newversion, void *restrict userdata)
{
	(void) userdata;
	(void) identifier;
	
	if(newversion == 0)
	{
		/* Return target version */
		return 2;
	}
	log_printf(LOG_NOTICE, "DB: Migrating database to version %d\n", newversion);
	if(newversion == 1)
	{
		if(sql_execute(sql, "DROP TABLE IF EXISTS \"crawl_root\""))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE TABLE \"crawl_root\" ("
			"\"hash\" VARCHAR(32) NOT NULL COMMENT 'Hash of canonical root URI',"
			"\"uri\" VARCHAR(255) NOT NULL COMMENT 'Root URI',"
			"\"added\" DATETIME NOT NULL COMMENT 'Timestamp that this root was added',"
			"\"last_updated\" DATETIME DEFAULT NULL COMMENT 'Timestamp of most recent fetch from this root',"
			"\"earliest_update\" DATETIME DEFAULT NULL COMMENT 'Earliest time this root can be fetched from again',"
			"\"rate\" INT NOT NULL DEFAULT 1000 COMMENT 'Minimum wait time in milliseconds between fetches',"
			"PRIMARY KEY (\"hash\"),"
			"KEY \"crawl_root_last_updated\" (\"last_updated\"),"
			"KEY \"crawl_root_earliest_update\" (\"earliest_update\")"
			") ENGINE=InnoDB DEFAULT CHARSET=utf8 DEFAULT COLLATE=utf8_unicode_ci"))
		{
			return -1;
		}
		return 0;
	}
	if(newversion == 2)
	{
		if(sql_execute(sql, "DROP TABLE IF EXISTS \"crawl_resource\""))
		{
			return -1;
		}
		if(sql_execute(sql, "CREATE TABLE \"crawl_resource\" ("
			"\"hash\" VARCHAR(32) NOT NULL COMMENT 'Hash of canonical URI',"
			"\"shorthash\" BIGINT UNSIGNED NOT NULL COMMENT 'First 32 bits of hash',"
			"\"crawl_bucket\" INT NOT NULL COMMENT 'Assigned crawler instance',"
			"\"cache_bucket\" INT NOT NULL COMMENT 'Assigned cache instance',"
			"\"crawl_instance\" INT NOT NULL COMMENT 'Active crawler instance',"
			"\"root\" VARCHAR(32) NOT NULL COMMENT 'Hash of canonical root URI',"
			"\"updated\" DATETIME DEFAULT NULL COMMENT 'Timestamp that this resource was updated in the cache',"
			"\"added\" DATETIME NOT NULL COMMENT 'Timestamp that this resource was added',"
			"\"last_modified\" DATETIME DEFAULT NULL COMMENT 'Timestamp that this resource was modified',"
			"\"status\" INT DEFAULT NULL COMMENT 'HTTP status code',"			
			"\"uri\" VARCHAR(255) NOT NULL COMMENT 'Resource URI',"	
			"\"next_fetch\" DATETIME NOT NULL COMMENT 'Earliest time for next fetch of this resource',"
			"\"error_count\" INT NOT NULL DEFAULT 0 COMMENT 'Hard error count',"
			"\"last_ttl\" INT NOT NULL DEFAULT 0 COMMENT 'Last TTL in hours',"
			"\"soft_error_count\" INT NOT NULL DEFAULT 0 COMMENT 'Soft error count',"
			"PRIMARY KEY (\"hash\"),"
			"KEY \"crawl_resource_crawl_bucket\" (\"crawl_bucket\"),"
			"KEY \"crawl_resource_cache_bucket\" (\"cache_bucket\"),"
			"KEY \"crawl_resource_crawl_instance\" (\"crawl_instance\"),"
			"KEY \"crawl_resource_root\" (\"root\"),"
			"KEY \"crawl_resource_next_fetch\" (\"next_fetch\")"				
			") ENGINE=InnoDB DEFAULT CHARSET=utf8 DEFAULT COLLATE=utf8_unicode_ci"))
		{
			return -1;
		}
		return 0;
	}
	return -1;
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
		" \"root\".\"earliest_update\" < NOW() AND "
		" \"res\".\"next_fetch\" < NOW() "
		" ORDER BY \"root\".\"earliest_update\" ASC, \"res\".\"next_fetch\" ASC", me->crawler_id);
	if(!rs)
	{
		log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
		exit(1);
	}
	if(sql_stmt_eof(rs))
	{
		log_printf(LOG_DEBUG, "db_next: queue query returned no results\n");
		sql_stmt_destroy(rs);
		return 0;
	}
	needed = sql_stmt_value(rs, 0, NULL, 0);
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
db_uristr_key_root(QUEUE *me, const char *uristr, char **uri, char *urikey, uint32_t *shortkey, char **root, char *rootkey)
{
	URI *u_resource, *u_root;
	char *str, *t;
	char skey[9];
	
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
	u_resource = uri_create_str(str, NULL);
	if(!u_resource)
	{
		free(str);
		return -1;
	}
	/* Ensure we have the canonical form of the URI */
	free(str);
	str = uri_stralloc(u_resource);
	if(!str)
	{
		uri_destroy(u_resource);
		return -1;
	}
	if(uri)
	{
		*uri = str;
	}
	if(crawl_cache_key(me->crawl, str, urikey, 48))
	{
		uri_destroy(u_resource);
		free(str);
		return -1;
	}
	strncpy(skey, urikey, 8);
	skey[8] = 0;
	*shortkey = (uint32_t) strtoul(skey, NULL, 16);
	
	u_root = uri_create_str("/", u_resource);
	if(crawl_cache_key_uri(me->crawl, u_root, rootkey, 48))
	{
		uri_destroy(u_resource);
		uri_destroy(u_root);
		free(str);
		return -1;		
	}
	if(root)
	{
		*root = uri_stralloc(u_root);
	}
	if(!uri)
	{
		free(str);
	}
	uri_destroy(u_resource);
	uri_destroy(u_root);
	return 0;
}

static int
db_add_uristr(QUEUE *me, const char *uristr)
{
	char *canonical, *root;
	char cachekey[48], rootkey[48];
	uint32_t shortkey;
	
	if(db_uristr_key_root(me, uristr, &canonical, cachekey, &shortkey, &root, rootkey))
	{
		return -1;
	}
	
	db_insert_resource(me, cachekey, shortkey, canonical, rootkey);
	db_insert_root(me, rootkey, root);
	
	free(root);
	free(canonical);
	return 0;
	
}

static int
db_updated_uristr(QUEUE *me, const char *uristr, time_t updated, time_t last_modified, int status, time_t ttl)
{
	char *canonical, *root;
	char cachekey[48], rootkey[48], updatedstr[32], lastmodstr[32], nextfetchstr[32];
	uint32_t shortkey;
	struct tm tm;
	time_t now;
	
	if(db_uristr_key_root(me, uristr, &canonical, cachekey, &shortkey, &root, rootkey))
	{
		return -1;
	}
	gmtime_r(&updated, &tm);
	strftime(updatedstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	gmtime_r(&last_modified, &tm);
	strftime(lastmodstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	if(status != 200)
	{
		if(ttl < 86400)
		{
			ttl = 86400;
		}
	}
	else
	{
		if(ttl < 3600)
		{
			ttl = 3600;
		}
	}
	ttl += time(NULL);
	gmtime_r(&ttl, &tm);
	strftime(nextfetchstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	log_printf(LOG_DEBUG, "UPDATE \"crawl_resource\" SET \"updated\" = %s, \"last_modified\" = %s, \"status\" = %d, \"next_fetch\" = %s, \"crawl_instance\" = NULL WHERE \"hash\" = %s\n",
		updatedstr, lastmodstr, status, nextfetchstr, cachekey);
	if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"updated\" = %Q, \"last_modified\" = %Q, \"status\" = %d, \"next_fetch\" = %Q, \"crawl_instance\" = NULL WHERE \"hash\" = %Q",
		updatedstr, lastmodstr, status, nextfetchstr, cachekey))
	{
		log_printf(LOG_CRIT, "%s\n", sql_error(me->db));
		exit(1);
	}
	now = time(NULL);
	gmtime_r(&now, &tm);
	strftime(lastmodstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	now += 2;
	strftime(nextfetchstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	if(sql_executef(me->db, "UPDATE \"crawl_root\" SET \"last_updated\" = %Q, \"earliest_update\" = %Q WHERE \"hash\" = %Q", lastmodstr, nextfetchstr, rootkey))
	{
		log_printf(LOG_CRIT, "%s\n", sql_error(me->db));
		exit(1);
	}
	if(status >= 400 && status < 499)
	{
		if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"error_count\" = \"error_count\" + 1 WHERE \"hash\" = %Q", cachekey))
		{
			log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
			exit(1);
		}
	}
	else if(status >= 500 && status < 599)
	{
		if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"error_count\" = 0, \"soft_error_count\" = \"soft_error_count\" + 1 WHERE \"hash\" = %Q", cachekey))
		{
			log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
			exit(1);
		}
	}
	else
	{
		if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"error_count\" = 0, \"soft_error_count\" = 0 WHERE \"hash\" = %Q", cachekey))
		{
			log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
			exit(1);
		}	
	}	
	free(root);
	free(canonical);
	return 0;
}


static int
db_insert_resource(QUEUE *me, const char *cachekey, uint32_t shortkey, const char *uri, const char *rootkey)
{
	struct resource_insert data;
	
	data.me = me;
	data.cachekey = cachekey;
	data.shortkey = shortkey;
	data.uri = uri;
	data.rootkey = rootkey;
	
	if(sql_perform(me->db, db_insert_resource_txn, &data, TXN_MAX_RETRIES))
	{
		log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
		exit(1);
		return -1;
	}
	return 0;
}

static int
db_insert_root(QUEUE *me, const char *rootkey, const char *uri)
{
	struct root_insert data;
	
	if(!rootkey || strlen(rootkey) != 32)
	{
		log_printf(LOG_CRIT, "DB: invalid root key '%s'\n", rootkey);
		abort();
	}
	data.me = me;
	data.rootkey = rootkey;
	data.uri = uri;
		
	if(sql_perform(me->db, db_insert_root_txn, &data, TXN_MAX_RETRIES))
	{
		log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
		exit(1);
		return -1;
	}
	return 0;
}

/* A transaction callback returns 0 for commit, -1 for rollback and retry, 1 for rollback successfully */
static int
db_insert_resource_txn(SQL *db, void *userdata)
{
	struct resource_insert *data;
	SQL_STATEMENT *rs;
	
	data = (struct resource_insert *) userdata;
	
	rs = sql_queryf(db, "SELECT * FROM \"crawl_resource\" WHERE \"hash\" = %Q", data->cachekey);
	if(!rs)
	{
		return -2;
	}
	if(sql_stmt_eof(rs))
	{
		if(sql_executef(db, "INSERT INTO \"crawl_resource\" (\"hash\", \"shorthash\", \"crawl_bucket\", \"cache_bucket\", \"root\", \"uri\", \"added\", \"next_fetch\") VALUES (%Q, %lu, %d, %d, %Q, %Q, NOW(), NOW())", data->cachekey, data->shortkey, (data->shortkey % data->me->ncrawlers) + 1, (data->shortkey % data->me->ncaches) + 1, data->rootkey, data->uri))
		{
			if(sql_deadlocked(db))
			{
				return -1;
			}
			return -2;
		}
	}
	else
	{
		/* XXX only update if values differ */
		if(sql_executef(db, "UPDATE \"crawl_resource\" SET \"crawl_bucket\" = %d, \"cache_bucket\" = %d WHERE \"hash\" = %Q", (data->shortkey % data->me->ncrawlers) + 1, (data->shortkey % data->me->ncaches) + 1, data->cachekey))
		{
			if(sql_deadlocked(db))
			{
				return -1;
			}
			return -2;
		}
	}
	sql_stmt_destroy(rs);
	return 1;
}

/* A transaction callback returns 0 for commit, -1 for rollback and retry, 1 for rollback successfully */
static int
db_insert_root_txn(SQL *db, void *userdata)
{
	struct root_insert *data;
	SQL_STATEMENT *rs;
	
	data = (struct root_insert *) userdata;
	
	rs = sql_queryf(db, "SELECT * FROM \"crawl_root\" WHERE \"hash\" = %Q", data->rootkey);
	if(!rs)
	{
		return -2;
	}
	if(!sql_stmt_eof(rs))
	{
		return 0;
	}
	sql_stmt_destroy(rs);
	if(sql_executef(db, "INSERT INTO \"crawl_root\" (\"hash\", \"uri\", \"added\", \"earliest_update\", \"rate\") VALUES (%Q, %Q, NOW(), NOW(), 1000)", data->rootkey, data->uri))
	{
		if(sql_deadlocked(db))
		{
			return -1;
		}
		return -2;
	}
	return 1;
}
