-- -*- mysql -*-

-- schema for database containing AG words and their POS, lemmata

DROP TABLE IF EXISTS lemmata;
CREATE TABLE lemmata (
  form TEXT PRIMARY KEY,
  lemma TEXT,
  postag TEXT
);

-- CREATE INDEX posIdx ON words(postag);

DROP TABLE IF EXISTS corpora;
CREATE TABLE corpora (
  corpus_id INTEGER PRIMARY KEY,
  corpus_name TEXT UNIQUE
);

DROP TABLE IF EXISTS texts;
CREATE TABLE texts (
  text_id INTEGER PRIMARY KEY,
  corpus_id INTEGER,
  text_name TEXT,
  title TEXT,
  settlement TEXT,
  region TEXT,
  earliest INTEGER,
  latest INTEGER,
  person TEXT
);

DROP TABLE IF EXISTS lines;
CREATE TABLE lines (
  line_id INTEGER PRIMARY KEY,
  text_id INTEGER,
  line_number INTEGER
);

DROP TABLE IF EXISTS words;
CREATE TABLE words (
  word_id INTEGER PRIMARY KEY,
  line_id INTEGER,
  word_number INTEGER,
  unique_id INTEGER
);

DROP TABLE IF EXISTS unique_words;
CREATE TABLE unique_words (
  unique_id INTEGER PRIMARY KEY,
  form TEXT UNIQUE
);

CREATE INDEX uniqueIdx on words(unique_id);
CREATE INDEX lineUniqueIdx on words(line_id, unique_id);

DROP TABLE IF EXISTS collocation_definitions;
CREATE TABLE collocation_definitions (
  collocation_def_id INTEGER PRIMARY KEY,
  corpus_id INTEGER,
  min_occurrences INTEGER,
  min_length INTEGER
);

DROP TABLE IF EXISTS collocations;
CREATE TABLE collocations (
  collocation_id INTEGER PRIMARY KEY,
  collocation_def_id INTEGER
);

DROP TABLE IF EXISTS collocation_words;
CREATE TABLE collocation_words (
  collocation_id INTEGER,
  unique_id INTEGER
);

DROP TABLE IF EXISTS collocation_lines;
CREATE TABLE collocation_lines (
  collocation_id INTEGER,
  line_id INTEGER
);
