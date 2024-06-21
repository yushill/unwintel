/***************************************************************************
                             uwt/x86/calldb.hh
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

/** @file uwt/x86/calldb.hh
    @brief  header file for the translation cache

*/

#ifndef UWT_CALLDB_HH
#define UWT_CALLDB_HH

#include <uwt/fwd.hh>
#include <string>
#include <inttypes.h>

namespace UWT {
  // Functions map
  struct CallDB
  {
    struct Entry
    {
      Entry*            m_next;
      uint32_t          m_ip;
      hostcode_t        m_hostcode;
      std::string       m_name;

      Entry( Entry* _next, uint32_t _ip, hostcode_t _hostcode, std::string const& _name )
        : m_next( _next ), m_ip( _ip ), m_hostcode( _hostcode ), m_name( _name ) {}
      ~Entry() { delete m_next; }
    };
    static const
    uint32_t            s_size = 256;
    Entry*              m_map[s_size];

    enum exception_t { Conflict };
    struct TransRequest { uint32_t m_addr; TransRequest( uint32_t _addr ) : m_addr( _addr ) {} };

    CallDB();
    ~CallDB();

    Entry&            operator[] ( uint32_t _addr );

    CallDB&           add( uint32_t _addr, hostcode_t _function, std::string const& _name );
  };
};

#endif // UWT_CALLDB_HH
