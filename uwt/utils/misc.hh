/***************************************************************************
                          uwt/utils/misc.hh
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

/** @file uwt/utils/misc.hh
    @brief  header file for miscellaneous utilities

*/

#ifndef UWT_MISC_HH
#define UWT_MISC_HH

#include <string>
#include <cstring>
#include <inttypes.h>

namespace UWT
{
  template <uintptr_t N> bool startswith( char const (&ref)[N], char const* str ) { return strncmp( ref, str, N-1 ) == 0; }
  template <uintptr_t N> char const* popfirst( char const (&ref)[N], char const* str ) { return strncmp( ref, str, N-1 ) == 0 ? &str[N-1] : 0; }

  template <typename T> intptr_t cmp( T a, T b ) { return a < b ? -1 : a > b ? + 1 : 0; }
};

namespace strx
{
  std::string abspath( char const* src );
  std::string readlink( char const* src );
  std::string fmt( char const* _fmt, ... );
}

#endif // UWT_MISC_HH
