/***************************************************************************
                             uwt/x86/memory.cc
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

/** @file uwt/x86/memory.cc
    @brief  implementation file for guest memory management

*/

#include <uwt/x86/memory.hh>
#include <istream>
#include <string>

namespace UWT
{
  Memory::Memory() {
    for( uint32_t idx = 0; idx < s_mapsize; idx++ )
      m_map[idx] = NULL;
  }

  Memory::~Memory() {
    for( uint32_t idx = 0; idx < s_mapsize; idx++ )
      delete m_map[idx];
  }

  void
  Memory::read( uint32_t _addr, std::string& _string ) const
  {
    while( true ) {
      uint8_t const* byte = getpage( _addr )->m_storage;
      for (uint32_t idx = _addr % Page::s_size; idx < Page::s_size; idx++ ) {
        if (byte[idx] == '\0')
          return;
        _string += byte[idx];
      }
      _addr = ((_addr / Page::s_size)+1)*Page::s_size;
    }
  }

  uint32_t
  Memory::write( uint32_t _addr, char const* _string )
  {
    uint32_t size = 0;
    uint8_t* dst = getpage( _addr )->m_storage + (_addr % Page::s_size);
    while( (*dst++ = _string[size++]) != '\0' )
      if( ((++_addr) % Page::s_size) == 0 ) dst = getpage( _addr )->m_storage;

    return size;
  }

  // uint32_t
  // Memory::read( whY::Field const& _field ) {
  //   if( _field.size > 4 ) throw AccessError;
  //   uint8_t buf[4];
  //   read( _field.offset, buf, _field.size );

  //   uint32_t word = 0;
  //   for( int idx = _field.size-1; idx >= 0; idx-- )
  //     word = (word << 8) + buf[idx];
  //   return word;
  // }

  // void
  // Memory::write( whY::Field const& _field, uint32_t _value ) {
  //   if( _field.size > 4 ) throw AccessError;
  //   uint8_t buf[4];
  //   for( int idx = 0; idx < _field.size; idx++ ) {
  //     buf[idx] = _value & 0xff;
  //     _value = _value >> 8;
  //   }
  //   write( _field.offset, buf, _field.size );
  //   return;
  // }

  void
  Memory::zfill( uint32_t _addr, uint32_t _size )
  {
    uint32_t chunk = Page::s_size - (_addr % Page::s_size);
    uint32_t start = _addr % Page::s_size;

    while( _size > 0 ) {
      if( chunk > _size ) chunk = _size;
      ::memset( (void*)(getpage( _addr )->m_storage + start), 0, chunk );
      _addr += chunk;
      _size -= chunk;
      start = 0;
      chunk = Page::s_size;
    }
  }

  void
  Memory::write( uint32_t _addr, std::istream& _data, uint32_t _size )
  {
    uint32_t chunk = Page::s_size - (_addr % Page::s_size);
    uint32_t start = _addr % Page::s_size;

    while (_size > 0)
      {
        if (chunk > _size)
          chunk = _size;
        _data.read( (char*)(&getpage( _addr )->m_storage[start]), chunk );
        _addr += chunk;
        _size -= chunk;
        start = 0;
        chunk = Page::s_size;
      }
  }

  Memory::Page const*
  Memory::getpage( uint32_t _addr ) const
  {
    uint32_t index = _addr / Page::s_size;
    uint32_t mapKey = index % s_mapsize;
    for( Page* page = m_map[mapKey]; page; page = page->m_next )
      if( page->m_index == index ) return page;
    throw AccessError;
    return 0;
  }

  void
  Memory::read( uint32_t _addr, uint8_t* _buffer, uint32_t _size ) const
  {
    while( _size > 0 ) {
      Page const* page = getpage( _addr );
      uint32_t pageoffset = _addr % Page::s_size;
      uint32_t   subChunkSize = Page::s_size - pageoffset;
      subChunkSize = (subChunkSize < _size) ? subChunkSize : _size;
      memcpy( _buffer, (page->m_storage + pageoffset), subChunkSize );
      _addr += subChunkSize;
      _buffer += subChunkSize;
      _size -= subChunkSize;
    }
  }
};
