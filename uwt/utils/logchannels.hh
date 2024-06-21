/***************************************************************************
                          uwt/utils/logchannels.hh
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

/** @file uwt/utils/logchannels.hh
    @brief  header file for logging utils

*/

#ifndef UWT_LOGCHANNELS_HH
#define UWT_LOGCHANNELS_HH

#include <unisim/util/dbgate/client/client.hh>
#include <cstdint>

namespace UWT
{
  // struct Log {
  //   struct ChannelGroup {
  //     std::string                               m_dir;
  //     std::map<std::string,std::ostream*>       m_map;
  //     static ChannelGroup*                      s_one;

  //     ChannelGroup( char const* _dir );
  //     ~ChannelGroup();
  //   };

  //   static std::ostream&      chan( char const* _name ) {
  //     if( not ChannelGroup::s_one ) throw ChannelGroup::s_one;
  //     std::ostream* stream = ChannelGroup::s_one->m_map[_name];
  //     return stream ? *stream : defaultchan( _name );
  //   }
  //   static std::ostream&      defaultchan( char const* _name );
  //   static std::ostream&      filechan( char const* _name, char const* _filename );
  //   static void               trashchan( char const* _name );
  //   static void               delchan( char const* _name );
  // };

  struct DebugStream : public unisim::util::dbgate::client::odbgstream
  {
    DebugStream(char const* path) : unisim::util::dbgate::client::odbgstream(dbgate_open(path)) {};
    void close() { dbgate_close(cd()); buf.cd = -1; rdbuf(0); }
    ~DebugStream() { dbgate_close(cd()); }
  };
};

#endif // UWT_LOGCHANNELS_HH
