/*
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

#ident "$Id$"
#include "cci_ssl.h"
#include <string.h>
#include <openssl/x509v3.h>

/* TLS 1.2 key exchange is ECDHE only: !kRSA drops static RSA, which gives no forward secrecy, and DHE is left out
 * as the broker leaves it out - it has no DH parameters, and DHE costs the server far more than ECDHE.
 * The same list as the broker (CAS_SSL_CIPHER_LIST in cas_ssl.c). */
#define CCI_SSL_CIPHER_LIST	"ECDHE+AESGCM:ECDHE+CHACHA20:!aNULL:!kRSA"

SSL_CTX *
create_ssl_ctx ()
{
  const SSL_METHOD *method;
  SSL_CTX *ctx = NULL;

  method = TLS_client_method ();

  ctx = SSL_CTX_new (method);
  if (ctx == NULL)
    {
      /*The creation of a new SSL_CTX object failed. */
      return NULL;
    }

  if (SSL_CTX_set_min_proto_version (ctx, TLS1_2_VERSION) == 0
      || SSL_CTX_set_cipher_list (ctx, CCI_SSL_CIPHER_LIST) == 0)
    {
      SSL_CTX_free (ctx);
      return NULL;
    }

  return ctx;
}

/*
 * set_ssl_ctx_verify - have the server certificate checked against the certificates in ca_file, and nothing else
 *   return: 1, or 0 when ca_file cannot be read or holds no certificate
 *
 * The OS default store is never loaded (no SSL_CTX_set_default_verify_paths): the bundled OpenSSL looks for it
 * under the directory it was built in, and any public CA could then vouch for the broker.
 */
int
set_ssl_ctx_verify (SSL_CTX * ctx, const char *ca_file)
{
  if (ca_file == NULL || SSL_CTX_load_verify_locations (ctx, ca_file, NULL) != 1)
    {
      return 0;
    }

  /* Every certificate in ca_file is an anchor, not only self-signed roots: an intermediate CA, or the broker's own
   * certificate, put there is trusted as JDBC (JSSE) trusts it. The chain below it is checked as usual. */
  X509_VERIFY_PARAM_set_flags (SSL_CTX_get0_param (ctx), X509_V_FLAG_PARTIAL_CHAIN);
  SSL_CTX_set_verify (ctx, SSL_VERIFY_PEER, NULL);
  return 1;
}


static bool
is_ip_literal (const char *host)
{
  ASN1_OCTET_STRING *ip = a2i_IPADDRESS (host);

  if (ip == NULL)
    {
      return false;
    }

  ASN1_OCTET_STRING_free (ip);
  return true;
}

/*
 * create_ssl - the TLS session for srv_sock_fd
 *   hostname(in): the broker as written in the URL, or NULL or ""; sent as SNI unless it is an IP literal (RFC 6066)
 *   verify_host(in): also require the certificate to name hostname - in an IP SAN for an IP literal, else a DNS SAN
 *
 *   An absolute name ("db.example.com.") is the same host without its trailing dot, and both uses below take it
 *   without: RFC 6066 writes SNI without it, SSL_set1_host() refuses it from OpenSSL 4.0, and JDBC (JSSE) checks the
 *   certificate without it. The address was already looked up from the name as written.
 */
SSL *
create_ssl (SOCKET srv_sock_fd, SSL_CTX * ctx, const char *hostname, bool verify_host)
{
  SSL *ssl;
  int server = (int) srv_sock_fd;
  char name[256];
  size_t len = (hostname != NULL) ? strlen (hostname) : 0;
  bool has_name, is_ip;

  /* longer than any host name, and longer than SNI can carry */
  if (len >= sizeof (name))
    {
      return NULL;
    }
  if (len > 0)
    {
      memcpy (name, hostname, len);
      if (name[len - 1] == '.')
	{
	  len--;
	}
    }
  name[len] = '\0';

  /* "" counts as no name: SSL_set1_host() takes it to mean "check no name" */
  has_name = (len > 0);
  is_ip = (has_name && is_ip_literal (name));

  ssl = SSL_new (ctx);
  if (ssl == NULL)
    {
      /*The creation of a new SSL structure failed. */
      return NULL;
    }

  if (SSL_set_fd (ssl, server) == 0)
    {
      /*The operation failed.Check the error stack to find out why. */
      goto error;
    }

  if (has_name && !is_ip && SSL_set_tlsext_host_name (ssl, name) != 1)
    {
      goto error;
    }

  if (verify_host)
    {
      /* refused rather than skipped: without a name, any certificate the chain check accepts would pass */
      if (!has_name)
	{
	  goto error;
	}

      if (is_ip ? X509_VERIFY_PARAM_set1_ip_asc (SSL_get0_param (ssl), name) != 1 : SSL_set1_host (ssl, name) != 1)
	{
	  goto error;
	}
    }

  return ssl;

error:
  SSL_free (ssl);
  return NULL;
}

int
connect_ssl (SSL * ssl)
{
  return SSL_connect (ssl);
}


void
cleanup_ssl (SSL * ssl)
{
  if (ssl != NULL)
    {
      SSL_shutdown (ssl);
      SSL_free (ssl);
    }
}

void
cleanup_ssl_ctx (SSL_CTX * ctx)
{
  if (ctx != NULL)
    {
      SSL_CTX_free (ctx);
    }
}
