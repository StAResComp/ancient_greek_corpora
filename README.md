# Ingesting Ancient Greek corpora

The program `reader` reads in words in XML and text files and adds
them to a SQLite database.

The XML files come from [Perseus Treebank
data](https://github.com/PerseusDL/treebank_data). Words are in `word`
elements, where the significant attributes are `form`, `lemma` and
`postag` (part of speech).

The text files come from [SBL Greek New
Testament](https://github.com/morphgnt/sblgnt). The files contain
space delimited information. The relavent columns are PoS (second),
the word (fifth) and lemma (seventh).

This information is inserted into a SQLite database, using the word as
the primary key.