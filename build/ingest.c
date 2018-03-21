#include <string.h>
#include <stdio.h>
#include <libxml/xmlreader.h>
#include <sqlite3.h>
#include <libgen.h>
#include <stdlib.h>
#include <regex.h>

// SQL statements
#define INSERT_TEXT "INSERT INTO texts (text_id, text_name, title, settlement, region, earliest, latest, person, corpus_id) VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?);"
#define INSERT_LINE "INSERT INTO lines (line_id, text_id, line_number) VALUES (NULL, ?, ?);"
#define INSERT_UNIQUE "INSERT OR IGNORE INTO unique_words (unique_id, form) VALUES (NULL, ?);"
#define SELECT_UNIQUE "SELECT unique_id FROM unique_words WHERE form = ?;"
#define INSERT_WORD "INSERT INTO words (word_id, line_id, word_number, unique_id) VALUES (NULL, ?, ?, ?);"

#define TEXT_NAME 1
#define TEXT_TITLE 2
#define TEXT_SETTLEMENT 3
#define TEXT_REGION 4
#define TEXT_EARLIEST 5
#define TEXT_LATEST 6
#define TEXT_PERSON 7
#define TEXT_CORPUS_ID 8
#define TEXT_WHEN 9

#define WORD_W 1
#define WORD_P 2

#define MAX_WORD_LEN 1024

#define INGEST_OK 1
#define INGEST_END 0
#define INGEST_ERROR -1

#define REGEX_GREEK "[Ͱ-Ͽ]"

#ifdef DBG
# define DEBUG(str) fprintf(stderr, "%s\n", str)
#else
# define DEBUG(str)
#endif

// global variables
sqlite3 *db;
sqlite3_stmt *insert_text_stmt;
sqlite3_stmt *insert_line_stmt;
sqlite3_stmt *insert_unique_stmt;
sqlite3_stmt *select_unique_stmt;
sqlite3_stmt *insert_word_stmt;

regex_t greek;

// trim form of any whitespace
char *trimForm(char *form) /*{{{*/
{
     int i, j, l;
     char *newForm;

     l = strlen(form);
     j = 0;

     newForm = (char *) malloc(sizeof(char) * (l + 1));
     if (NULL == newForm)
     {
          fprintf(stderr, "couldn't allocate memory for trimmed form\n");
          goto error;
     }

     for (i = 0; i < l; ++ i)
     {
          switch (form[i])
          {
          case ' ':
          case '\n':
          case '\r':
          case '\t':
               break;

          default:
               newForm[j] = form[i];
               ++ j;
               break;
          }
     }

     newForm[j] = '\0';

error:

     return newForm;
}
/*}}}*/

