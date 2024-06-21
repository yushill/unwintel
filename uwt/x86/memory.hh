/***************************************************************************
                             uwt/x86/memory.hh
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

/** @file uwt/x86/memory.hh
    @brief  header file for guest memory management

*/

#ifndef UWT_MEMORY_HH
#define UWT_MEMORY_HH

#include <iosfwd>
#include <uwt/fwd.hh>
#include <cstring>
#include <string>

namespace UWT {
  struct Memory
  {
    enum exception_t { AccessError, };

    struct Page
    {
      Page( Page* _next, uint32_t _idx )
        : m_next( _next ), m_index( _idx ), m_storage( new uint8_t[s_size] )
      { memset( (void*)m_storage, 0, s_size ); }
      ~Page() { delete [] m_storage; delete m_next; }

      Page*                   m_next;
      uint32_t                  m_index;
      uint8_t*                  m_storage;
      static const uint32_t     s_size = 0x1000;
    };

    static const uint32_t       s_mapsize = 0x1000;
    Page*                     m_map[s_mapsize];

    Memory();
    ~Memory();

    Page const*      getpage( uint32_t _addr ) const;

    Page*            getpage( uint32_t _addr )
    {
      uint32_t index = _addr / Page::s_size;
      uint32_t mapKey = index % s_mapsize;
      if( m_map[mapKey] and m_map[mapKey]->m_index == index )
        return m_map[mapKey]; ///< In cache ...
      for( Page** page = &(m_map[mapKey]); *page; page = &((**page).m_next) ) {
        if( (**page).m_index != index ) continue;
        Page* found = *page;
        *page = (**page).m_next; ///< Extracting the page
        found->m_next = m_map[mapKey]; ///< Make it the first
        m_map[mapKey] = found;
        return found;
      }
      return (m_map[mapKey] = new Page( m_map[mapKey], index ));
    }

    uint32_t              write( uint32_t _addr, char const* _string );
    void                  read( uint32_t _addr, std::string& _string ) const;

    // void                  write( whY::Field const& _field, uint32_t _value );
    // uint32_t              read( whY::Field const& _field );

    void                  write( uint32_t _addr, uint8_t const* _buffer, uint32_t _size )
    {
      while( _size > 0 ) {
        Page const* page = getpage( _addr );
        uint32_t pageoffset = _addr % Page::s_size;
        uint32_t   subChunkSize = Page::s_size - pageoffset;
        subChunkSize = (subChunkSize < _size) ? subChunkSize : _size;
        memcpy( (page->m_storage + pageoffset), _buffer, subChunkSize );
        _addr += subChunkSize;
        _buffer += subChunkSize;
        _size -= subChunkSize;
      }
    }
    void                  read( uint32_t _addr, uint8_t* _buffer, uint32_t _size ) const;
    void                  read( uint32_t _addr, uint8_t* _buffer, uint32_t _size )
    {
      while( _size > 0 ) {
        Page* page = getpage( _addr );
        uint32_t   subChunkSize = Page::s_size - (_addr % Page::s_size);
        subChunkSize = (subChunkSize<_size)?subChunkSize:_size;
        memcpy( _buffer, (page->m_storage+(_addr % Page::s_size)), subChunkSize );
        _addr += subChunkSize;
        _buffer += subChunkSize;
        _size -= subChunkSize;
      }
    }

    void                  write( uint32_t _addr, std::istream&, uint32_t );
    void                  zfill( uint32_t _addr, uint32_t _size );

    template <uint32_t t_size>
    struct MemRef
    {
      Memory&           m_mem;
      uint32_t            m_addr;

      MemRef( Memory& _mem, uint32_t _addr ) : m_mem( _mem ), m_addr( _addr ) {}
      MemRef&               operator= ( uint32_t _word ) {
        uint8_t  buf[t_size];
        for( uint32_t idx = 0; idx < t_size; idx++ )
          buf[idx] = (_word >> (idx*8)) & 0xff;
        m_mem.write( m_addr, buf, t_size );
        return *this;
      }
    };
    template <uint32_t t_size>
    MemRef<t_size>        w( uint32_t _addr ) { return MemRef<t_size>( *this, _addr ); }

    template <uint32_t t_size>
    uint32_t              r( uint32_t _addr ) const {
      uint32_t word = 0;
      uint8_t buf[t_size];
      read( _addr, buf, t_size );
      for( int idx = t_size-1; idx >= 0; idx-- )
        word = (word << 8) + buf[idx];
      return word;
    }

    template <uint32_t t_size>
    uint32_t              r( uint32_t _addr ) {
      uint32_t word = 0;
      uint8_t buf[t_size];
      read( _addr, buf, t_size );
      for( int idx = t_size-1; idx >= 0; idx-- )
        word = (word << 8) + buf[idx];
      return word;
    }

  };
};

#endif /* UWT_MEMORY_HH */
