/***************************************************************************
                             uwt/win/memmgr.hh
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

/** @file uwt/win/memmgr.hh
    @brief  guest memory allocation

*/

#ifndef UWT_WIN_MEMMGR_HH
#define UWT_WIN_MEMMGR_HH

#include <uwt/fwd.hh>
#include <iosfwd>
#include <map>
#include <inttypes.h>

namespace UWT
{
  /** Range container ... to be inserted in a tree ?
   * Also links to a module
   * find:   ...
   */
  struct Range
  {
    enum Empty { empty };
    Range();
    explicit Range( uint32_t _sole ) : m_first(_sole), m_last(_sole) {};
    Range( uint32_t _first, uint32_t _last );

    bool                        operator>( Range const& _range ) const { return m_first > _range.m_last; };
    bool                        operator<( Range const& _range ) const { return m_last < _range.m_first; };
    Range                       narrow( uint32_t _size, bool _dir ) const;
    Range                       split( Range const& _range, bool _dir ) const;

    static const uint32_t       top = uint32_t( -1 );

    uint32_t                    m_first, m_last;
  };

  struct MemMgr
  {
    enum exception_t { NotFound, };
    MemMgr( uint32_t _alignment );

    void                  record( Range& _range, Module* _module );
    void                  allocate( Range& _range, bool _dir, uint32_t _size, Module* _module );
    std::ostream&         dump( std::ostream& _sink ) const;
    Module&               module_at( uint32_t addr );

  private:
    typedef std::map<Range,Module*> ModMap;
    ModMap                modmap;
    uint32_t              m_mask;
  };
};

#endif // UWT_WIN_MEMMGR_HH
