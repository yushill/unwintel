/***************************************************************************
                          uwt/utils/logchannels.cc
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

/** @file uwt/utils/logchannels.cc
    @brief  implementation file for logging utils

*/

#include <uwt/utils/logchannels.hh>

extern "C"
{
  DBGATE_DEFS;
}

namespace UWT
{
  // Log::ChannelGroup* Log::ChannelGroup::s_one = 0;

  // Log::ChannelGroup::ChannelGroup( char const* _dir )
  //   : m_dir( _dir )
  // {
  //   if( s_one ) throw s_one;
  //   s_one = this;
  // }

  // Log::ChannelGroup::~ChannelGroup() {
  //   for( std::map<std::string,std::ostream*>::iterator node = m_map.begin(); node != m_map.end(); ++node )
  //     delete node->second;
  // }

  // std::ostream&
  // Log::defaultchan( char const* _name )
  // {
  //   std::string path( ChannelGroup::s_one->m_dir + '/' + _name + ".log" );
  //   std::ostream*& sink = ChannelGroup::s_one->m_map[_name];
  //   delete sink;
  //   return *(sink = new std::ofstream( path.c_str() ));
  // }

  // std::ostream&
  // Log::filechan( char const* _name, char const* _filename )
  // {
  //   std::string path;
  //   if (UWT::startswith( "/", _filename )) path = _filename;
  //   else                                   path = ChannelGroup::s_one->m_dir + '/' + _filename;
  //   std::ostream*& sink = ChannelGroup::s_one->m_map[_name];
  //   delete sink;
  //   return *(sink = new std::ofstream( path.c_str() ));
  // }

  // void
  // Log::delchan( char const* _name )
  // {
  //   std::map<std::string,std::ostream*>::iterator node = ChannelGroup::s_one->m_map.find( _name );
  //   if (node == ChannelGroup::s_one->m_map.end()) return;
  //   delete node->second;
  //   ChannelGroup::s_one->m_map.erase( node );
  // }

  // void
  // Log::trashchan( char const* _name )
  // {
  //   std::ostream*& sink = ChannelGroup::s_one->m_map[_name];
  //   delete sink;
  //   sink = new std::ostream( 0 );
  // }
};
