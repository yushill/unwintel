/***************************************************************************
                          uwt/utils/misc.cc
                             -----------------
    begin                : Thu May 22 2003
    copyright            : (C) 2003 Yves Lhuillier
    authors              : Yves Lhuillier
    email                : yves.lhuillier@gmail.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 3        *
 *   as published by the Free Software Foundation.                         *
 *                                                                         *
 ***************************************************************************/

/** @file uwt/utils/misc.cc
    @brief  implementation file for miscellaneous utilities
*/

#include <string>
#include <stdexcept>
#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <unistd.h>
#include <inttypes.h>

namespace strx
{
  std::string abspath( char const* src )
  {
    std::string res;
    if (src[0] == '/')
      return src;
    uintptr_t xtra = strlen( src ) + 1;

    for (uintptr_t capacity = 128; true; capacity *= 2)
      {
        char storage[capacity+xtra]; /* allocation */

        if ( getcwd( storage, capacity ) ) {
          if( storage[0] != '/' )
            throw std::runtime_error("expecting unix paths");

          char* to = &storage[0];
          while (*to) to += 1;
          *to++ = '/';
          for (char const* from = src; *from;)
            *to++ = *from++;
          *to++ = '\0';

          res.assign( &storage[0] );

          break;
        }

        if (errno != ERANGE)
          throw std::runtime_error("internal error while retrieving CWD");
      }

    return res;
  }

  std::string readlink( char const* src )
  {
    std::string res;

    for (uintptr_t capacity = 128; true; capacity *= 2)
      {
        char storage[capacity]; /* allocation */

        uintptr_t size = ::readlink( src, &storage[0], capacity );
        if (size < 0)
          throw std::runtime_error("cannot read link");

        if (size >= capacity)
          continue;

        res.assign( &storage[0], &storage[size] );

        break;
      }

    return res;
  }

  std::string fmt( char const* _fmt, ... )
  {
    va_list ap;
    std::string res;

    for (intptr_t capacity = 128, size; true; capacity = (size > -1) ? size + 1 : capacity * 2)
      {
        /* allocation */
        char storage[capacity];
        /* Try to print in the allocated space. */
        va_start( ap, _fmt );
        size = vsnprintf( storage, capacity, _fmt, ap );
        va_end( ap );
        /* If it didn't work, try again with bigger storage */
        if (size < 0 or size >= capacity)
          continue;
        res.assign( &storage[0] );
        break;
      }

    return res;
  }

}
