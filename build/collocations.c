#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>

#define WORD_LIST_STMT "SELECT unique_id FROM words INNER JOIN unique_words USING (unique_id) GROUP BY unique_id HAVING COUNT(*) >= ?;"

#define SAME_LINE 1
#define ADJACENT_W 2
#define ADJACENT_L 3

#define MAX_WORDS 1024
#define MAX_MATCHES 1024
#define MAX_SQL_LEN 1024

sqlite3 *db;
sqlite3_stmt *word_list_stmt;

// generate SQL for same_line_stmt
// returns NULL on error
char *generate_sameline_sql(int min_length) /*{{{*/
{
   // fixed strings
   char select[] = "SELECT line_id FROM words AS w1 ", 
     join_fmt[] = " INNER JOIN words AS w%d USING (line_id) ", 
     where[] = " WHERE w1.unique_id = ? ",
     and_where_fmt[] = " AND w%d.unique_id = ? ",
     group[] = " GROUP BY line_id;";
   
   char *stmt, *join, *and_where;
   int i;

   // allocate memory for stmt
   stmt = (char *) malloc(sizeof(char) * MAX_SQL_LEN);
   join = (char *) malloc(sizeof(char) * MAX_SQL_LEN);
   and_where = (char *) malloc(sizeof(char) * MAX_SQL_LEN);
   
   if (NULL == stmt || NULL == join || NULL == and_where) 
     {
        fprintf(stderr, "couldn't allocate memory for stmt\n");
        goto error;
     }
   
   stmt[0] = '\0';
   join[0] = '\0';
   and_where[0] = '\0';

   // put together statement
   // initial select
   stmt = strcat(stmt, select);
   
   // joins
   for (i = 2; i <= min_length; ++ i) 
     {
        sprintf(join, join_fmt, i);
        stmt = strcat(stmt, join);
     }

   // initial where
   stmt = strcat(stmt, where);

   // and_where clauses
   for (i = 2; i <= min_length; ++ i) 
     {
        sprintf(and_where, and_where_fmt, i);
        stmt = strcat(stmt, and_where);
     }

   // and then group clause
   stmt = strcat(stmt, group);

   free(join);
   free(and_where);

   // go here on error
error:
   
   return stmt;
}
/*}}}*/

// generate SQL for match_stmt
char *generate_match_sql(int min_length, char *same_line_sql) /*{{{*/ 
{
   char select[] = "SELECT text_name, line_number, GROUP_CONCAT(form, ' ') FROM (SELECT text_name, line_id, line_number, form FROM texts INNER JOIN lines USING (text_id) INNER JOIN words USING (line_id) INNER JOIN unique_words USING (unique_id) WHERE line_id IN (",
     order[] = ") ORDER BY word_number) GROUP BY line_id;";
   
   char *stmt, *same_line_copy;
   
   stmt = (char *) malloc(sizeof(char) * MAX_SQL_LEN);
   same_line_copy = (char *) malloc(sizeof(char) * MAX_SQL_LEN);
   if (NULL == stmt || NULL == same_line_copy) 
     {
        fprintf(stderr, "couldn't allocate memory for match stmt\n");
        goto error;
     }
   stmt[0] = '\0';
   
   same_line_copy = strdup(same_line_sql);
   same_line_copy[strlen(same_line_copy) - 1] = '\0';
   
   // put statement together
   stmt = strcat(stmt, select);
   stmt = strcat(stmt, same_line_copy);
   stmt = strcat(stmt, order);
   
error:
   return stmt;
}
/*}}}*/

// generate SQL for getting space delimited list of words
char *generate_match_report_sql(int min_length) /*{{{*/
{
   char select[] = "SELECT GROUP_CONCAT(form, '* *') FROM unique_words WHERE ",
     where[] = " unique_id = ? ";
   
   char *stmt;
   int i;
   
   stmt = (char *) malloc(sizeof(char) * MAX_SQL_LEN);
   stmt[0] = '\0';
   
   stmt = strcat(stmt, select);
   
   for (i = 0; i < min_length; ++ i) 
     {
        stmt = strcat(stmt, where);
        
        if (i == min_length - 1) 
          {
             stmt = strcat(stmt, " ;");
          }
        else 
          {
             stmt = strcat(stmt, " OR ");
          }
     }
   
   return stmt;
}
/*}}}*/

