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
 * cci_t_lob.h -
 */

#ifndef	_CCI_T_LOB_H_
#define	_CCI_T_LOB_H_

#ident "$Id$"

/************************************************************************
 * IMPORTED SYSTEM HEADER FILES						*
 ************************************************************************/

/************************************************************************
 * IMPORTED OTHER HEADER FILES						*
 ************************************************************************/

#include "cci_common.h"
#include "cas_cci.h"

/************************************************************************
 * EXPORTED DEFINITIONS							*
 ************************************************************************/

#define LOB_IO_LENGTH 131072

/************************************************************************
 * EXPORTED TYPE DEFINITIONS						*
 ************************************************************************/

typedef struct
{
  T_CCI_U_TYPE type;
  int handle_size;
  char *handle;
} T_LOB;

/* the value an internal LOB handle (T_INTERNAL_LOB_HANDLE) holds */
typedef enum
{
  INTERNAL_LOB_HELD,		/* made by cci_blob_new: the content, uploaded when bound */
  INTERNAL_LOB_REF,		/* fetched: the locator, read from the server */
  INTERNAL_LOB_INLINE		/* fetched: the content itself (a value with no storage) */
} T_INTERNAL_LOB_STATE;

/* What cci_blob_new on PROTOCOL_V13 and a fetch of an internal BLOB/CLOB give the application. The type comes first
 * as in T_LOB, where it is 23/24, so one T_CCI_BLOB can point at either. */
typedef struct
{
  T_CCI_U_TYPE type;		/* CCI_U_TYPE_BLOB/CLOB */
  T_INTERNAL_LOB_STATE state;
  INT64 length;			/* bytes */
  INT64 id;			/* unique in the process: owner mark of a connection's read slot */
  char *bytes;			/* HELD: the content up to INTERNAL_LOB_MEM_MAX; INLINE: the content; REF: the locator */
  int bytes_size;		/* HELD: allocated; INLINE, REF: used */
  int spill_fd;			/* HELD past INTERNAL_LOB_MEM_MAX: the whole content in an unlinked temp file; else -1 */
  int read_con_id;		/* REF: the connection that read it last, 0 if none */
} T_INTERNAL_LOB_HANDLE;

#define INTERNAL_LOB_MEM_MAX (1024 * 1024)
#define INTERNAL_LOB_MAX_LENGTH ((INT64) 1 << 32)

#define IS_INTERNAL_LOB_HANDLE(p) \
  (((T_LOB *) (p))->type == CCI_U_TYPE_BLOB || ((T_LOB *) (p))->type == CCI_U_TYPE_CLOB)

/************************************************************************
 * EXPORTED FUNCTION PROTOTYPES						*
 ************************************************************************/
extern INT64 t_lob_get_size (char *pack_bytes);
extern void t_lob_set_size (T_LOB * lob, INT64 new_size);

extern int t_internal_lob_new (T_CCI_U_TYPE type, T_INTERNAL_LOB_HANDLE ** lob);
extern int t_internal_lob_from_fetch (T_CCI_U_TYPE type, T_CCI_INTERNAL_LOB * fetched, T_INTERNAL_LOB_HANDLE ** lob);
extern int t_internal_lob_write (T_INTERNAL_LOB_HANDLE * lob, INT64 pos, int length, const char *buf);
extern int t_internal_lob_read (T_INTERNAL_LOB_HANDLE * lob, INT64 pos, int length, char *buf);
extern void t_internal_lob_free (T_INTERNAL_LOB_HANDLE * lob);

/************************************************************************
 * EXPORTED VARIABLES							*
 ************************************************************************/

#endif /* _CCI_T_LOB_H_ */
