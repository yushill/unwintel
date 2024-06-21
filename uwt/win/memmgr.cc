/***************************************************************************
                             uwt/win/memmgr.cc
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

/** @file uwt/win/memmgr.cc
    @brief  guest memory allocation

*/

#include <uwt/win/memmgr.hh>
#include <uwt/win/module.hh>
#include <uwt/utils/misc.hh>
#include <ostream>
#include <cassert>

namespace UWT
{
  Range::Range(): m_first( 0 ), m_last( top ) {}

  Range::Range( uint32_t _first, uint32_t _last )
    : m_first( _first ), m_last( _last )
  {
    // Sanity check
    if (_last < _first) throw empty;
  }

  Range
  Range::split( Range const& _range, bool _dir ) const
  {
    uint32_t bound;
    if (_dir) {
      // split toward upper regions
      bound = _range.m_last;
      if (++bound == 0) throw 0;
    } else {
      // split toward lower regions
      bound = _range.m_first;
      if (bound-- == 0) throw 0;
    }

    uint32_t first = _dir ? bound : m_first;
    uint32_t last  = _dir ? m_last : bound;
    return Range( first, last );
  }

  MemMgr::MemMgr( uint32_t _alignment )
    : m_mask( _alignment - 1 )
  {
    assert( _alignment and ((m_mask & _alignment) == 0) );
  }

  std::ostream&
  MemMgr::dump( std::ostream& sink ) const
  {
    for (auto && x: modmap) {
      sink << strx::fmt( "[%#010x,%#010x] %s\n", x.first.m_first, x.first.m_last, x.second->getname().c_str() );
    }

    return sink;
  }

  void
  MemMgr::record( Range& _range, Module* module )
  {
    Range range( _range.m_first & ~m_mask, _range.m_last | m_mask );

    ModMap::const_iterator itr = modmap.lower_bound( range );

    if ((itr != modmap.end()) and not (range < itr->first))
      throw Range::empty;

    modmap.emplace_hint( itr, std::piecewise_construct, std::forward_as_tuple( range ), std::forward_as_tuple( module ) );
    _range = range;
  }

  void
  MemMgr::allocate( Range& region, bool topdown, uint32_t size, Module* module )
  {
    // Range range( region.m_first & ~m_mask, region.m_last | m_mask );

    size = (size + m_mask) & ~m_mask;
    uint32_t span = size-1;
    if ((size == 0) or (span > (region.m_last - region.m_first)))
      throw Range::empty;

    ModMap::const_iterator itr;
    Range range;

    if (topdown) {
      range = Range( region.m_last - span, region.m_last );
    } else {
      range = Range( region.m_first, region.m_first + span );
    }

    itr = modmap.lower_bound( range );
    if ((itr != modmap.end()) and not (range < itr->first))
      {
        if (topdown) {
          do {
            uint32_t last = itr->first.m_first;
            if (last <= (region.m_first + span))
              throw Range::empty;
            --last;
            range = Range( last - span, last );
            --itr;
          } while ((itr != modmap.end()) and not (itr->first < range));
        } else {
          do {
            uint32_t first = itr->first.m_last;
            if (first >= (region.m_last - span))
              throw Range::empty;
            ++first;
            range = Range( first, first + span );
            ++itr;
          } while ((itr != modmap.end()) and not (range < itr->first));
        }
      }

    modmap.emplace_hint( itr, std::piecewise_construct, std::forward_as_tuple( range ), std::forward_as_tuple( module ) );
    region = range;
  }

  Module&
  MemMgr::module_at( uint32_t addr )
  {
    Range loc(addr);
    ModMap::const_iterator itr = modmap.lower_bound( loc );

    if ((itr == modmap.end()) or (loc < itr->first))
      throw Range::empty;

    return *itr->second;
  }
};
