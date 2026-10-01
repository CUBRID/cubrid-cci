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
 * Checks that the useSSL connection property really brings TLS up against a broker
 * running with SSL turned on.
 *
 * A successful SSL connect proves very little on its own: a driver that quietly
 * ignored the property would look exactly the same from here. So the behaviour is
 * pinned from both directions.
 *
 *   1. useSSL=true against the SSL broker connects and carries a real query.
 *   2. useSSL=false against that same broker is refused - which is what shows the
 *      broker is in SSL mode, and therefore that step 1 was not plaintext.
 *   3. useSSL=true against the plain broker from "port" is refused as well - which is
 *      what shows the driver really sends the SSL handshake instead of pretending.
 *
 * A broker takes either plain or SSL clients, never both (broker.c rejects the wrong
 * kind with CAS_ER_SSL_TYPE_NOT_ALLOWED), so checking SSL needs a second broker with
 * SSL=ON in cubrid_broker.conf. Point ssl_port at it in cci_test.conf:
 *
 *     ssl_port = 33001
 *     ssl_host = 192.168.3.31     # optional, defaults to host
 *
 * With no ssl_port there is nothing to test against, so the case reports SKIPPED and
 * exits 0 rather than failing a checkout that has no SSL broker to hand.
 *
 * Exit status is 0 when every step passed, or when the case was skipped.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cas_cci.h"

#define MAX_VALUE_LEN   256
#define MAX_LINE_LEN    1024
#define MAX_URL_LEN     1024

