/*
 *  Copyright 2016 CUBRID Corporation
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

/*
 * Smoke test for the cascci that win\build.bat produces: connect, create a table,
 * read it back, delete the rows and drop the table again.
 *
 *   win\build.bat test          tests what is already in win\output
 *   win\build.bat build test    builds first, then tests that build
 *
 * It is built the way anything using the package is: include\cas_cci.h for the
 * declarations, lib\cascci.lib to link, bin\cascci.dll beside the executable at run
 * time. Nothing is resolved by hand, so the test fails to build at all if the
 * package cannot be consumed - which is exactly what happened while the static
 * library was being shipped in place of the import library.
 *
 * The server comes from cci_test.conf and from nowhere else. Pass a different file
 * as the first argument to use another one.
 *
 * Exit status is 0 only when every step passed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cas_cci.h"

#define TEST_TABLE      "cci_test_t"
#define MAX_VALUE_LEN   256
#define MAX_LINE_LEN    1024
#define EXPECTED_ROWS   3

typedef struct
{
  char host[MAX_VALUE_LEN];
  int port;
  char database[MAX_VALUE_LEN];
  char user[MAX_VALUE_LEN];
  char password[MAX_VALUE_LEN];
} TEST_CONFIG;

static TEST_CONFIG config;
static int step_no = 0;
static int failed_steps = 0;

/*
 * Trim whitespace at both ends in place. The config file is written for humans, so
 * "port = 33000" has to mean the same as "port=33000".
 */
static void
trim (char *s)
{
  char *start = s;
  size_t len;

  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
    {
      start++;
    }
  if (start != s)
    {
      memmove (s, start, strlen (start) + 1);
    }

  len = strlen (s);
  while (len > 0)
    {
      char c = s[len - 1];
      if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
	{
	  break;
	}
      s[--len] = '\0';
    }
}

static void
copy_value (char *dst, const char *src)
{
  strncpy (dst, src, MAX_VALUE_LEN - 1);
  dst[MAX_VALUE_LEN - 1] = '\0';
}

/*
 * Reads key = value lines. Returns 0 on success. A key missing from the file keeps
 * its empty default and is rejected below, so a truncated config fails with a named
 * key rather than with a connection error. One config file serves every case under
 * testcases\, so a key this test does not know belongs to another one and is skipped.
 */
static int
load_config (const char *path)
{
  FILE *fp;
  char line[MAX_LINE_LEN];
  int port_seen = 0;

  fp = fopen (path, "r");
  if (fp == NULL)
    {
      fprintf (stderr, "ERROR: cannot open the config file [%s].\n", path);
      return -1;
    }

  while (fgets (line, sizeof (line), fp) != NULL)
    {
      char *comment;
      char *sep;
      char *key;
      char *value;

      comment = strchr (line, '#');
      if (comment != NULL)
	{
	  *comment = '\0';
	}

      sep = strchr (line, '=');
      if (sep == NULL)
	{
	  trim (line);
	  if (line[0] != '\0')
	    {
	      fprintf (stderr, "ERROR: [%s] is not a key = value line.\n", line);
	      fclose (fp);
	      return -1;
	    }
	  continue;
	}

      *sep = '\0';
      key = line;
      value = sep + 1;
      trim (key);
      trim (value);

      if (strcmp (key, "host") == 0)
	{
	  copy_value (config.host, value);
	}
      else if (strcmp (key, "port") == 0)
	{
	  config.port = atoi (value);
	  port_seen = 1;
	}
      else if (strcmp (key, "database") == 0)
	{
	  copy_value (config.database, value);
	}
      else if (strcmp (key, "user") == 0)
	{
	  copy_value (config.user, value);
	}
      else if (strcmp (key, "password") == 0)
	{
	  /* An empty password is legitimate, so this key is never checked below. */
	  copy_value (config.password, value);
	}
    }

  fclose (fp);

  if (config.host[0] == '\0')
    {
      fprintf (stderr, "ERROR: host is missing from %s.\n", path);
      return -1;
    }
  if (!port_seen || config.port <= 0)
    {
      fprintf (stderr, "ERROR: port is missing or not a positive number in %s.\n", path);
      return -1;
    }
  if (config.database[0] == '\0')
    {
      fprintf (stderr, "ERROR: database is missing from %s.\n", path);
      return -1;
    }
  if (config.user[0] == '\0')
    {
      fprintf (stderr, "ERROR: user is missing from %s.\n", path);
      return -1;
    }

  return 0;
}

