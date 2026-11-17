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
 * cci_t_lob.c -
 */

#ident "$Id$"

/************************************************************************
 * IMPORTED SYSTEM HEADER FILES						*
 ************************************************************************/
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WINDOWS)
#include <winsock.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#endif

/************************************************************************
 * OTHER IMPORTED HEADER FILES						*
 ************************************************************************/

#include "cci_net_buf.h"
#include "cci_t_lob.h"

/************************************************************************
 * PRIVATE DEFINITIONS							*
 ************************************************************************/

/************************************************************************
 * PRIVATE TYPE DEFINITIONS						*
 ************************************************************************/

/************************************************************************
 * PRIVATE FUNCTION PROTOTYPES						*
 ************************************************************************/

/************************************************************************
 * INTERFACE VARIABLES							*
 ************************************************************************/

/************************************************************************
 * PUBLIC VARIABLES							*
 ************************************************************************/

/************************************************************************
 * PRIVATE VARIABLES							*
 ************************************************************************/

/************************************************************************
 * IMPLEMENTATION OF INTERFACE FUNCTIONS 				*
 ************************************************************************/

/************************************************************************
 * IMPLEMENTATION OF PUBLIC FUNCTIONS	 				*
 ************************************************************************/

INT64
t_lob_get_size (char *pack_bytes)
{
  INT64 size;
  INT64 temp;

  memcpy (&temp, pack_bytes + 4, sizeof (INT64));
  size = ntohi64 (temp);
  return size;
}

void
t_lob_set_size (T_LOB * lob, INT64 new_size)
{
  new_size = htoni64 (new_size);
  memcpy (lob->handle + 4, &new_size, sizeof (INT64));
}

static T_INTERNAL_LOB_HANDLE *
internal_lob_alloc (T_CCI_U_TYPE type, T_INTERNAL_LOB_STATE state)
{
  static volatile UINT64 last_id = 0;
  T_INTERNAL_LOB_HANDLE *lob;

  lob = (T_INTERNAL_LOB_HANDLE *) MALLOC (sizeof (T_INTERNAL_LOB_HANDLE));
  if (lob != NULL)
    {
      memset (lob, 0, sizeof (T_INTERNAL_LOB_HANDLE));
      lob->type = type;
      lob->state = state;
      lob->id = (INT64) ATOMIC_INC_64 (&last_id, 1);
      lob->spill_fd = -1;
    }
  return lob;
}

int
t_internal_lob_new (T_CCI_U_TYPE type, T_INTERNAL_LOB_HANDLE ** lob)
{
  *lob = internal_lob_alloc (type, INTERNAL_LOB_HELD);
  return (*lob == NULL) ? CCI_ER_NO_MORE_MEMORY : CCI_ER_NO_ERROR;
}

/* a copy of what the fetch buffer holds, so the handle outlives the cursor */
int
t_internal_lob_from_fetch (T_CCI_U_TYPE type, T_CCI_INTERNAL_LOB * fetched, T_INTERNAL_LOB_HANDLE ** lob)
{
  bool is_ref = (fetched->locator != NULL);
  int size = is_ref ? fetched->locator_size : fetched->content_size;

  *lob = internal_lob_alloc (type, is_ref ? INTERNAL_LOB_REF : INTERNAL_LOB_INLINE);
  if (*lob == NULL)
    {
      return CCI_ER_NO_MORE_MEMORY;
    }
  (*lob)->length = fetched->length;
  (*lob)->bytes_size = size;
  (*lob)->bytes = (char *) MALLOC (size > 0 ? size : 1);
  if ((*lob)->bytes == NULL)
    {
      FREE_MEM (*lob);
      return CCI_ER_NO_MORE_MEMORY;
    }
  if (size > 0)
    {
      memcpy ((*lob)->bytes, is_ref ? fetched->locator : fetched->content, size);
    }
  return CCI_ER_NO_ERROR;
}

/* glibc tmpfile () ignores TMPDIR; the file is unlinked at once, so nothing is left behind if the process dies */
static int
internal_lob_spill_open (void)
{
#if defined(WINDOWS)
  char dir[MAX_PATH], path[MAX_PATH];

  if (GetTempPathA (sizeof (dir), dir) == 0 || GetTempFileNameA (dir, "cci", 0, path) == 0)
    {
      return -1;
    }
  return _open (path, _O_RDWR | _O_BINARY | _O_TEMPORARY, _S_IREAD | _S_IWRITE);
#else
  const char *dir = getenv ("TMPDIR");
  char path[PATH_MAX];
  int fd;

  if (dir == NULL || dir[0] == '\0')
    {
      dir = "/tmp";
    }
  if (snprintf (path, sizeof (path), "%s/cci_lob_XXXXXX", dir) >= (int) sizeof (path))
    {
      return -1;
    }
  fd = mkstemp (path);
  if (fd >= 0)
    {
      unlink (path);
      fcntl (fd, F_SETFD, FD_CLOEXEC);
    }
  return fd;
#endif
}

