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

static int config_defaults(void);

int
main(int argc, char **argv)
{
	char *t;
	
	t = strrchr(argv[0], '/');
	if(t)
	{
		t++;
	}
	else
	{
		t = argv[0];
	}
	log_set_ident(t);
	log_set_stderr(1);
	log_set_facility(LOG_DAEMON);
	log_set_level(LOG_NOTICE);
	config_init(config_defaults);
	if(argc > 1)
	{
		config_set("global:configFile", argv[1]);
	}	
	if(config_load(NULL))
	{
		return 1;
	}
	log_set_use_config(1);
	log_reset();
	policy_init();
	queue_init();
	processor_init();

	/* Perform a single thread's crawl actions */
	thread_create(0);
	
	processor_cleanup();
	queue_cleanup();
	return 0;
}

static int
config_defaults(void)
{
	config_set_default("log:ident", "crawld");
	config_set_default("log:facility", "daemon");
	config_set_default("log:level", "notice");
	/* Log to stderr initially, until daemonized */
	config_set_default("log:stderr", "1");
	config_set_default("global:configFile", SYSCONFDIR "/crawl.conf");
	return 0;
}
