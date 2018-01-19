-- -*- mysql -*-

-- schema for database containing AG words and their POS, lemmata

DROP TABLE IF EXISTS words;
CREATE TABLE words (
  form TEXT PRIMARY KEY,
  lemma TEXT,
  postag TEXT
);

CREATE INDEX posIdx ON words(postag);