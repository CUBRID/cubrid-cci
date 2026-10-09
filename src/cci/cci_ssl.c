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

/* TLS 1.2 key exchange is ephemeral only: !kRSA drops static RSA, which gives no forward secrecy.
 * The same list as the broker (CAS_SSL_CIPHER_LIST in cas_ssl.c). */
#define CCI_SSL_CIPHER_LIST	"ECDHE+AESGCM:ECDHE+CHACHA20:DHE+AESGCM:!aNULL:!kRSA"

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

  SSL_CTX_set_verify (ctx, SSL_VERIFY_PEER, NULL);
  return 1;
}


SSL *
create_ssl (SOCKET srv_sock_fd, SSL_CTX * ctx)
{
  SSL *ssl;
  int server = (int) srv_sock_fd;

  ssl = SSL_new (ctx);
  if (ssl == NULL)
    {
      /*The creation of a new SSL structure failed. */
      return NULL;
    }

  if (SSL_set_fd (ssl, server) == 0)
    {
      /*The operation failed.Check the error stack to find out why. */
      return NULL;
    }

  return ssl;
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
