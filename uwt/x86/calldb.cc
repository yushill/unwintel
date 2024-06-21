/***************************************************************************
                             uwt/x86/calldb.cc
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

/** @file uwt/x86/calldb.cc
    @brief  implementation file for the translation cache

*/

#include <uwt/x86/calldb.hh>
#include <uwt/x86/context.hh>
#include <uwt/utils/misc.hh>
#include <iostream>

namespace UWT
{
  CallDB::CallDB() {
    for( uint32_t idx = 0; idx < s_size; idx++ )
      m_map[idx] = 0;
  }

  CallDB::~CallDB() {
    for( uint32_t idx = 0; idx < s_size; idx++ )
      delete m_map[idx];
  }

  CallDB::Entry&
  CallDB::operator[]( uint32_t _addr )
  {
    for( Entry* node = m_map[_addr % s_size]; node; node = node->m_next ) {
      if( node->m_ip != _addr ) continue;
      return *node;
    }
    throw TransRequest( _addr );
  }

  CallDB&
  CallDB::add( uint32_t _addr, hostcode_t _function, std::string const& _name )
  {
    for (Entry* node = m_map[_addr % s_size]; node; node = node->m_next) {
      if (node->m_ip != _addr) continue;
      std::cerr << strx::fmt( "function %s conflicts with %s.\n", _name.c_str(), node->m_name.c_str() );
      throw Conflict;
    }
    m_map[_addr % s_size] =
      new Entry( m_map[_addr % s_size], _addr, _function, _name );
    return *this;
  }

};
