#include <string.h>
#include <stdio.h>
#include <libxml/xmlreader.h>
#include <sqlite3.h>

#define INSERT "INSERT OR IGNORE INTO lemmata (form, lemma, postag) VALUES (?, ?, ?);"

#define LINE_LEN 512

#define FORM_PARAM 1
#define LEMMA_PARAM 2
#define POS_PARAM 3

#define WORD_OK 0
#define WORD_FATAL_ERROR 1
#define WORD_NON_FATAL_ERROR -1

int step_reset(int a, sqlite3_stmt *stmt) /*{{{*/
{
   int rc = WORD_FATAL_ERROR;
   
   // check number of params bound
   if (3 != a) 
     {
        rc = WORD_NON_FATAL_ERROR;
        goto step_error_non_fatal;
     }
   
   // run insert stmt
   if (SQLITE_DONE != sqlite3_step(stmt)) 
     {
        fprintf(stderr, "couldn't step stmt\n");
        goto step_error;
     }
   
   rc = WORD_OK;
   
   // goto here on non-fatal error
step_error_non_fatal:

   // reset stmt for next word
   if (SQLITE_OK != sqlite3_reset(stmt)) 
     {
        fprintf(stderr, "couldn't reset stmt\n");
        rc = WORD_FATAL_ERROR;
     }

   // go here on fatal error
step_error:
   return rc;
}
/*}}}*/

int node(xmlTextReaderPtr reader, sqlite3_stmt *stmt) /*{{{*/
{
   const xmlChar *elname, *attname, *value;
   int i = 0, p, a = 0, rc = WORD_FATAL_ERROR;
   
   // get name of node
   elname = xmlTextReaderConstName(reader);
   
   // no name or name is not word, so finished
   if (NULL == elname || 
       strncmp((char *) elname, "word", 4) || 
       XML_READER_TYPE_END_ELEMENT == xmlTextReaderNodeType(reader)) 
     {
        goto node_error;
     }
   
   // loop over attributes
   while (1 == xmlTextReaderMoveToAttributeNo(reader, i ++)) 
     {
        // get name
        attname = xmlTextReaderConstName(reader);
        
        // which param does this attribute belong to?
        p = 0;
        if (0 == strncmp((char *) attname, "form", 4)) 
          {
             p = FORM_PARAM;
          }
        else if (0 == strncmp((char *) attname, "lemma", 5)) 
          {
             p = LEMMA_PARAM;
          }
        else if (0 == strncmp((char *) attname, "postag", 6)) 
          {
             p = POS_PARAM;
          }
        else 
          {
             continue;
          }

        // get value
        value = xmlTextReaderConstValue(reader);
        
        if (POS_PARAM == p) 
          {
             switch (value[0]) 
               {
                case 'n': value = (xmlChar*) "N"; break; // noun
                case 'v': value = (xmlChar*) "V"; break; // verb
                case 't': value = (xmlChar*) "T"; break; // participle
                case 'a': value = (xmlChar*) "A"; break; // adjective
                case 'd': value = (xmlChar*) "D"; break; // adverb
                case 'l': value = (xmlChar*) "L"; break; // article
                case 'g': value = (xmlChar*) "G"; break; // particle
                case 'c': value = (xmlChar*) "C"; break; // conjunction
                case 'r': value = (xmlChar*) "R"; break; // preposition
                case 'p': value = (xmlChar*) "P"; break; // pronoun
                case 'm': value = (xmlChar*) "M"; break; // numeral
                case 'i': value = (xmlChar*) "I"; break; // interjection
                case 'e': value = (xmlChar*) "E"; break; // exclamation
                case 'u': value = (xmlChar*) "U"; break; // punctuation
                default: value = (xmlChar*) ""; break;
               }
          }
        
        // bind param
        if (SQLITE_OK != sqlite3_bind_text(stmt, p, (char *) value, -1, SQLITE_TRANSIENT)) 
          {
             fprintf(stderr, "couldn't bind param %d - %s\n", p, value);
             goto node_error;
          }
        
        // remember this binding
        ++ a;
        // next attribute
        //j = xmlTextReaderMoveToAttributeNo(reader, ++ i);
     }
   
   // there were attributes, so back to parent element node
   if (i > 0) 
     {
        xmlTextReaderMoveToElement(reader);
     }
   
   // on error goto here
node_error:

   // step and reset statement
   rc = step_reset(a, stmt);
   
   return rc;
}
/*}}}*/

