## Copyright 2013 Mo McRoberts.
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##      http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##

DIST_SUBDIRS = m4 util liburi jsondata

SUBDIRS = @subdirs@ . util

ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libcrawl.la

include_HEADERS = crawl.h

libcrawl_la_SOURCES = p_libcrawl.h \
	context.c cache.c fetch.c obj.c crawler.c

libcrawl_la_LDFLAGS = -avoid-version

libcrawl_la_LIBADD = $(LIBJSONDATA_LOCAL_LIBS) $(LIBJSONDATA_LIBS) \
	$(LIBURI_LOCAL_LIBS) $(LIBURI_LIBS) \
	$(LIBCURL_LOCAL_LIBS) $(LIBCURL_LIBS) \
	$(OPENSSL_LOCAL_LIBS) $(OPENSSL_LIBS)