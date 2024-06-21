/***************************************************************************
                          uwt/utils/bytefield.hh
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

/** @file uwt/utils/bytefield.hh
    @brief  header file for bytefield utils
*/

#ifndef UWT_BYTEFIELD_HH
#define UWT_BYTEFIELD_HH

#include <iosfwd>
#include <cstdint>

namespace UWT
{
  struct ByteField
  {
    struct SeekError {};
    struct CapacityError {};
    struct ReadError {};

    ByteField() : offset( 0 ), size( 0 ) {}
    ByteField( uintptr_t _offset, uintptr_t _size ) : offset( _offset ), size( _size ) {}

    ByteField&              seek( char const* _desc, char const* _key, uintptr_t baseoffset );
    ByteField&              all( char const* _desc, uintptr_t _baseoffset );
    uintptr_t               upper() { return this->offset + this->size; }

    template <typename T> T getint( std::istream& source ) const
    {
      this->fits( sizeof (T) );
      char    buf[sizeof (T)];
      getstring(&buf[0], source);
      T result = T();
      for (unsigned idx = size; idx-- > 0;)
        result = result << 8 | T(uint8_t(buf[idx]));
      return result;
    }

    void                    getstring(char*, std::istream& source) const;
    ByteField&              fits(uintptr_t bound) { if (size > bound) throw CapacityError(); return *this; }
    ByteField const&        fits(uintptr_t bound) const { if (size > bound) throw CapacityError(); return *this; }

    uintptr_t               offset;
    uintptr_t               size;
  };
};

#endif // UWT_BYTEFIELD_HH
