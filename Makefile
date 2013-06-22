OUT = crawler
OBJ = fetch.o crawler.o cache.o context.o

CFLAGS = $(CPPFLAGS) -O0 -g
CPPFLAGS = -W -Wall $(CURL_CPPFLAGS) $(OPENSSL_CPPFLAGS)
LIBS = $(CURL_LIBS) $(OPENSSL_LIBS)

CURL_CPPFLAGS = $(shell curl-config --cflags)
CURL_LIBS = $(shell curl-config --libs)

OPENSSL_CPPFLAGS = $(shell pkg-config --cflags openssl)
OPENSSL_LIBS = $(shell pkg-config --libs openssl)

$(OUT): $(OBJ)
	$(CC) $(LDFLAGS) -o $(OUT) $(OBJ) $(LIBS)

clean:
	rm -f $(OUT) $(OBJ)