int xmlword(xmlTextReaderPtr reader, int line_id, int word_number) /*{{{*/
{
     int rc = INGEST_ERROR, i, form_l = 0, w_l = 0, c_l = MAX_WORD_LEN, p,
          unique_id;
     char *form, *newForm = NULL, *elname, *w;

     // allocate memory for form
     form = (char *) malloc(sizeof(char) * c_l);
     if (NULL == form)
     {
          fprintf(stderr, "Error allocating memory\n");
          goto xmlword_error;
     }
     form[0] = '\0';

     i = xmlTextReaderRead(reader);
     while (1 == i)
     {
          // closing element
          if (XML_READER_TYPE_END_ELEMENT == xmlTextReaderNodeType(reader))
          {
               i = xmlTextReaderRead(reader);
               continue;
          }

          elname = (char *) xmlTextReaderConstName(reader);
          p = 0;

          // next word
          if (0 == strncmp(elname, "w", 2))
          {
               DEBUG("next word");
               p = WORD_W;
               break;
          }
          // on next line, so finished with words for this line
          else if (0 == strncmp(elname, "p", 2))
          {
               DEBUG("end of words in p");
               p = WORD_P;
               break;
          }
          // node inside word, so build up form
          else
          {
               DEBUG("inside word");
               DEBUG(elname);
               w = (char *) xmlTextReaderValue(reader);

               if (NULL != w)
               {
                    DEBUG(w);
                    w_l = strlen(w);

                    // need to expand space for form
                    if (w_l + form_l > c_l)
                    {
                         c_l += MAX_WORD_LEN;
                         form = (char *) realloc(form, sizeof(char) * c_l);
                         if (NULL == form)
                         {
                              fprintf(stderr, "Error reallocating memory\n");
                              goto xmlword_error;
                         }
                    }

                    // concat w to form
                    form_l += w_l;
                    form = strncat(form, w, w_l);
                    DEBUG(form);

                    // finished with w
                    xmlFree(w);
               }

               // next node
               i = xmlTextReaderRead(reader);
          }
     }

     // have form to add to database
     if (NULL != form)
     {
          // trim whitespace from form
          newForm = trimForm(form);
          if (NULL == newForm)
          {
               goto xmlword_error;
          }

          unique_id = 0;

          // run regular expression
          // only look for form in unique_words if it contains some Greek
          if (0 == regexec(&greek, newForm, 0, NULL, 0))
          {
               // try to insert unique form
               if (SQLITE_OK != sqlite3_bind_text(insert_unique_stmt, 1, newForm, -1, SQLITE_TRANSIENT) ||
                   SQLITE_DONE != sqlite3_step(insert_unique_stmt) ||
                   SQLITE_OK != sqlite3_reset(insert_unique_stmt))
               {
                    fprintf(stderr, "Couldn't insert unique form %s\n", newForm);
                    goto xmlword_error;
               }

               // get unique_id of form
               if (SQLITE_OK != sqlite3_bind_text(select_unique_stmt, 1, newForm, -1, SQLITE_TRANSIENT) ||
                   SQLITE_ROW != sqlite3_step(select_unique_stmt))
               {
                    fprintf(stderr, "Couldn't select unique form %s\n", newForm);
                    goto xmlword_error;
               }

               unique_id = sqlite3_column_int(select_unique_stmt, 0);

               if (SQLITE_OK != sqlite3_reset(select_unique_stmt))
               {
                    fprintf(stderr, "Couldn't reset unique select %s\n", newForm);
                    goto xmlword_error;
               }
          }

          // insert this word
          if (SQLITE_OK != sqlite3_bind_int(insert_word_stmt, 1, line_id) ||
              SQLITE_OK != sqlite3_bind_int(insert_word_stmt, 2, word_number) ||
              SQLITE_OK != sqlite3_bind_int(insert_word_stmt, 3, unique_id) ||
              SQLITE_DONE != sqlite3_step(insert_word_stmt) ||
              SQLITE_OK != sqlite3_reset(insert_word_stmt))
          {
               fprintf(stderr, "Couldn't insert word %s\n", newForm);
               goto xmlword_error;
          }
          DEBUG("have form");
          DEBUG(newForm);

          free(form);
          free(newForm);
     }

     // go to next word
     if (WORD_W == p)
     {
          rc = xmlword(reader, line_id, word_number + 1);
     }
     // on next line, so success
     else if (WORD_P == p)
     {
          rc = INGEST_OK;
     }
     // end of document
     else if (0 == i)
     {
          DEBUG("end of document");
          rc = INGEST_END;
     }

     // go here on error
xmlword_error:

     return rc;
}
/*}}}*/

int xmlline(xmlTextReaderPtr reader, int text_id, int line_number) /*{{{*/
{
     int rc = INGEST_ERROR, line_id, i, word_number = 1;
     char *elname;

     // insert line
     if (SQLITE_OK != sqlite3_bind_int(insert_line_stmt, 1, text_id) ||
         SQLITE_OK != sqlite3_bind_int(insert_line_stmt, 2, line_number) ||
         SQLITE_DONE != sqlite3_step(insert_line_stmt) ||
         SQLITE_OK != sqlite3_reset(insert_line_stmt))
     {
          fprintf(stderr, "Couldn't bind line params\n");
          goto xmlline_error;
     }

     // get ID for line
     line_id = sqlite3_last_insert_rowid(db);

     i = xmlTextReaderRead(reader);
     while (1 == i)
     {
          // get name of element
          elname = (char *) xmlTextReaderConstName(reader);
          DEBUG(elname);

          // closing element
          if (XML_READER_TYPE_END_ELEMENT == xmlTextReaderNodeType(reader))
          {
               i = xmlTextReaderRead(reader);
               continue;
          }

          // on word
          if (0 == strncmp(elname, "w", 1))
          {
               DEBUG("starting words in line");
               rc = xmlword(reader, line_id, word_number ++);
               if (INGEST_OK != rc)
               {
                    DEBUG("xmlword finished");
                    goto xmlline_error;
               }
          }
          // on next line
          else if (0 == strncmp(elname, "p", 1))
          {
               DEBUG("next line");
               rc = xmlline(reader, text_id, line_number + 1);
               if (INGEST_OK != rc)
               {
                    DEBUG("xmlline finished");
                    goto xmlline_error;
               }

               break;
          }
          // otherwise next node
          else
          {
               i = xmlTextReaderRead(reader);
          }
     }

     // go here on error
xmlline_error:
     return rc;
}
/*}}}*/

