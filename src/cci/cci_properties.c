/*
 * Copyright (C) 2008 Search Solution Corporation. 
 * Copyright (c) 2016 CUBRID Corporation.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the <ORGANIZATION> nor the names of its contributors
 *   may be used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */


/*
 * cci_properties.c -
 */

#ident "$Id$"

/*
 * IMPORTED SYSTEM HEADER FILES
 */
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#if defined(WINDOWS)
#include <direct.h>
#else
#include <sys/time.h>
#endif

/*
 * OTHER IMPORTED HEADER FILES
 */
#include "porting.h"
#include "cci_handle_mng.h"
#include "cci_log.h"

/*
 * PRIVATE DEFINITIONS
 */
typedef enum
{
  BOOL_PROPERTY,
  INT_PROPERTY,
  STRING_PROPERTY
} T_TYPE_PROPERTY;

typedef struct
{
  const char *name;
  T_TYPE_PROPERTY type;
  void *data;
} T_URL_PROPERTY;

/*
 * PRIVATE FUNCTION PROTOTYPES
 */
static int cci_url_set_althosts (T_CON_HANDLE * handle, char *data);
static int cci_url_parse_properties (T_URL_PROPERTY props[], int len, char *properties);
static int cci_url_set_properties (T_URL_PROPERTY props[], int len, char *name, char *value);
static int cci_url_set_value (T_URL_PROPERTY * property, char *value);
static int cci_url_get_int (char *str, int *value);
static int cci_url_get_bool (char *str, bool * value);
static int cci_url_resolve_sslmode (const char *sslmode, bool use_ssl);
static void cci_url_set_invalid (T_CON_HANDLE * handle, const char *format, ...);
static void cci_shuffle_althosts (T_CON_HANDLE * handle);

/*
 * INTERFACE VARIABLES
 */

/*
 * PUBLIC VARIABLES
 */

/*
 * PRIVATE VARIABLES
 */

/*
 * IMPLEMENTATION OF INTERFACE FUNCTIONS
 */
static int
cci_url_get_bool (char *str, bool * value)
{
  static const char *accepts[] = {
    "true", "false", "on", "off", "yes", "no"
  };
  int i, dim;

  if (value == NULL)
    {
      return CCI_ER_INVALID_URL;
    }

  dim = DIM (accepts);
  for (i = 0; i < dim; i++)
    {
      if (strcasecmp (accepts[i], str) == 0)
	{
	  *value = (i % 2) == 0;
	  return CCI_ER_NO_ERROR;
	}
    }

  return CCI_ER_INVALID_URL;
}

static int
cci_url_get_int (char *str, int *value)
{
  int v;
  char *end;

  if (value == NULL)
    {
      return CCI_ER_INVALID_URL;
    }

  v = strtol (str, &end, 10);
  if (end != NULL && end[0] != '\0')
    {
      return CCI_ER_INVALID_URL;
    }

  *value = v;
  return CCI_ER_NO_ERROR;
}

/*
 * cci_url_resolve_sslmode - the mode a URL asks for: sslmode when given, otherwise derived from useSSL
 *   return: T_CCI_SSLMODE, or -1 when sslmode names no mode
 */
static int
cci_url_resolve_sslmode (const char *sslmode, bool use_ssl)
{
  if (sslmode == NULL)
    {
      return use_ssl ? CCI_SSLMODE_REQUIRED : CCI_SSLMODE_DISABLED;
    }

  return cci_sslmode_from_label (sslmode);
}

/*
 * cci_url_set_invalid - why the URL was refused, left in the handle for cci_connect_with_url_internal() to pass on
 */
static void
cci_url_set_invalid (T_CON_HANDLE * handle, const char *format, ...)
{
  va_list args;

  handle->err_buf.err_code = CCI_ER_INVALID_URL;
  va_start (args, format);
  vsnprintf (handle->err_buf.err_msg, sizeof (handle->err_buf.err_msg), format, args);
  va_end (args);
}

static int
cci_url_set_value (T_URL_PROPERTY * property, char *value)
{
  int error = CCI_ER_NO_ERROR;

  switch (property->type)
    {
    case BOOL_PROPERTY:
      {
	bool v;
	error = cci_url_get_bool (value, &v);
	if (error == CCI_ER_NO_ERROR)
	  {
	    *((char *) property->data) = v;
	  }
	break;
      }
    case INT_PROPERTY:
      {
	int v;
	error = cci_url_get_int (value, &v);
	if (error == CCI_ER_NO_ERROR)
	  {
	    *((int *) property->data) = v;
	  }
	break;
      }
    case STRING_PROPERTY:
      {
	*((char **) property->data) = strdup (value);
	if (*((char **) property->data) == NULL)
	  {
	    return CCI_ER_NO_MORE_MEMORY;
	  }
	break;
      }
    default:
      return CCI_ER_INVALID_URL;
    }

  return error;
}

