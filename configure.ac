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

AC_INIT([crawl],[master])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE
AC_CONFIG_HEADER([config.h])

BT_PROG_XCODE
AC_PROG_CC
BT_PROG_CC_WARN
LT_INIT
BT_DEFINE_PREFIX

dnl save and restore LIBS -- each set will be used explicitly where needed
save_LIBS="$LIBS"
BT_REQUIRE_OPENSSL
BT_REQUIRE_LIBCURL
BT_REQUIRE_LIBURI_INCLUDED
BT_REQUIRE_LIBJSONDATA_INCLUDED
BT_REQUIRE_LIBXML2
LIBS="$save_LIBS"

AC_CONFIG_FILES([
Makefile
util/Makefile
m4/Makefile
])

AC_OUTPUT