// find collocations in same line
int sameline(int min_length, int min_occurence) /*{{{*/ 
{
   char *same_line_sql, *match_sql, *match_report_sql;
   int i, rc = 1, j, w = 0, p;
   int *words, *perms;
   sqlite3_stmt *same_line_stmt, *match_stmt, *match_report_stmt;
   
   // allocate memory for word lists and lines
   words = (int *) malloc(sizeof(int) * MAX_WORDS);
   perms = (int *) malloc(sizeof(int) * min_length);
   
   if (NULL == words || NULL == perms) 
     {
        fprintf(stderr, "couldn't allocate memory for word lists\n");
        goto sameline_error;
     }

   // step over list of words with min_occurence
   // w will have the number of words in the list
   // and then finished with word list statement
   while (SQLITE_ROW == sqlite3_step(word_list_stmt)) 
     {
        words[w ++] = sqlite3_column_int(word_list_stmt, 0);
     }
   sqlite3_finalize(word_list_stmt);
   fprintf(stderr, "have %d words\n", w);
      
   // initalise permutations
   for (i = 0; i < min_length; ++ i) 
     {
        perms[i] = i;
     }
   
   // generate SQL
   same_line_sql = generate_sameline_sql(min_length);
   match_sql = generate_match_sql(min_length, same_line_sql);
   match_report_sql = generate_match_report_sql(min_length);
   
   if (NULL == same_line_sql || NULL == match_sql || NULL == match_report_sql) 
     {
        goto sameline_error;
     }
   
   // prepare statements
   if (SQLITE_OK != sqlite3_prepare_v2(db, same_line_sql, -1, &same_line_stmt, 
                                       NULL)) 
     {
        fprintf(stderr, "couldn't prepare statement %s\n", same_line_sql);
        goto sameline_error;
     }
   if (SQLITE_OK != sqlite3_prepare_v2(db, match_sql, -1, &match_stmt, 
                                       NULL)) 
     {
        fprintf(stderr, "couldn't prepare statement %s\n", match_sql);
        goto sameline_error;
     }
   if (SQLITE_OK != sqlite3_prepare_v2(db, match_report_sql, -1, &match_report_stmt, 
                                       NULL)) 
     {
        fprintf(stderr, "couldn't prepare statement %s\n", match_report_sql);
        goto sameline_error;
     }
   
   // loop over permutations
   i = 0;
   while (perms[0] < (w - min_length + 1)) 
     {
        // loop over numbers in this permutation
        for (j = 0; j < min_length; ++ j) 
          {
             // bind
             if (SQLITE_OK != sqlite3_bind_int(same_line_stmt, j + 1, words[perms[j]])) 
               {
                  fprintf(stderr, "couldn't bind perm %d (%d)\n", j + 1, words[perms[j]]);
                  fprintf(stderr, "error: %s\n", sqlite3_errmsg(db));
                  goto sameline_error;
               }
          }
        
        // step until reached min_occurence
        p = 0;
        while (SQLITE_ROW == sqlite3_step(same_line_stmt) && p < min_occurence) 
          {
             ++ p;
          }
        
        // reset statement
        if (SQLITE_OK != sqlite3_reset(same_line_stmt)) 
          {
             fprintf(stderr, "couldn't reset same_line_stmt\n");
             fprintf(stderr, "error: %s\n", sqlite3_errmsg(db));
             goto sameline_error;
          }
        
        // reached min_occurence threashold
        // so remember this permutation of words
        if (p >= min_occurence) 
          {
             // bind for this set of matched words
             for (j = 0; j < min_length; ++ j) 
               {
                  // bind for matches
                  if (SQLITE_OK != sqlite3_bind_int(match_stmt, j + 1, 
                                                    words[perms[j]])) 
                    {
                       fprintf(stderr, "error binding param %d (%d) of match stmt\n",
                               j + 1, words[perms[j]]);
                       goto sameline_error;
                    }
                  
                  // bind for match report
                  if (SQLITE_OK != sqlite3_bind_int(match_report_stmt, j + 1, 
                                                    words[perms[j]])) 
                    {
                       fprintf(stderr, "error binding param %d (%d) of match report stmt\n",
                               j + 1, words[perms[j]]);
                       goto sameline_error;
                    }
               }
             
             if (SQLITE_ROW == sqlite3_step(match_report_stmt)) 
               {
                  printf("For collocation *%s*\n", 
                         sqlite3_column_text(match_report_stmt, 0));
               }
             
             // loop over results
             p = 0;
             while (SQLITE_ROW == sqlite3_step(match_stmt)) 
               {
                  printf("%s\t%d\t%s\n",
                         sqlite3_column_text(match_stmt, 0),
                         sqlite3_column_int(match_stmt, 1),
                         sqlite3_column_text(match_stmt, 2));
                  ++ p;
               }
             
             printf("%d matches\n\n", p);
             
             // reset
             if (SQLITE_OK != sqlite3_reset(match_stmt) ||
                 SQLITE_OK != sqlite3_reset(match_report_stmt)) 
               {
                  fprintf(stderr, "couldn't reset match stmts\n");
                  fprintf(stderr, "error: %s\n", sqlite3_errmsg(db));
                  goto sameline_error;
               }
          }
        
        // indicate progress
        ++ i;
        if (0 == (i % 100)) 
          {
             fprintf(stderr, "%d\n", i);
          }
        
        // incremented final position hasn't yet run over
        // so nothing else to do
        if (++ perms[min_length - 1] < w) 
          {
             continue;
          }

        // loop over positions from left to right
        for (j = 0; j < min_length - 1; ++ j) 
          {
             // look ahead to number in next position
             // if it has run over increment current position
             // and set next one to that (it will be incremented next)
             if (perms[j + 1] > w - (min_length - j)) 
               {
                  ++ perms[j];
                  perms[j + 1] = perms[j];
               }
          }
        
        // increment final position
        ++ perms[min_length - 1];
     }
   
   // finish up
   sqlite3_finalize(same_line_stmt);
   sqlite3_finalize(match_stmt);
   
   free(same_line_sql);
   free(match_sql);
   free(words);
   free(perms);
   
   rc = 0;
   
sameline_error:
   return rc;
}
/*}}}*/