static int
cci_url_set_properties (T_URL_PROPERTY props[], int len, char *name, char *value)
{
  int i, error = CCI_ER_NO_ERROR;

  if (name == NULL || value == NULL)
    {
      return CCI_ER_INVALID_URL;
    }

  for (i = 0; i < len && error == CCI_ER_NO_ERROR; i++)
    {
      if (strcasecmp (name, props[i].name) == 0)
	{
	  error = cci_url_set_value (&props[i], value);
	  return error;
	}
    }

  if (i == len)
    {
      return CCI_ER_INVALID_URL;
    }

  return error;
}

static int
cci_url_parse_properties (T_URL_PROPERTY props[], int len, char *properties)
{
  char *token, *save_url = NULL;
  int error = CCI_ER_NO_ERROR;

  if (props == NULL)
    {
      return CCI_ER_INVALID_URL;
    }

  token = strtok_r (properties, "?&", &save_url);
  while (token != NULL && error == CCI_ER_NO_ERROR)
    {
      char *name, *value, *save_property = NULL;
      name = strtok_r (token, "=", &save_property);
      value = strtok_r (NULL, "=", &save_property);
      error = cci_url_set_properties (props, len, name, value);

      token = strtok_r (NULL, "&", &save_url);
    }

  return error;
}

static int
cci_url_set_althosts (T_CON_HANDLE * handle, char *data)
{
  T_ALTER_HOST *hosts = handle->alter_hosts;
  char *token, *save_data = NULL, *end;
  int i, error = CCI_ER_NO_ERROR;

  memcpy (handle->alter_hosts[0].ip_addr, handle->ip_addr, 4);
  handle->alter_hosts[0].port = handle->port;
  /* freed first: a pooled handle parses its properties again on reuse */
  FREE_MEM (hosts[0].hostname);
  ALLOC_COPY (hosts[0].hostname, handle->hostname);
  if (hosts[0].hostname == NULL)
    {
      return CCI_ER_NO_MORE_MEMORY;
    }

  for (i = 1;; i++, data = NULL)
    {
      char *host, *port, *save_alter = NULL;
      int v;

      if (i >= ALTER_HOST_MAX_SIZE)
	{
	  return CCI_ER_INVALID_URL;
	}

      token = strtok_r (data, ",", &save_data);
      if (token == NULL)
	{
	  break;
	}

      host = strtok_r (token, ":", &save_alter);
      if (host == NULL)
	{
	  return CCI_ER_INVALID_URL;
	}
      error = hm_ip_str_to_addr (host, hosts[i].ip_addr);
      if (error < 0)
	{
	  return error;
	}
      FREE_MEM (hosts[i].hostname);
      ALLOC_COPY (hosts[i].hostname, host);
      if (hosts[i].hostname == NULL)
	{
	  return CCI_ER_NO_MORE_MEMORY;
	}

      port = strtok_r (NULL, ":", &save_alter);
      if (port == NULL)
	{
	  return CCI_ER_INVALID_URL;
	}
      v = strtol (port, &end, 10);
      if (v <= 0 || (end != NULL && end[0] != '\0'))
	{
	  return CCI_ER_INVALID_URL;
	}
      hosts[i].port = v;
    }
  handle->alter_host_count = i;
  if (handle->load_balance)
    {
      cci_shuffle_althosts (handle);
    }

  return error;
}

static void
cci_shuffle_althosts (T_CON_HANDLE * handle)
{
  struct timeval t;
  int i, j;
  double r;
  struct drand48_data buf;
  T_ALTER_HOST temp_host;

  gettimeofday (&t, NULL);

  /* tv_usec returned by gettimeofday on WINDOWS is millisec * 1000 and seeding it would result in generating an even
   * random number at first. To avoid such a pattern in generating a random number, tv_usec/1000 is used on WINDOWS. */
#if defined (WINDOWS)
  srand48_r (t.tv_usec / 1000, &buf);
#else /* WINDOWS */
  srand48_r (t.tv_usec, &buf);
#endif /* !WINDOWS */

  /* Fisher-Yates shuffle */
  for (i = handle->alter_host_count - 1; i > 0; i--)
    {
      drand48_r (&buf, &r);
      j = (int) ((i + 1) * r);

      temp_host = handle->alter_hosts[j];
      handle->alter_hosts[j] = handle->alter_hosts[i];
      handle->alter_hosts[i] = temp_host;
    }
}

