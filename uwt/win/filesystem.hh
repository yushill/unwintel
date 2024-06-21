/***************************************************************************
                           uwt/win/filesystem.hh
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

/** @file uwt/win/filesystem.hh
    @brief  header file for windows filesystem emulation

*/

#ifndef UWT_FILESYSTEM_HH
#define UWT_FILESYSTEM_HH

#include <string>
#include <inttypes.h>

namespace UWT {
  void  fsconvert_x2w( std::string& _dst, char const* _src );
};

#endif // UWT_FILESYSTEM_HH

