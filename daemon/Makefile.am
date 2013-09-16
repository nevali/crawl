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

sbin_PROGRAMS = crawld

crawld_SOURCES = p_crawld.h \
	crawld.c context.c thread.c processor.c queue.c policy.c \
	rdf.c db.c

crawld_CPPFLAGS = @AM_CPPFLAGS@ @CPPFLAGS@ -I$(top_srcdir)/libsupport $(LIBRDF_CPPFLAGS) $(LIBSQL_CPPFLAGS)
crawld_LDADD = ../libcrawl.la ../libsupport/libsupport.la -lpthread $(LIBRDF_LIBS) $(LIBSQL_LOCAL_LIBS) $(LIBSQL_LIBS)


