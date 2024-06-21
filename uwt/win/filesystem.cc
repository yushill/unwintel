/***************************************************************************
                           uwt/win/filesystem.cc
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

/** @file uwt/win/filesystem.cc
    @brief  implementation file for windows filesystem emulation

*/

#include <uwt/win/filesystem.hh>

namespace UWT {
  void
  fsconvert_x2w( std::string& _dst, char const* _src ) {
    if( _src[0] == '/' )
      _dst += "X:";
    for( char const* ptr = _src-1; *++ptr; ) {
      switch( *ptr ) {
      case '/': _dst += '\\'; break;
      default:  _dst += *ptr; break;
      }
    }
  }
};
