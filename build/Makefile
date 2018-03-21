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
READER_SRC = reader.c
READER_OBJS = $(READER_SRC:%.c=%.o)
READER = ./reader

# ingest
INGEST_SRC = ingest.c
INGEST_OBJS = $(INGEST_SRC:%.c=%.o)
INGEST = ./ingest

# collocations
COLLOC_SRC = collocations.c
COLLOC_OBJS = $(COLLOC_SRC:%.c=%.o)
COLLOC = ./collocations

# # debug
DBG = 

# compiler options
CC = gcc
CFLAGS = $(shell xml2-config --cflags)
LIBS = $(shell xml2-config --libs) -lsqlite3
OPTS = -Wall -O3 -g $(DBG)

# .docx documents
DATA_DIR = /home/cs2/greek/data
DOCX_DOCS = $(wildcard $(DATA_DIR)/*.docx)
DOCX_TXT = $(DOCX_DOCS:%.docx=%.txt)
TEI_DOCS = $(DOCX_DOCS:%.docx=%.xml)

# CWB info
CWB_DIR = /home/cwb/cwb_files
CWB_PROJECT = greek_athletes
CORPUS_DIR = $(CWB_DIR)/index/$(CWB_PROJECT)
CORPUS_UPLOADS = $(CWB_DIR)/uploads
REGISTRY_DIR = $(CWB_DIR)/registry
CORPUS_XML = $(CORPUS_UPLOADS)/$(CWB_PROJECT).vrt
CORPUS_METADATA = $(CORPUS_UPLOADS)/$(CWB_PROJECT).csv
REGISTRY = $(REGISTRY_DIR)/$(CWB_PROJECT)
CORPUS_NAME = $(shell echo $(CWB_PROJECT) | tr a-z A-Z)

# attributes
P_ATT = -P pos -P lemma
S_ATT = -S s:0 -S text:0+id+title+from+to+settlement+region+person
V_ATT = 

# CWB programs
BIN_DIR = /usr/local/cwb-3.4.11/bin/
MAKEALL = $(BIN_DIR)cwb-makeall
ENCODE = $(BIN_DIR)cwb-encode

# convertions - .docx -> .txt > .xml > .vrt
CONVERT_TO_TXT = libreoffice --convert-to "txt:Text (encoded):UTF8" --outdir $(DATA_DIR) --headless
CONVERT_TO_TEI = ./text2xml.pl
CONVERT_TO_VRT = ./xml2cwb.pl

# get corpus ID
CORPUS_ID = $(shell echo "INSERT OR IGNORE INTO corpora (corpus_id, corpus_name) VALUES (NULL, '$(CWB_PROJECT)'); SELECT corpus_id FROM corpora WHERE corpus_name = '$(CWB_PROJECT)';" | sqlite3 $(DB))

.PHONY: clean

all : $(READER) $(DB) ingest_xml.touch ingest_txt.touch convert_docx.touch $(INGEST) ingest_tei.touch $(COLLOC)

# compile reader from object files
$(READER) : $(READER_OBJS)
	$(CC) $(LIBS) $(OPTS) -o $@ $<

# compile object files from C files
$(READER_OBJS) $(INGEST_OBJS) $(COLLOC_OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(OPTS) -c -o $@ $<

# compile ingest from object files
$(INGEST) : $(INGEST_OBJS)
	$(CC) $(LIBS) $(OPTS) -o $@ $<

# # compile collocations from object files
$(COLLOC) : $(COLLOC_OBJS)
	$(CC) $(LIBS) $(OPTS) -o $@ $<

# ingest XML corpus documents
ingest_xml.touch : $(XML_DOCS)
	$(READER) $(DB) $^
	touch $@

# ingest text corpus documents
ingest_txt.touch : $(TEXT_DOCS)
	$(READER) $(DB) $^
	touch $@

# create database
$(DB) : $(SCHEMA)
	sqlite3 $@ < $^

# convert .docx to .txt
convert_docx.touch : $(DOCX_DOCS)
	$(CONVERT_TO_TXT) $^
	touch $@

# convert .txt to .xml (TEI)
# use sed to remove control characters
$(TEI_DOCS) : %.xml : %.txt
	sed -e "s/\x07//g" -e "s/\x08//g" $< | $(CONVERT_TO_TEI) > $@

# convert .xml files to single .vrt
$(CORPUS_XML) : $(TEI_DOCS)
	$(CONVERT_TO_VRT) $(DB) $@ $(CORPUS_METADATA) $^

makeall : encode
	$(MAKEALL) -r $(REGISTRY_DIR)/ -V $(CORPUS_NAME)

encode : $(CORPUS_XML)
	mkdir -p $(CORPUS_DIR)
	$(ENCODE) -d $(CORPUS_DIR) -f $< -R $(REGISTRY) -c utf8 -xsB $(P_ATT) $(V_ATT) $(S_ATT) -0 corpus

# ingest words and metadata from TEI into database
ingest_tei.touch : $(TEI_DOCS)
	$(INGEST) $(DB) $(CORPUS_ID) $^
	touch $@

clean:
	$(RM) $(READER) $(READER_OBJS) $(DB) $(DOCX_TXT)