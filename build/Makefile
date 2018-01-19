# -*- make -*-

# database
DB = words.db
SCHEMA = schema.sql

# XML documents
XML_DOC_DIR = /home/cs2/greek/treebank_data/v2.0/Greek/nonArethusaCompliant
XML_DOCS = $(wildcard $(XML_DOC_DIR)/*.xml)

# TEXT documents
TEXT_DOC_DIR = /home/cs2/greek/sblgnt
TEXT_DOCS = $(wildcard $(TEXT_DOC_DIR)/*.txt)

# XML reader
READER_SRC = $(wildcard *.c)
READER_OBJS = $(READER_SRC:%.c=%.o)
READER = ./reader

# compiler options
CC = gcc
CFLAGS = $(shell xml2-config --cflags)
LIBS = $(shell xml2-config --libs) -lsqlite3
OPTS = -Wall -O3

.PHONY: clean

all : $(READER) $(DB) ingest_xml ingest_txt

# compile reader from object files
$(READER) : $(READER_OBJS)
	$(CC) $(LIBS) $(OPTS) -o $@ $<

# compile object files from C files
$(READER_OBJS) : %.o : %.c
	$(CC) $(CFLAGS) $(OPTS) -c -o $@ $<

ingest_xml : $(XML_DOCS)
	$(READER) $(DB) $^

ingest_txt : $(TEXT_DOCS)
	$(READER) $(DB) $^

$(DB) : $(SCHEMA)
	sqlite3 $@ < $^

clean:
	$(RM) $(READER) $(READER_OBJS) $(DB)