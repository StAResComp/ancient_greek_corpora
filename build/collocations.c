#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>

#define WORD_LIST_SQL "SELECT unique_id FROM words INNER JOIN unique_words USING (unique_id) INNER JOIN lines USING (line_id) INNER JOIN texts USING (text_id) WHERE corpus_id = ? GROUP BY unique_id HAVING COUNT(*) >= ?;"
#define INSERT_COLLOC_DEF_SQL "INSERT OR IGNORE INTO collocation_definitions (collocation_def_id, min_occurrences, min_length, corpus_id) VALUES (NULL, ?, ?, ?);"
#define SELECT_COLLOC_DEF_SQL "SELECT collocation_def_id FROM collocation_definitions WHERE min_occurrences = ? AND min_length = ? and corpus_id = ?;"
#define INSERT_COLLOC_SQL "INSERT INTO collocations (collocation_id, collocation_def_id) VALUES (NULL, ?);"

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

// generate SQL for inserting collocation words
char *generate_colloc_words_sql(int min_length) /*{{{*/
{
     char insert[] = "INSERT INTO collocation_words(collocation_id, unique_id) VALUES ",
          values[] = "(?, ?)";

     char *sql;
     int i;

     sql = (char *) malloc(sizeof(char) * MAX_SQL_LEN);
     if (NULL == sql)
     {
          fprintf(stderr, "couldn't allocate memory for colloc words stmt\n");
          goto error;
     }
     sql[0] = '\0';

     sql = strcat(sql, insert);

     for (i = 0; i < min_length; ++ i)
     {
          sql = strcat(sql, values);

          if (i < min_length - 1)
          {
               sql = strcat(sql, ", ");
          }
     }
     sql = strcat(sql, " ;");

error:
     return sql;
}
/*}}}*/

// generate SQL for inserting collocation lines
char *generate_colloc_lines_sql(int min_length, char *same_line_sql) /*{{{*/
{
     char insert[] = "INSERT INTO collocation_lines (collocation_id, line_id) SELECT ?, line_id FROM ";
     char *sql, *same_line_copy;

     sql = (char *) malloc(sizeof(char) * MAX_SQL_LEN);
     same_line_copy = (char *) malloc(sizeof(char) * MAX_SQL_LEN);
     if (NULL == sql || NULL == same_line_copy)
     {
          fprintf(stderr, "couldn't allocate memory for colloc lines stmt\n");
          goto error;
     }
     sql[0] = '\0';

     same_line_copy = strndup(same_line_sql, strlen(same_line_sql) - 1);

     sql = strcat(sql, insert);
     sql = strcat(sql, " ( ");
     sql = strcat(sql, same_line_copy);
     sql = strcat(sql, " );");

     free(same_line_copy);

error:
     return sql;
}
/*}}}*/