// insert text metadata
int xmlfile(xmlTextReaderPtr reader, char *text_name, int corpus_id) /*{{{*/
{
     const char *elname = NULL;
     char *value;
     int i, a = 0, rc = INGEST_ERROR, p, t = 1, j, text_id;

     if (SQLITE_OK != sqlite3_bind_text(insert_text_stmt, TEXT_NAME,
                                        text_name, -1, SQLITE_TRANSIENT) ||
         SQLITE_OK != sqlite3_bind_int(insert_text_stmt, TEXT_CORPUS_ID,
                                      corpus_id))
     {
          fprintf(stderr, "Couldn't bind param %d/%d, %s/%d\n", 
                  TEXT_NAME, TEXT_CORPUS_ID, text_name, corpus_id);
          goto xmlfile_error;
     }

     i = xmlTextReaderRead(reader);
     while (1 == i)
     {
          // closing element
          if (XML_READER_TYPE_END_ELEMENT == xmlTextReaderNodeType(reader))
          {
               i = xmlTextReaderRead(reader);
               continue;
          }

          // get name of element
          elname = (char *) xmlTextReaderConstName(reader);
          p = 0;

          if (0 == strncmp(elname, "title", 6))
          {
               p = TEXT_TITLE;
          }
          else if (0 == strncmp(elname, "settlement", 10))
          {
               p = TEXT_SETTLEMENT;
          }
          else if (0 == strncmp(elname, "region", 6))
          {
               p = TEXT_REGION;
          }
          // date, so start looking at attributes
          else if (0 == strncmp(elname, "date", 4))
          {
               i = xmlTextReaderMoveToAttributeNo(reader, a ++);
               continue;
          }
          // date attributes
          else if (0 == strncmp(elname, "notBefore", 9))
          {
               p = TEXT_EARLIEST;
          }
          else if (0 == strncmp(elname, "notAfter", 8))
          {
               p = TEXT_LATEST;
          }
          else if (0 == strncmp(elname, "when", 4))
          {
               p = TEXT_WHEN;
          }
          else if (0 == strncmp((char *) elname, "person", 6))
          {
               p = TEXT_PERSON;
          }
          // got to p element, so break
          else if (0 == strncmp((char *) elname, "p", 2))
          {
               break;
          }

          // nothing matched this node, so continue with next
          if (0 == p)
          {
               i = xmlTextReaderRead(reader);
               continue;
          }

          // remember this match
          t |= 1 << p;

          // see what was matched
          switch (p)
          {
          case TEXT_EARLIEST:
          case TEXT_LATEST:
               value = (char *) xmlTextReaderValue(reader);

               if (SQLITE_OK != sqlite3_bind_int(insert_text_stmt, p,
                                                 atoi(value)))
               {
                    fprintf(stderr, "Couldn't bind param %d, %s\n", p, value);
                    goto xmlfile_error;
               }

               // if can't move to next attribute, move back to element
               if (1 != xmlTextReaderMoveToAttributeNo(reader, a ++))
               {
                    xmlTextReaderMoveToElement(reader);
                    i = xmlTextReaderRead(reader);
               }

               break;

          case TEXT_WHEN:
               value = (char *) xmlTextReaderValue(reader);

               if (SQLITE_OK != sqlite3_bind_int(insert_text_stmt, TEXT_EARLIEST,
                                                 atoi(value)) ||
                   SQLITE_OK != sqlite3_bind_int(insert_text_stmt, TEXT_LATEST,
                                                 atoi(value)))
               {
                    fprintf(stderr, "Couldn't bind param %d, %s\n", p, value);
                    goto xmlfile_error;
               }

               // if can't move to next attribute, move back to element
               if (1 != xmlTextReaderMoveToAttributeNo(reader, a ++))
               {
                    xmlTextReaderMoveToElement(reader);
                    i = xmlTextReaderRead(reader);
               }

               break;

          case TEXT_TITLE:
          case TEXT_SETTLEMENT:
          case TEXT_REGION:
          case TEXT_PERSON:
               // move to text node in element and get value
               if (0 == xmlTextReaderIsEmptyElement(reader))
               {
                    i = xmlTextReaderRead(reader);
               }

               value = (char *) xmlTextReaderValue(reader);

               if (SQLITE_OK != sqlite3_bind_text(insert_text_stmt, p,
                                                  value, -1, SQLITE_TRANSIENT))
               {
                    fprintf(stderr, "Couldn't bind param %d, %s\n", p, value);
                    goto xmlfile_error;
               }

               // fall through
               // and move to next element
          default:
               i = xmlTextReaderRead(reader);
               break;
          }

          xmlFree(value);
     }

     // check for missing params
     for (j = TEXT_TITLE; j <= TEXT_PERSON; ++ j)
     {
          // this param not set, so bind NULL
          if (0 == (t & 1 << j) &&
              SQLITE_OK != sqlite3_bind_null(insert_text_stmt, j))
          {
               fprintf(stderr, "Couldn't bind param %d, null\n", j);
               goto xmlfile_error;
          }
     }

     // step and reset statement
     if (SQLITE_DONE != sqlite3_step(insert_text_stmt))
     {
          fprintf(stderr, "Couldn't step intest text statement\n");
          goto xmlfile_error;
     }
     if (SQLITE_OK != sqlite3_reset(insert_text_stmt))
     {
          fprintf(stderr, "Couldn't reset ingest text statement\n");
          goto xmlfile_error;
     }

     // get ID for text
     text_id = sqlite3_last_insert_rowid(db);

     // success depends on inserting lines
     rc = xmlline(reader, text_id, 1);

xmlfile_error:
     return rc;
}
/*}}}*/

