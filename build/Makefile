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

# .docx documents
DATA_DIR = /home/cs2/greek/data
DOCX_DOCS = $(wildcard $(DATA_DIR)/*.docx)
DOCX_TXT = $(DOCX_DOCS:%.docx=%.txt)
TEI_DOCS = $(DOCX_DOCS:%.docx=%.xml)

CONVERT_TO_TXT = libreoffice --convert-to "txt:Text (encoded):UTF8" --outdir $(DATA_DIR) --headless

CONVERT_TO_TEI = ./text2xml.pl

.PHONY: clean

all : $(READER) $(DB) ingest_xml ingest_txt convert_docx $(TEI_DOCS)

# compile reader from object files
$(READER) : $(READER_OBJS)
	$(CC) $(LIBS) $(OPTS) -o $@ $<

# compile object files from C files
$(READER_OBJS) : %.o : %.c
	$(CC) $(CFLAGS) $(OPTS) -c -o $@ $<

ingest_xml : $(XML_DOCS)
	$(READER) $(DB) $^
	touch $@

ingest_txt : $(TEXT_DOCS)
	$(READER) $(DB) $^
	touch $@

$(DB) : $(SCHEMA)
	sqlite3 $@ < $^

convert_docx : $(DOCX_DOCS)
	$(CONVERT_TO_TXT) $^
	touch $@

$(TEI_DOCS) : %.xml : %.txt
	$(CONVERT_TO_TEI) < $< > $@

clean:
	$(RM) $(READER) $(READER_OBJS) $(DB) $(DOCX_TXT)