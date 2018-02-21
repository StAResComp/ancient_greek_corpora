-- -*- mysql -*-

-- schema for database containing AG words and their POS, lemmata

DROP TABLE IF EXISTS lemmata;
CREATE TABLE lemmata (
  form TEXT PRIMARY KEY,
  lemma TEXT,
  postag TEXT
);

-- CREATE INDEX posIdx ON words(postag);

DROP TABLE IF EXISTS texts;
CREATE TABLE texts (
  text_id INTEGER PRIMARY KEY,
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