/* all of length bytes at pos, or -1 */
static int
internal_lob_spill_io (int fd, char *buf, int length, INT64 pos, bool is_write)
{
  int n;

  while (length > 0)
    {
#if defined(WINDOWS)
      n = (_lseeki64 (fd, pos, SEEK_SET) < 0) ? -1 : (is_write ? _write (fd, buf, length) : _read (fd, buf, length));
#else
      n = (int) (is_write ? pwrite (fd, buf, length, (off_t) pos) : pread (fd, buf, length, (off_t) pos));
      if (n < 0 && errno == EINTR)
	{
	  continue;
	}
#endif
      if (n <= 0)
	{
	  return -1;
	}
      buf += n;
      length -= n;
      pos += n;
    }
  return 0;
}

/*
 * t_internal_lob_write () - Write a HELD value at any position, as 11.4 wrote its file
 *   return: length, or an error; on an error the handle keeps its length
 *
 * A gap between the end and pos reads as zeros. Up to INTERNAL_LOB_MEM_MAX the content is in memory, past it all of it
 * moves to a temp file.
 */
int
t_internal_lob_write (T_INTERNAL_LOB_HANDLE * lob, INT64 pos, int length, const char *buf)
{
  INT64 end = pos + length;

  if (lob->spill_fd < 0 && end > INTERNAL_LOB_MEM_MAX)
    {
      int fd = internal_lob_spill_open ();

      if (fd < 0 || (lob->length > 0 && internal_lob_spill_io (fd, lob->bytes, (int) lob->length, 0, true) < 0))
	{
	  if (fd >= 0)
	    {
	      close (fd);
	    }
	  return CCI_ER_FILE;
	}
      FREE_MEM (lob->bytes);
      lob->bytes_size = 0;
      lob->spill_fd = fd;
    }

  if (lob->spill_fd >= 0)
    {
      if (internal_lob_spill_io (lob->spill_fd, (char *) buf, length, pos, true) < 0)
	{
	  /* bytes past the old end would show in a later gap */
#if defined(WINDOWS)
	  (void) _chsize_s (lob->spill_fd, lob->length);
#else
	  (void) ftruncate (lob->spill_fd, (off_t) lob->length);
#endif
	  return CCI_ER_FILE;
	}
    }
  else
    {
      if (end > lob->bytes_size)
	{
	  int size = (lob->bytes_size > 0) ? lob->bytes_size : 4096;
	  char *bytes;

	  while (size < end)
	    {
	      size *= 2;
	    }
	  if (size > INTERNAL_LOB_MEM_MAX)
	    {
	      size = INTERNAL_LOB_MEM_MAX;
	    }
	  bytes = (char *) REALLOC (lob->bytes, size);
	  if (bytes == NULL)
	    {
	      return CCI_ER_NO_MORE_MEMORY;
	    }
	  lob->bytes = bytes;
	  lob->bytes_size = size;
	}
      if (pos > lob->length)
	{
	  memset (lob->bytes + lob->length, 0, (size_t) (pos - lob->length));
	}
      memcpy (lob->bytes + pos, buf, length);
    }

  if (end > lob->length)
    {
      lob->length = end;
    }
  return length;
}

/* HELD and INLINE: min (length, size - pos) bytes from pos (0 <= pos < size) */
int
t_internal_lob_read (T_INTERNAL_LOB_HANDLE * lob, INT64 pos, int length, char *buf)
{
  int n = (lob->length - pos < length) ? (int) (lob->length - pos) : length;

  if (lob->spill_fd >= 0)
    {
      if (internal_lob_spill_io (lob->spill_fd, buf, n, pos, false) < 0)
	{
	  return CCI_ER_FILE;
	}
    }
  else if (n > 0)
    {
      memcpy (buf, lob->bytes + pos, n);
    }
  return n;
}

void
t_internal_lob_free (T_INTERNAL_LOB_HANDLE * lob)
{
  if (lob->spill_fd >= 0)
    {
      close (lob->spill_fd);
    }
  FREE_MEM (lob->bytes);
  FREE_MEM (lob);
}

/************************************************************************
 * IMPLEMENTATION OF PRIVATE FUNCTIONS	 				*
 ************************************************************************/
