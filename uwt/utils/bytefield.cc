/***************************************************************************
                          uwt/utils/bytefield.cc
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

/** @file uwt/utils/bytefield.cc
    @brief  implementation file for logging utils

*/

#include <uwt/utils/bytefield.hh>
#include <istream>
#include <cstdlib>

namespace UWT
{
  void
  ByteField::getstring(char* sink, std::istream& source) const
  {
    if (not source.seekg(offset).good())
      throw ReadError();

    if (not source.read(sink, size).good())
      throw ReadError();
  }

  ByteField&
  ByteField::seek( char const* _desc, char const* _key, uintptr_t baseoffset )
  {
    if (not _desc or not _key)
      throw SeekError();

    this->offset = baseoffset;

    while (*_desc)
      {
        char const* ptr = _key;

        while (*ptr and *ptr != ':' and (*ptr == *_desc)) { ptr++; _desc++; }

        if ((*ptr == '\0' or *ptr == ':') and *_desc == ':')
          {
            // Found...
            this->size = strtoull( _desc+1, (char**)( &_desc ), 0 );
            if (*_desc != ';')
              throw SeekError();
            return *this;
          }

        while (*_desc != ':') { if (not *_desc) throw SeekError(); ++_desc; }

        this->offset += strtoull( _desc+1, (char**)( &_desc ), 0 );

        if (*_desc != ';')
          throw SeekError();

        ++_desc;
      }

    throw SeekError();

    return *this;
  }

  ByteField&
  ByteField::all( char const* _desc, uintptr_t baseoffset )
  {
    if (not _desc) throw SeekError();

    this->offset = 0;

    while (*_desc)
      {
        while (*_desc != ':') { if (not *_desc) throw SeekError(); ++_desc; }

        this->offset += strtoull( _desc+1, (char**)( &_desc ), 0 );

        if (*_desc != ';')
          throw SeekError();

        ++_desc;
      }

    this->size = this->offset;
    this->offset = baseoffset;

    return *this;
  }

}