static void
step_begin (const char *what)
{
  step_no++;
  printf ("  %d) %-38s", step_no, what);
  fflush (stdout);
}

static void
step_ok (const char *detail)
{
  if (detail != NULL && detail[0] != '\0')
    {
      printf ("OK   (%s)\n", detail);
    }
  else
    {
      printf ("OK\n");
    }
  fflush (stdout);
}

/*
 * CCI reports the driver side code as the return value and the server side detail in
 * err_buf, and either one can be the interesting half, so both are printed.
 */
static void
step_fail (int code, T_CCI_ERROR * err)
{
  failed_steps++;
  printf ("FAIL\n");
  printf ("       cci error : %d\n", code);
  if (err != NULL && err->err_msg[0] != '\0')
    {
      printf ("       server    : %d, %s\n", err->err_code, err->err_msg);
    }
  fflush (stdout);
}

/*
 * Runs one statement that returns no rows. Gives back the affected row count, or a
 * negative CCI error code.
 */
static int
exec_sql (int con, const char *sql, T_CCI_ERROR * err)
{
  int req;
  int affected;

  req = cci_prepare (con, sql, 0, err);
  if (req < 0)
    {
      return req;
    }

  affected = cci_execute (req, 0, 0, err);
  cci_close_req_handle (req);

  return affected;
}

/*
 * Reads the whole table back and returns the number of rows, or a negative CCI error
 * code. Rows are printed so a failing comparison can be eyeballed.
 */
static int
select_rows (int con, T_CCI_ERROR * err, int print_rows)
{
  int req;
  int res;
  int rows = 0;

  req = cci_prepare (con, "SELECT id, name FROM " TEST_TABLE " ORDER BY id", 0, err);
  if (req < 0)
    {
      return req;
    }

  res = cci_execute (req, 0, 0, err);
  if (res < 0)
    {
      cci_close_req_handle (req);
      return res;
    }

  res = cci_cursor (req, 1, CCI_CURSOR_FIRST, err);
  while (res != CCI_ER_NO_MORE_DATA)
    {
      int id = 0;
      char *name = NULL;
      int indicator = 0;

      if (res < 0)
	{
	  cci_close_req_handle (req);
	  return res;
	}

      res = cci_fetch (req, err);
      if (res < 0)
	{
	  cci_close_req_handle (req);
	  return res;
	}

      res = cci_get_data (req, 1, CCI_A_TYPE_INT, &id, &indicator);
      if (res < 0)
	{
	  cci_close_req_handle (req);
	  return res;
	}
      res = cci_get_data (req, 2, CCI_A_TYPE_STR, &name, &indicator);
      if (res < 0)
	{
	  cci_close_req_handle (req);
	  return res;
	}

      rows++;
      if (print_rows)
	{
	  printf ("\n       row %d : id=%d, name=%s", rows, id, name == NULL ? "(null)" : name);
	}

      res = cci_cursor (req, 1, CCI_CURSOR_CURRENT, err);
    }

  if (print_rows && rows > 0)
    {
      printf ("\n       %-38s", "");
    }

  cci_close_req_handle (req);
  return rows;
}