int
cci_conn_set_properties (T_CON_HANDLE * handle, char *properties)
{
  char *base = NULL, *file = NULL, *althosts = NULL;
  char *sslmode = NULL, *sslca = NULL;
  char path[PATH_MAX];
  int mode;

  T_URL_PROPERTY props[] = {
    {"altHosts", STRING_PROPERTY, &althosts},
    {"loadBalance", BOOL_PROPERTY, &handle->load_balance},
    {"rcTime", INT_PROPERTY, &handle->rc_time},
    {"loginTimeout", INT_PROPERTY, &handle->login_timeout},
    {"queryTimeout", INT_PROPERTY, &handle->query_timeout},
    {"disconnectOnQueryTimeout", BOOL_PROPERTY,
     &handle->disconnect_on_query_timeout},
    {"logFile", STRING_PROPERTY, &file},
    {"logOnException", BOOL_PROPERTY, &handle->log_on_exception},
    {"logSlowQueries", BOOL_PROPERTY, &handle->log_slow_queries},
    {"slowQueryThresholdMillis", INT_PROPERTY,
     &handle->slow_query_threshold_millis},
    {"logTraceApi", BOOL_PROPERTY, &handle->log_trace_api},
    {"logTraceNetwork", BOOL_PROPERTY, &handle->log_trace_network},
    {"logBaseDir", STRING_PROPERTY, &base},
    /* for backward compatibility */
    {"login_timeout", INT_PROPERTY, &handle->login_timeout},
    {"query_timeout", INT_PROPERTY, &handle->query_timeout},
    {"disconnect_on_query_timeout", BOOL_PROPERTY,
     &handle->disconnect_on_query_timeout},
    {"useSSL", BOOL_PROPERTY, &handle->useSSL},
    {"sslmode", STRING_PROPERTY, &sslmode},
    {"sslca", STRING_PROPERTY, &sslca},
    {"__gateway", BOOL_PROPERTY, &handle->__gateway},
  };
  int error = CCI_ER_NO_ERROR;

  error = cci_url_parse_properties (props, DIM (props), properties);
  if (error != CCI_ER_NO_ERROR)
    {
      goto set_properties_end;
    }

  if (handle->rc_time < 0 || handle->slow_query_threshold_millis < 0 || handle->login_timeout < 0
      || handle->query_timeout < 0)
    {
      error = CCI_ER_INVALID_URL;
      goto set_properties_end;
    }

  if (handle->rc_time < MONITORING_INTERVAL)
    {
      handle->rc_time = MONITORING_INTERVAL;
    }

  /* sslmode decides when given, and useSSL is filled to match (1 is USESSL) so that the == USESSL checks in
   * cci_network.c keep working unchanged */
  mode = cci_url_resolve_sslmode (sslmode, handle->useSSL);
  if (mode < 0)
    {
      cci_url_set_invalid (handle, "Invalid sslmode value '%s'", sslmode);
      error = CCI_ER_INVALID_URL;
      goto set_properties_end;
    }
  handle->sslmode = (T_CCI_SSLMODE) mode;
  handle->useSSL = (handle->sslmode != CCI_SSLMODE_DISABLED);

  FREE_MEM (handle->ssl_ca_file);
  handle->ssl_ca_file = sslca;
  sslca = NULL;

  /* No fallback to a system CA store: the bundled OpenSSL cannot find one, and any public CA could then vouch for
   * the broker. JDBC has the same rule. */
  if (handle->sslmode >= CCI_SSLMODE_VERIFY_CA && handle->ssl_ca_file == NULL)
    {
      cci_url_set_invalid (handle, "sslca is required for sslmode=%s", cci_sslmode_label (handle->sslmode));
      error = CCI_ER_INVALID_URL;
      goto set_properties_end;
    }

  if (althosts != NULL)
    {
      error = cci_url_set_althosts (handle, althosts);
      if (error != CCI_ER_NO_ERROR)
	{
	  goto set_properties_end;
	}
    }

  if (althosts == NULL && handle->load_balance)
    {
      handle->load_balance = false;
    }

  /* for logging */
  if (handle->log_on_exception || handle->log_slow_queries || handle->log_trace_api || handle->log_trace_network)
    {
      if (file == NULL)
	{
	  if (base == NULL)
	    {
	      snprintf (path, PATH_MAX, "cci_%04d.log", handle->id);
	    }
	  else
	    {
	      snprintf (path, PATH_MAX, "%s/cci_%04d.log", base, handle->id);
	    }
	}
      else
	{
	  if (base == NULL)
	    {
	      snprintf (path, PATH_MAX, "%s", file);
	    }
	  else
	    {
	      snprintf (path, PATH_MAX, "%s/%s", base, file);
	    }
	}

      handle->log_filename = strdup (path);
      if (handle->log_filename == NULL)
	{
	  error = CCI_ER_NO_MORE_MEMORY;
	  goto set_properties_end;
	}

      handle->logger = cci_log_get (handle->log_filename);
      if (handle->logger == NULL)
	{
	  error = CCI_ER_INVALID_URL;
	  goto set_properties_end;
	}
      cci_log_set_level (handle->logger, CCI_LOG_LEVEL_DEBUG);
    }

set_properties_end:
  FREE_MEM (althosts);
  FREE_MEM (base);
  FREE_MEM (file);
  FREE_MEM (sslmode);
  FREE_MEM (sslca);

  API_SLOG (handle);
  API_ELOG (handle, error);
  return error;
}

