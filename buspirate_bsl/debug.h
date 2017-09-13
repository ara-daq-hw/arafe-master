#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <assert.h>
#include <stdarg.h>
#include <sys/time.h>

// ------------------------------------------------------------------
static inline void __debug_more_time__()
{
  struct timeval tv;
  assert(gettimeofday(&tv, NULL) >= 0);
  fprintf(DEBUG_STREAM, "%ld.%.6ld:",
	    (long) tv.tv_sec, (long) tv.tv_usec);
}

// ------------------------------------------------------------------
static inline void __debug__(const char * msg, ...)
{
#ifdef DEBUG
  va_list ap;
  va_start(ap, msg);
  fprintf(DEBUG_STREAM, "DEBUG:"DEBUG_ID);
  __debug_more_time__();
  vfprintf(DEBUG_STREAM, msg, ap);
  va_end(ap);
#endif
}

// ------------------------------------------------------------------
static inline void __debug_more__(const char * msg, ...)
{
#ifdef DEBUG
  va_list ap;
  va_start(ap, msg);
  vfprintf(DEBUG_STREAM, msg, ap);
  va_end(ap);
#endif
}

// ------------------------------------------------------------------
static inline void __debug_more_hex_buf__(unsigned char * buf, int nlen)
{
  while (nlen--)
    __debug_more__(" 0x%.2X", *(buf++));
}

#ifdef DEBUG
#define __debug_cond(CODE) {CODE}
#else
#define __debug_cond(CODE)
#endif /* DEBUG */

// ------------------------------------------------------------------
#ifdef DEBUG
#define __debug_perror(MSG) perror((MSG))
#else
#define __debug_perror(MSG)
#endif /* DEBUG */

#endif /* __DEBUG_H__ */