int main (int argc, char **argv) /*{{{*/
{
     int rc = 1, j, corpus_id;
     xmlTextReaderPtr reader = NULL;
     char *path, *text_name, *o;

     // check arguments
     if (argc < 3)
     {
          printf("Usage: %s database corpus_id xmlfile [xmlfile ...]\n", argv[0]);
          goto error;
     }

     // open database
     if (SQLITE_OK != sqlite3_open(argv[1], &db))
     {
          fprintf(stderr, "couldn't open database %s\n", argv[1]);
          goto error;
     }
     
     // get corpus_id
     corpus_id = atoi(argv[2]);

     // prepare statements
     if (SQLITE_OK != sqlite3_prepare_v2(db, INSERT_TEXT, -1, &insert_text_stmt, NULL))
     {
          fprintf(stderr, "couldn't prepare statement %s\n", INSERT_TEXT);
          goto error;
     }
     if (SQLITE_OK != sqlite3_prepare_v2(db, INSERT_LINE, -1, &insert_line_stmt, NULL))
     {
          fprintf(stderr, "couldn't prepare statement %s\n", INSERT_LINE);
          goto error;
     }
     if (SQLITE_OK != sqlite3_prepare_v2(db, INSERT_WORD, -1, &insert_word_stmt, NULL))
     {
          fprintf(stderr, "couldn't prepare statement %s\n", INSERT_WORD);
          goto error;
     }
     if (SQLITE_OK != sqlite3_prepare_v2(db, INSERT_UNIQUE, -1, &insert_unique_stmt, NULL))
     {
          fprintf(stderr, "couldn't prepare statement %s\n", INSERT_UNIQUE);
          goto error;
     }
     if (SQLITE_OK != sqlite3_prepare_v2(db, SELECT_UNIQUE, -1, &select_unique_stmt, NULL))
     {
          fprintf(stderr, "couldn't prepare statement %s\n", SELECT_UNIQUE);
          goto error;
     }

     // start transaction
     if (SQLITE_OK != sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL))
     {
          fprintf(stderr, "couldn't start transaction\n");
          goto error;
     }

     // compile regex
     if (regcomp(&greek, REGEX_GREEK, REG_NOSUB))
     {
          fprintf(stderr, "couldn't compile regular expression\n");
          goto error;
     }

     // loop over XML files
     for (j = 3; j < argc; ++ j)
     {
          // first file
          if (NULL == reader)
          {
               reader = (xmlTextReaderPtr) xmlReaderForFile(argv[j], NULL, 0);
          }
          // subsequent file, so reuse reader
          else
          {
               xmlReaderNewFile(reader, argv[j], NULL, 0);
          }

          // check that reader was created
          if (NULL == reader)
          {
               fprintf(stderr, "problem creating reader for %s\n", argv[j]);
               goto error;
          }

          // get basename of file
          path = strdup(argv[j]);
          text_name = basename(path);
          o = strstr(text_name, ".");
          if (NULL != o)
          {
               text_name[o - text_name] = '\0';
          }

          if (INGEST_END != xmlfile(reader, text_name, corpus_id))
          {
               fprintf(stderr, "Error processing %s\n", text_name);
               //goto error;
          }

          // free path (because of strdup)
          free(path);
     }

     // commit transaction
     if (SQLITE_OK != sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL))
     {
          fprintf(stderr, "couldn't commit transaction");
     }

     // clean up
     if (NULL != reader)
     {
          xmlFreeTextReader(reader);
     }

     xmlCleanupParser();

     sqlite3_finalize(insert_text_stmt);
     sqlite3_finalize(insert_line_stmt);
     sqlite3_finalize(insert_word_stmt);
     sqlite3_close(db);

     regfree(&greek);

     // success
     rc = 0;

     // on error goto here
error:

     return rc;
}
/*}}}*/