int textline(char *line, sqlite3_stmt *stmt) /*{{{*/
{
   int rc = WORD_FATAL_ERROR, p, a = 0, s = 0;
   char *value;
   char delims[] = " \n"; // space and new line

   // tokenise line
   value = strtok(line, delims);
   while (NULL != value) 
     {
        // which param does this token belong to?
        p = 0;
        switch (s) 
          {
             // pos
           case 1:
             p = POS_PARAM;
             break;
             
             // form
           case 4:
             p = FORM_PARAM;
             break;
             
             // lemma
           case 6:
             p = LEMMA_PARAM;
             break;
             
           default:
             break;
          }

        // have param to bind
        if (0 == p) 
          {
             goto next_token;
          }
        
        if (POS_PARAM == p) 
          {
             switch (value[0]) 
               {
                case 'N': value = (char*) "N"; break; // noun
                case 'V': value = (char*) "V"; break; // verb
                case 'A': value = (char*) "A"; break; // adjective
                case 'D': value = (char*) "D"; break; // adverb
                case 'X': value = (char*) "G"; break; // particle
                case 'P': value = (char*) "R"; break; // preposition
                case 'C': value = (char*) "C"; break; // conjunction
                case 'I': value = (char*) "I"; break; // interjection
                case 'R':
                  if ('A' == value[1]) value = (char*) "L"; // article
                  else value = (char*) "P"; // pronoun
                  break;
                //case 'l': value = (char*) "L"; break; // article
                //case 'p': value = (char*) "P"; break; // pronoun
                //case 't': value = (char*) "T"; break; // participle
                //case 'm': value = (char*) "M"; break; // numeral
                //case 'e': value = (char*) "E"; break; // exclamation
                //case 'u': value = (char*) "U"; break; // punctuation
                default: value = (char*) ""; break;
               }
          }
        
        // bind param
        if (SQLITE_OK != sqlite3_bind_text(stmt, p, value, -1, SQLITE_TRANSIENT))
          {
             fprintf(stderr, "couldn't bind param %d - %s\n", p, value);
             goto line_error;
          }
        
        // remember this binding
        ++ a;
        
        // go to next token
next_token:
        value = strtok(NULL, delims);
        ++ s;
     }
   
   // goto here on error
line_error:
   
   // step and reset statement
   rc = step_reset(a, stmt);
   
   return rc;
}
/*}}}*/

int main(int argc, char **argv) /*{{{*/
{
   int rc = 1, i, j;
   xmlTextReaderPtr reader = NULL;
   sqlite3 *db;
   sqlite3_stmt *stmt;
   FILE *fh;
   char line[LINE_LEN];
   
   // check arguments
   if (argc < 3) 
     {
        printf("Usage: %s database infile [infile]\n", argv[0]);
        goto error;
     }
   
   // open database
   if (SQLITE_OK != sqlite3_open(argv[1], &db)) 
     {
        fprintf(stderr, "couldn't open database %s\n", argv[1]);
        goto error;
     }
   
   // prepare statement
   if (SQLITE_OK != sqlite3_prepare_v2(db, INSERT, -1, &stmt, NULL)) 
     {
        fprintf(stderr, "couldn't prepare statement %s\n", INSERT);
        goto error;
     }
   
   // start transaction
   if (SQLITE_OK != sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL)) 
     {
        fprintf(stderr, "couldn't start transaction");
        goto error;
     }
   
   // loop over files
   for (j = 2; j < argc; ++ j) 
     {
        // XML file
        if (NULL != strstr(argv[j], ".xml")) 
          {
             // first file
             if (NULL == reader) 
               {
                  reader = (xmlTextReaderPtr) xmlReaderForFile(argv[j], NULL, 0);
               }
             // subsequent files
             else 
               {
                  xmlReaderNewFile(reader, argv[j], NULL, 0);
               }
             
             // failed to create reader
             if (NULL == reader) 
               {
                  fprintf(stderr, "problem creating reader\n");
                  goto error;
               }
             
             // loop over nodes in XML document
             i = xmlTextReaderRead(reader);
             while (1 == i) 
               {
                  if (WORD_FATAL_ERROR == node(reader, stmt)) 
                    {
                       fprintf(stderr, "node returned %d\n", WORD_FATAL_ERROR);
                       goto error;
                    }
                  
                  i = xmlTextReaderRead(reader);
               }

             // check final return value from reader
             if (0 != i) 
               {
                  fprintf(stderr, "couldn't parse %s\n", argv[j]);
                  goto error;
               }
          }
        // TEXT file
        else if (NULL != strstr(argv[j], ".txt")) 
          {
             // open file
             fh = fopen(argv[j], "r");
             
             // failed
             if (NULL == fh) 
               {
                  fprintf(stderr, "opening %s failed\n", argv[j]);
                  goto error;
               }
             
             // loop over lines in text file
             while (NULL != fgets(line, LINE_LEN, fh)) 
               {
                  if (WORD_FATAL_ERROR == textline(line, stmt)) 
                    {
                       fprintf(stderr, "textline returned %d\n", WORD_FATAL_ERROR);
                       goto error;
                    }
               }
             
             // close file
             fclose(fh);
          }
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
   sqlite3_finalize(stmt);
   sqlite3_close(db);
   
   // success
   rc = 0;

   // on error goto here
error:
   return rc;
}
/*}}}*/