int
main (int argc, char *argv[])
{
  const char *config_path = "cci_test.conf";
  char version[64];
  char detail[128];
  int major = 0, minor = 0, patch = 0;
  T_CCI_ERROR err;
  int con;
  int res;

  if (argc > 1)
    {
      config_path = argv[1];
    }

  memset (&config, 0, sizeof (config));
  memset (&err, 0, sizeof (err));

  if (load_config (config_path) < 0)
    {
      return 1;
    }

  /* The first call into the DLL, so it also proves the import library resolves. */
  strcpy (version, "unknown");
  if (cci_get_version (&major, &minor, &patch) >= 0)
    {
      sprintf (version, "%d.%d.%d", major, minor, patch);
    }

  printf ("==========================================================\n");
  printf (" cci version  : %s\n", version);
  printf (" config       : %s\n", config_path);
  printf (" server       : %s:%d\n", config.host, config.port);
  printf (" database     : %s\n", config.database);
  printf (" user         : %s%s\n", config.user, config.password[0] == '\0' ? " (no password)" : "");
  printf ("==========================================================\n");

  step_begin ("connect");
  con = cci_connect_ex (config.host, config.port, config.database, config.user, config.password, &err);
  if (con < 0)
    {
      step_fail (con, &err);
      printf ("\nCannot continue without a connection.\n");
      return 1;
    }
  step_ok (NULL);

  /* Autocommit keeps the test to one statement per step; nothing here needs a transaction. */
  step_begin ("set autocommit");
  res = cci_set_autocommit (con, CCI_AUTOCOMMIT_TRUE);
  if (res < 0)
    {
      step_fail (res, NULL);
    }
  else
    {
      step_ok (NULL);
    }

  /* A leftover table from an interrupted run would break CREATE, so clear it first. */
  step_begin ("drop " TEST_TABLE " if it exists");
  res = exec_sql (con, "DROP TABLE IF EXISTS " TEST_TABLE, &err);
  if (res < 0)
    {
      step_fail (res, &err);
    }
  else
    {
      step_ok (NULL);
    }

  step_begin ("create table");
  res = exec_sql (con, "CREATE TABLE " TEST_TABLE " (id INT PRIMARY KEY, name VARCHAR(64))", &err);
  if (res < 0)
    {
      step_fail (res, &err);
    }
  else
    {
      step_ok (NULL);
    }

  step_begin ("insert 3 rows");
  res = exec_sql (con,
		  "INSERT INTO " TEST_TABLE " (id, name) VALUES " "(1, 'first'), (2, 'second'), (3, 'third')", &err);
  if (res < 0)
    {
      step_fail (res, &err);
    }
  else
    {
      sprintf (detail, "%d rows", res);
      step_ok (detail);
    }

  step_begin ("select");
  res = select_rows (con, &err, 1);
  if (res < 0)
    {
      step_fail (res, &err);
    }
  else if (res != EXPECTED_ROWS)
    {
      failed_steps++;
      printf ("FAIL (expected %d rows, read %d)\n", EXPECTED_ROWS, res);
    }
  else
    {
      sprintf (detail, "%d rows", res);
      step_ok (detail);
    }

  step_begin ("delete");
  res = exec_sql (con, "DELETE FROM " TEST_TABLE, &err);
  if (res < 0)
    {
      step_fail (res, &err);
    }
  else if (res != EXPECTED_ROWS)
    {
      failed_steps++;
      printf ("FAIL (expected %d rows deleted, got %d)\n", EXPECTED_ROWS, res);
    }
  else
    {
      sprintf (detail, "%d rows", res);
      step_ok (detail);
    }

  /* The delete is only proven once the table reads back empty. */
  step_begin ("select after delete");
  res = select_rows (con, &err, 0);
  if (res < 0)
    {
      step_fail (res, &err);
    }
  else if (res != 0)
    {
      failed_steps++;
      printf ("FAIL (expected 0 rows, read %d)\n", res);
    }
  else
    {
      step_ok ("0 rows");
    }

  step_begin ("drop table");
  res = exec_sql (con, "DROP TABLE " TEST_TABLE, &err);
  if (res < 0)
    {
      step_fail (res, &err);
    }
  else
    {
      step_ok (NULL);
    }

  step_begin ("disconnect");
  res = cci_disconnect (con, &err);
  if (res < 0)
    {
      step_fail (res, &err);
    }
  else
    {
      step_ok (NULL);
    }

  printf ("\n");
  if (failed_steps == 0)
    {
      printf ("All %d steps passed.\n", step_no);
      return 0;
    }

  printf ("%d of %d steps failed.\n", failed_steps, step_no);
  return 1;
}