int main (int argc, char **argv) /*{{{*/
{
   int rc = 1, min_occurence = 3, adjacent = SAME_LINE, min_length = 2;

   // check arguments
   switch (argc) 
     {
        // length of collocation
      case 5:
        min_length = atoi(argv[4]);
        // fall through

        // min occurence
      case 4:
        min_occurence = atoi(argv[3]);
        // fall through
        
        // what kind of adjacanncy
      case 3:
        switch (argv[2][0]) 
          {
           case 's':
             adjacent = SAME_LINE;
             break;
           case 'w':
             adjacent = ADJACENT_W;
             break;
           case 'l':
             adjacent = ADJACENT_L;
             break;
          }
        // fall through
        
        // database file
      case 2:
        if (SQLITE_OK == sqlite3_open(argv[1], &db)) 
          {
             break;
          }
        else 
          {
             fprintf(stderr, "Error opening %s\n", argv[1]);
             goto error;
          }
        
        // usage message
      default:
        printf("Usage: %s db_file [adjacent [min_occurence [min_length]]]\n", argv[0]);
        printf("\tadjacent is 's' for same line, 'w' for adjacent words, 'l' for adjacent lines, default s\n\tmin_occurence is a number, default 3\n\tmin_length is minimum length of collocation, default 2\n");
        goto error;
     }
   
   // prepare statements
   if (SQLITE_OK != sqlite3_prepare_v2(db, WORD_LIST_STMT, -1, &word_list_stmt, NULL)) 
     {
        fprintf(stderr, "couldn't prepare statement %s\n", WORD_LIST_STMT);
        goto error;
     }
   
   // bind statements
   if (SQLITE_OK != sqlite3_bind_int(word_list_stmt, 1, min_occurence)) 
     {
        fprintf(stderr, "couldn't bind min_occurance %d\n", min_occurence);
        goto error;
     }
   
   switch (adjacent) 
     {
        // words must be adjacent in same line
      case ADJACENT_W:
        break;
        
        // words must be in same line
      case SAME_LINE:
        sameline(min_length, min_occurence);
        break;
        
        // words must be in adjacent lines
      case ADJACENT_L:
        break;
     }
   
   // finish up
   sqlite3_close(db);
   
   // success
   rc = 0;
   
   // go here on error
error:
   
   return rc;
}
/*}}}*/