// find collocations in same line
int sameline(int min_length, int min_occurence, int colloc_def_id) /*{{{*/
{
     char *same_line_sql, *insert_colloc_word_sql, *insert_colloc_line_sql;
     int i, rc = 1, j, w = 0, p, colloc_id;
     int *words, *perms;
     sqlite3_stmt *same_line_stmt, *insert_colloc_stmt,
          *insert_colloc_word_stmt, *insert_colloc_line_stmt;

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
     insert_colloc_word_sql = generate_colloc_words_sql(min_length);
     insert_colloc_line_sql = generate_colloc_lines_sql(min_length, same_line_sql);

     if (NULL == same_line_sql || NULL == insert_colloc_word_sql ||
         NULL == insert_colloc_line_sql)
     {
          goto sameline_error;
     }

     fprintf(stderr, "%s\n%s\n%s\n", same_line_sql, insert_colloc_word_sql, insert_colloc_line_sql);

     // prepare statements
     if (SQLITE_OK != sqlite3_prepare_v2(db, same_line_sql, -1, &same_line_stmt,
                                         NULL) ||
         SQLITE_OK != sqlite3_prepare_v2(db, INSERT_COLLOC_SQL, -1, &insert_colloc_stmt,
                                         NULL) ||
         SQLITE_OK != sqlite3_prepare_v2(db, insert_colloc_word_sql, -1, &insert_colloc_word_stmt,
                                         NULL) ||
         SQLITE_OK != sqlite3_prepare_v2(db, insert_colloc_line_sql, -1, &insert_colloc_line_stmt,
                                         NULL))
     {
          fprintf(stderr, "error: %s\n", sqlite3_errmsg(db));
          goto sameline_error;
     }

     // bind collocation def ID
     if (SQLITE_OK != sqlite3_bind_int(insert_colloc_stmt, 1, colloc_def_id))
     {
          fprintf(stderr, "couldn't bind colloc def ID\n");
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
          // so work with this permutation of words
          if (p >= min_occurence)
          {
               // insert new collocation
               if (SQLITE_DONE != sqlite3_step(insert_colloc_stmt) ||
                   SQLITE_OK != sqlite3_reset(insert_colloc_stmt))
               {
                    fprintf(stderr, "couldn't step/reset insert colloc stmt\n");
                    goto sameline_error;
               }

               // get collocation ID
               colloc_id = sqlite3_last_insert_rowid(db);

               // bind colloc statements for this set of matched words
               if (SQLITE_OK != sqlite3_bind_int(insert_colloc_line_stmt, 1, colloc_id))
               {
                    fprintf(stderr, "couldn't bind colloc ID to colloc line stmt\n");
                    goto sameline_error;
               }

               for (j = 0; j < min_length; ++ j)
               {
                    if (SQLITE_OK != sqlite3_bind_int(insert_colloc_word_stmt, (j * 2) + 1,
                                                      colloc_id) ||
                        SQLITE_OK != sqlite3_bind_int(insert_colloc_word_stmt, (j * 2) + 2,
                                                      words[perms[j]]) ||
                        SQLITE_OK != sqlite3_bind_int(insert_colloc_line_stmt, j + 2,
                                                      words[perms[j]]))
                    {
                         fprintf(stderr, "error binding param %d, %d (%d, %d) of colloc stmt\n",
                                 (j * 2) + 1, (j* 2) + 2, colloc_id, words[perms[j]]);
                         goto sameline_error;
                    }
               }

               // step and reset
               if (SQLITE_DONE != sqlite3_step(insert_colloc_word_stmt) ||
                   SQLITE_OK != sqlite3_reset(insert_colloc_word_stmt))
               {
                    fprintf(stderr, "couldn't step/reset colloc wordstmts\n");
                    fprintf(stderr, "error: %s\n", sqlite3_errmsg(db));
                    goto sameline_error;
               }

               if (SQLITE_DONE != sqlite3_step(insert_colloc_line_stmt) ||
                   SQLITE_OK != sqlite3_reset(insert_colloc_line_stmt))
               {
                    fprintf(stderr, "couldn't step/reset colloc line stmts\n");
                    fprintf(stderr, "error: %s\n", sqlite3_errmsg(db));
                    goto sameline_error;
               }

               fprintf(stderr, "Rows inserted: %d\n", sqlite3_changes(db));
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
     sqlite3_finalize(insert_colloc_stmt);
     sqlite3_finalize(insert_colloc_word_stmt);
     sqlite3_finalize(insert_colloc_line_stmt);

     free(same_line_sql);
     free(insert_colloc_word_sql);
     free(insert_colloc_line_sql);
     free(words);
     free(perms);

     rc = 0;

sameline_error:
     return rc;
}
/*}}}*/

int main (int argc, char **argv) /*{{{*/
{
     int rc = 1, min_occurence = 3, adjacent = SAME_LINE, min_length = 2,
          colloc_def_id, corpus_id = 0;
     sqlite3_stmt *insert_colloc_def_stmt, *select_colloc_def_stmt;

     // check arguments
     switch (argc)
     {
          // length of collocation
     case 6:
          min_length = atoi(argv[5]);
          // fall through
          
          // min occurence
     case 5:
          min_occurence = atoi(argv[4]);
          // fall through
          
          // what kind of adjacanncy
     case 4:
          switch (argv[3][0])
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
          
          // corpus ID
     case 3:
          corpus_id = atoi(argv[2]);
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
          printf("Usage: %s db_file corpus_id [adjacent [min_occurence [min_length]]]\n", argv[0]);
          printf("\tadjacent is 's' for same line, 'w' for adjacent words, 'l' for adjacent lines, default s\n\tmin_occurence is a number, default 3\n\tmin_length is minimum length of collocation, default 2\n");
          goto error;
     }

     // prepare statements
     if (SQLITE_OK != sqlite3_prepare_v2(db, WORD_LIST_SQL, -1, &word_list_stmt, NULL))
     {
          fprintf(stderr, "couldn't prepare statement %s\n", WORD_LIST_SQL);
          goto error;
     }
     if (SQLITE_OK != sqlite3_prepare_v2(db, INSERT_COLLOC_DEF_SQL, -1, &insert_colloc_def_stmt, NULL))
     {
          fprintf(stderr, "couldn't prepare statement %s\n", INSERT_COLLOC_DEF_SQL);
          goto error;
     }
     if (SQLITE_OK != sqlite3_prepare_v2(db, SELECT_COLLOC_DEF_SQL, -1, &select_colloc_def_stmt, NULL))
     {
          fprintf(stderr, "couldn't prepare statement %s\n", SELECT_COLLOC_DEF_SQL);
          goto error;
     }

     // bind statements
     if (SQLITE_OK != sqlite3_bind_int(word_list_stmt, 1, corpus_id))
     {
          fprintf(stderr, "couldn't bind corpus_id %d\n", corpus_id);
          goto error;
     }
     if (SQLITE_OK != sqlite3_bind_int(word_list_stmt, 2, min_occurence))
     {
          fprintf(stderr, "couldn't bind min_occurance %d\n", min_occurence);
          goto error;
     }
     if (SQLITE_OK != sqlite3_bind_int(insert_colloc_def_stmt, 1, min_occurence))
     {
          fprintf(stderr, "couldn't bind min_occurance %d\n", min_occurence);
          goto error;
     }
     if (SQLITE_OK != sqlite3_bind_int(insert_colloc_def_stmt, 2, min_length))
     {
          fprintf(stderr, "couldn't bind min_length %d\n", min_length);
          goto error;
     }
     if (SQLITE_OK != sqlite3_bind_int(insert_colloc_def_stmt, 3, corpus_id))
     {
          fprintf(stderr, "couldn't bind corpus_id %d\n", corpus_id);
          goto error;
     }
     if (SQLITE_OK != sqlite3_bind_int(select_colloc_def_stmt, 1, min_occurence))
     {
          fprintf(stderr, "couldn't bind min_occurance %d\n", min_occurence);
          goto error;
     }
     if (SQLITE_OK != sqlite3_bind_int(select_colloc_def_stmt, 2, min_length))
     {
          fprintf(stderr, "couldn't bind min_length %d\n", min_length);
          goto error;
     }
     if (SQLITE_OK != sqlite3_bind_int(select_colloc_def_stmt, 3, corpus_id))
     {
          fprintf(stderr, "couldn't bind corpus_id %d\n", corpus_id);
          goto error;
     }

     // insert and select collocation definition
     if (SQLITE_DONE != sqlite3_step(insert_colloc_def_stmt))
     {
          fprintf(stderr, "couldn't insert collocation definition\n");
          goto error;
     }
     if (SQLITE_ROW != sqlite3_step(select_colloc_def_stmt))
     {
          fprintf(stderr, "couldn't select collocation definition\n");
          goto error;
     }

     // get collocation definition ID
     colloc_def_id = sqlite3_column_int(select_colloc_def_stmt, 0);
     printf("Have collocation definition_id %d\n", colloc_def_id);

     // finished with insert/select collocation
     sqlite3_finalize(insert_colloc_def_stmt);
     sqlite3_finalize(select_colloc_def_stmt);

     switch (adjacent)
     {
          // words must be adjacent in same line
     case ADJACENT_W:
          break;

          // words must be in same line
     case SAME_LINE:
          sameline(min_length, min_occurence, colloc_def_id);
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