/*
 * cci_url_peek_sslmode - the TLS mode and sslca the URL properties ask for; read before the handle exists, to pick
 *			  a pooled one that was opened the same way
 *   return: T_CCI_SSLMODE, or -1 when sslmode names no mode
 *   prop(in): URL properties
 *   sslca(out): the sslca value, or "" when not given
 *   sslca_size(in): size of sslca
 *
 * Unlike cci_conn_set_properties(), names other than useSSL, sslmode and sslca are skipped rather than refused:
 * refusing them stopped the scan, so "?loginTimeout=1000&useSSL=true" read as plaintext and could be given a pooled
 * plaintext connection. Invalid values are left for cci_conn_set_properties() to refuse.
 */
int
cci_url_peek_sslmode (char *prop, char *sslca, int sslca_size)
{
  char buf[4096] = { 0, };
  char *token, *save_url = NULL;
  char *sslmode = NULL;
  bool use_ssl = false;

  sslca[0] = '\0';
  if (prop == NULL)
    {
      return CCI_SSLMODE_DISABLED;
    }

  snprintf (buf, sizeof (buf), "%s", prop);

  for (token = strtok_r (buf, "?&", &save_url); token != NULL; token = strtok_r (NULL, "&", &save_url))
    {
      char *name, *value, *save_property = NULL;

      name = strtok_r (token, "=", &save_property);
      value = strtok_r (NULL, "=", &save_property);
      if (name == NULL || value == NULL)
	{
	  continue;
	}

      if (strcasecmp (name, "useSSL") == 0)
	{
	  cci_url_get_bool (value, &use_ssl);
	}
      else if (strcasecmp (name, "sslmode") == 0)
	{
	  sslmode = value;
	}
      else if (strcasecmp (name, "sslca") == 0)
	{
	  snprintf (sslca, sslca_size, "%s", value);
	}
    }

  /* the same rule as cci_conn_set_properties() */
  return cci_url_resolve_sslmode (sslmode, use_ssl);
}

/* indexed by T_CCI_SSLMODE */
static const char *cci_sslmode_labels[] = {
  "disabled", "required", "verify-ca", "verify-full"
};

/*
 * cci_sslmode_from_label - the T_CCI_SSLMODE written as sslmode=<label>, compared case-insensitively
 *   return: the mode, or -1 when the label names none
 */
int
cci_sslmode_from_label (const char *label)
{
  int i;

  if (label == NULL)
    {
      return -1;
    }

  for (i = 0; i < (int) DIM (cci_sslmode_labels); i++)
    {
      if (strcasecmp (label, cci_sslmode_labels[i]) == 0)
	{
	  return i;
	}
    }

  return -1;
}

const char *
cci_sslmode_label (int sslmode)
{
  if (sslmode < 0 || sslmode >= (int) DIM (cci_sslmode_labels))
    {
      return "unknown";
    }

  return cci_sslmode_labels[sslmode];
}
