sbin_PROGRAMS = crawld

noinst_LTLIBRARIES = libiniparser.la

libiniparser_la_SOURCES = \
        iniparser/src/dictionary.h \
        iniparser/src/dictionary.c \
        iniparser/src/iniparser.h \
        iniparser/src/iniparser.c

libiniparser_la_LDFLAGS = -static

crawld_SOURCES = p_crawld.h \
	crawld.c

crawld_CPPFLAGS = @AM_CPPFLAGS@ @CPPFLAGS@ -I$(srcdir)/iniparser/src
crawld_LDADD = ../libcrawl.la libiniparser.la


