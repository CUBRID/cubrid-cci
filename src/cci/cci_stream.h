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

/*
 * cci_stream.h - The stream calls CCI keeps for its own use
 *
 * The uploads a driver opens itself and the downloads: only the LOB code reaches them, and an application reads and
 * writes BLOB and CLOB values through the LOB calls of cas_cci.h. The calls an application uses for
 * COPY ... FROM STDIN are in cas_cci.h. Not installed.
 */

#ifndef _CCI_STREAM_H_
#define _CCI_STREAM_H_

#include "cas_cci.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* A config starts with the kind's version, a big-endian int; CCI reads none of it. Besides the refusals cas_cci.h
   * lists, the open can fail with -1219 when the kind is unknown, is opened in the other direction or only by its SQL
   * statement, or the config version is above the highest one the server takes, and with -1220 when the config has
   * no version or one below 1, or the transaction already holds the most download handles it may (1024). */

  /* flags of the calls below, and of what cci_stream_read_open () / cci_stream_read () report */
#define CCI_STREAM_FINAL		0x01	/* cci_stream_init (): data is the whole value */
#define CCI_STREAM_RELEASE_AT_EOF	0x01	/* cci_stream_read_open (), cci_stream_read () */
#define CCI_STREAM_EOF			0x01	/* reply */
#define CCI_STREAM_RELEASED		0x02	/* reply: the handle is gone */

  extern int cci_stream_init (int mapped_conn_id, int stream_kind, const char *config, int config_len, char flags,
			      const char *data, int data_len, long long *count, T_CCI_ERROR * err_buf);
  /* A download handle lasts only as long as the transaction that opened it: a commit ends it, and in auto-commit so
   * does any other statement on the connection. A read on it then fails with -1218. */
  extern int cci_stream_read_open (int mapped_conn_id, int stream_kind, const char *config, int config_len,
				   int first_max, char flags, long long *handle, char *buf, int *nread,
				   char *reply_flags, T_CCI_ERROR * err_buf);
  extern int cci_stream_read (int mapped_conn_id, long long handle, char *buf, int max, char flags, int *nread,
			      char *reply_flags, T_CCI_ERROR * err_buf);
  extern int cci_stream_release (int mapped_conn_id, long long handle, T_CCI_ERROR * err_buf);

#ifdef __cplusplus
}
#endif

#endif				/* _CCI_STREAM_H_ */