typedef struct
{
  char host[MAX_VALUE_LEN];
  int port;
  char database[MAX_VALUE_LEN];
  char user[MAX_VALUE_LEN];
  char password[MAX_VALUE_LEN];
  char ssl_host[MAX_VALUE_LEN];
  int ssl_port;
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
 * Reads key = value lines. Returns 0 on success. One config file serves every case
 * under testcases\, so a key this test does not know belongs to another one and is
 * skipped; only the keys used here are checked.
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
      else if (strcmp (key, "ssl_host") == 0)
	{
	  copy_value (config.ssl_host, value);
	}
      else if (strcmp (key, "ssl_port") == 0)
	{
	  config.ssl_port = atoi (value);
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

  /* The SSL broker is usually the plain one's neighbour, so the host is shared. */
  if (config.ssl_host[0] == '\0')
    {
      copy_value (config.ssl_host, config.host);
    }

  return 0;
}

static void
step_begin (const char *what)
{
  step_no++;
  printf ("  %d) %-46s", step_no, what);
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
step_fail (int code, T_CCI_ERROR *err)
{
  char msg[1024];

  failed_steps++;
  printf ("FAIL\n");

  msg[0] = '\0';
  if (cci_get_err_msg (code, msg, sizeof (msg)) == 0 && msg[0] != '\0')
    {
      printf ("       cci error : %d, %s\n", code, msg);
    }
  else
    {
      printf ("       cci error : %d\n", code);
    }

  if (err != NULL && err->err_msg[0] != '\0')
    {
      printf ("       server    : %d, %s\n", err->err_code, err->err_msg);
    }
  fflush (stdout);
}

/*
 * The connection URL CCI parses is
 * cci:cubrid:<host>:<port>:<db>:<user>:<password>:[?prop=value&...], and user and
 * password are left out here because cci_connect_with_url_ex takes them as arguments.
 */
static void
build_url (char *buf, size_t size, const char *host, int port, const char *db, int use_ssl)
{
  snprintf (buf, size, "cci:cubrid:%s:%d:%s:::?useSSL=%s", host, port, db, use_ssl ? "true" : "false");
}

static int
connect_with_ssl (const char *host, int port, int use_ssl, T_CCI_ERROR *err)
{
  char url[MAX_URL_LEN];

  build_url (url, sizeof (url), host, port, config.database, use_ssl);
  memset (err, 0, sizeof (*err));

  return cci_connect_with_url_ex (url, config.user, config.password, err);
}

/*
 * One round trip over the connection. A handshake that completes but cannot carry a
 * statement is not a working SSL connection, so this is what step 1 actually proves.
 */
static int
run_query (int con, T_CCI_ERROR *err)
{
  int req;
  int res;
  int value = 0;
  int indicator = 0;

  req = cci_prepare (con, "SELECT 1 FROM db_root", 0, err);
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

  res = cci_get_data (req, 1, CCI_A_TYPE_INT, &value, &indicator);
  cci_close_req_handle (req);
  if (res < 0)
    {
      return res;
    }

  return value;
}

/*
 * The two negative controls. Connecting is supposed to fail here, so a connection that
 * comes up is the failure, and it is closed again before saying so.
 */
static void
expect_refused (const char *what, const char *host, int port, int use_ssl)
{
  T_CCI_ERROR err;
  char detail[256];
  int con;

  step_begin (what);

  con = connect_with_ssl (host, port, use_ssl, &err);
  if (con >= 0)
    {
      cci_disconnect (con, &err);
      failed_steps++;
      printf ("FAIL (the connection was accepted, so SSL is not being enforced)\n");
      fflush (stdout);
      return;
    }

  sprintf (detail, "refused, cci %d", con);
  step_ok (detail);
  if (err.err_msg[0] != '\0')
    {
      printf ("       server    : %d, %s\n", err.err_code, err.err_msg);
      fflush (stdout);
    }
}

int
main (int argc, char *argv[])
{
  const char *config_path = "cci_test.conf";
  char version[64];
  char detail[256];
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

  strcpy (version, "unknown");
  if (cci_get_version (&major, &minor, &patch) >= 0)
    {
      sprintf (version, "%d.%d.%d", major, minor, patch);
    }

  printf ("==========================================================\n");
  printf (" cci version  : %s\n", version);
  printf (" config       : %s\n", config_path);
  printf (" plain broker : %s:%d\n", config.host, config.port);
  if (config.ssl_port > 0)
    {
      printf (" ssl broker   : %s:%d\n", config.ssl_host, config.ssl_port);
    }
  else
    {
      printf (" ssl broker   : (not configured)\n");
    }
  printf (" database     : %s\n", config.database);
  printf ("==========================================================\n");

  if (config.ssl_port <= 0)
    {
      printf ("\n");
      printf ("SKIPPED: no ssl_port in %s, so there is no SSL broker to test against.\n", config_path);
      printf ("         Set SSL=ON on a broker in cubrid_broker.conf and add its port:\n");
      printf ("\n");
      printf ("             ssl_port = 33001\n");
      printf ("             ssl_host = %s   # optional, defaults to host\n", config.host);
      printf ("\n");
      return 0;
    }

  /* useSSL=true has to reach a server, so this is the step that can end the run. */
  step_begin ("connect with useSSL=true");
  con = connect_with_ssl (config.ssl_host, config.ssl_port, 1, &err);
  if (con < 0)
    {
      step_fail (con, &err);
      printf ("\n");
      printf ("Cannot continue: %s:%d did not accept an SSL connection. Check that the\n", config.ssl_host,
	      config.ssl_port);
      printf ("broker really has SSL=ON and that it was restarted afterwards.\n");
      return 1;
    }
  step_ok (NULL);

  step_begin ("query over the SSL connection");
  res = run_query (con, &err);
  if (res < 0)
    {
      step_fail (res, &err);
    }
  else if (res != 1)
    {
      failed_steps++;
      printf ("FAIL (expected 1, read %d)\n", res);
    }
  else
    {
      step_ok ("SELECT 1 returned 1");
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

  /*
   * Without this the test would pass just as happily against a driver that never
   * enabled SSL at all: it is the refusal that shows the broker is in SSL mode.
   */
  sprintf (detail, "useSSL=false against %s:%d is refused", config.ssl_host, config.ssl_port);
  expect_refused (detail, config.ssl_host, config.ssl_port, 0);

  /*
   * And the mirror image, whenever a plain broker is around to try it on: the driver
   * has to be sending something the plain broker recognises as an SSL client.
   */
  if (config.ssl_port != config.port || strcmp (config.ssl_host, config.host) != 0)
    {
      sprintf (detail, "useSSL=true against %s:%d is refused", config.host, config.port);
      expect_refused (detail, config.host, config.port, 1);
    }
  else
    {
      printf ("  -) %-46s%s\n", "useSSL=true against the plain broker",
	      "SKIP (ssl_port is the plain port)");
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
