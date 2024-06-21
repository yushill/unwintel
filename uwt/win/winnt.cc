/***************************************************************************
                              uwt/win/winnt.cc
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

/** @file uwt/win/winnt.cc
    @brief  impl. winNT emulation

*/

#include <uwt/win/winnt.hh>
#include <uwt/win/module.hh>
#include <uwt/win/filesystem.hh>
#include <uwt/x86/context.hh>
#include <uwt/launchbox.hh>
#include <uwt/utils/misc.hh>
#include <uwt/utils/logchannels.hh>
#include <unisim/util/dbgate/client/client.hh>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cstdio>
#include <cassert>

namespace UWT
{
  WinNT::WinNT( MainModule* _mainmodule )
    : m_mtc( 0 ), m_rrctxid( 0 ), m_memmgr( Memory::Page::s_size )
    , m_mainmodule( *_mainmodule ), m_psv( _mainmodule->m_imagepath )
    , m_major( 5 ), m_minor( 1 ), m_build( 2600 )
    , m_loader_log("loader.log")
  {
    m_modules.push_back( _mainmodule );
  }

  WinNT::~WinNT()
  {
    for (Module* mp : m_modules)
      delete mp;
    for (Context* tp : m_threads)
      delete tp;
  }

  //  void WinNT::close_loader_log() { dbgate_close(m_loader_log); m_loader_log = -1; }

  WinNT&
  WinNT::load()
  {
    // Loading main module
    assert( not m_mainmodule.isloaded() );
    m_mainmodule.loaded( true );

    m_mainmodule.init( *this );
    m_mainmodule.hostcodemap( m_calldb );

    // Main thread initialisation depending on extern intialization of
    // m_mtc (In an OS thread library for example).  TODO: check if
    // that's the better way.
    assert( m_mtc );
    Context* mainthread = (*m_mtc)( m_mem, m_calldb, *this );
    m_threads.push_back( mainthread );

    return *this;
  }

  bool
  WinNT::execute(UWT::IterBox& ibox)
  {
    do {
      m_rrctxid = (m_rrctxid + 1) % m_threads.size();

      Context::State ctxstate = m_threads[m_rrctxid]->cont();

      switch (ctxstate)
        {
        case Context::Running:
          break;

        case Context::CallException:
          trans( m_threads[m_rrctxid]->EIP, ibox ); // Entering source code generation
          return false;

        case Context::Exited:
          // Context exited normally: remove the context
          //delete m_threads[m_rrctxid];
          std::cerr << "internal error: unimplemented thread exit, bailing out.\n";
          throw std::runtime_error("not implemented");
          break;

        default:
          throw std::logic_error("internal error");
        }

    } while( m_threads.size() > 0 );
    return true; // All contexts exited normally
  }

  void
  WinNT::trans( uint32_t _addr, UWT::IterBox& ibox )
  {
    std::cerr << strx::fmt( "Translating from address %#x", _addr ) << std::endl;
    Module& transmodule = m_memmgr.module_at( _addr );

    transmodule.transmulate( _addr, m_mem );

    transmodule.make( ibox );
    if (transmodule.getname() == m_mainmodule.getname()) return;
    m_mainmodule.make( ibox );
  }

  Module&
  WinNT::module( char const* _name )
  {
    std::string normbuf( _name );
    for( std::string::iterator ptr = normbuf.begin(); ptr != normbuf.end(); ++ptr ) {
      char ch = *ptr;
      if( ch >= 'a' and ch <= 'z' )
        *ptr += 'A' - 'a';
      else if( (ch < 'A' or ch > 'Z') and (ch < '0' or ch > '9') )
        *ptr = '_';
    }
    std::string const normname( normbuf.c_str() );
    for (uintptr_t idx = 0; idx < m_modules.size(); ++ idx)
      if (m_modules[idx]->getname() == normname)
        return *(m_modules[idx]);
    std::cerr << strx::fmt( "No module named '%s'", normname.c_str() ) << std::endl;
    throw NotFound;
  }

  Module&
  WinNT::loadmodule( char const* _name )
  {
    Module& mod = module( _name );
    if( not mod.isloaded() ) {
      mod.loaded( true );
      mod.init( *this );
      mod.hostcodemap( m_calldb );
    }
    return mod;
  }

  void
  WinNT::addmodule( Range& _range, Module* _module )
  {
    m_memmgr.record( _range, _module );
  }

  void
  WinNT::stack( Range const& region, bool _dir, uint32_t _size, Module* _module )
  {
    m_stack = region;
    m_memmgr.allocate( m_stack, _dir, _size, _module );
  }

  void
  WinNT::heap( Range const& region, bool _dir, uint32_t _size, Module* _module )
  {
    m_heap = region;
    m_memmgr.allocate( m_heap, _dir, _size, _module );
  }

  WinNT::ProcessStartupVars::ProcessStartupVars( std::string const& _imagepath )
  {
    // default startup conditions
    std::string buffer;
    fsconvert_x2w( buffer, _imagepath.c_str() );
    m_args.push_back( buffer.c_str() );
    m_envs.push_back( "HOSTTYPE=unwintel" );
    m_envs.push_back( "NUMBER_OF_PROCESSORS=1" );
    m_envs.push_back( "OS=WINDOWS_NT" );
    m_envs.push_back( "PROCESSOR_ARCHITECTURE=x86" );
    m_envs.push_back( "PROCESSOR_IDENTIFIER=x86 Family 6 Model 2 Stepping 9, GenuineIntel" );
    m_envs.push_back( "PROCESSOR_LEVEL=6" );
    m_envs.push_back( "PROCESSOR_REVISION=0209" );
    m_envs.push_back( strx::fmt( "USERNAME=%s", getenv( "USER" ) ) );

    /* FIXME: some common variables set in win environment
     * + ComSpec=c:\windows\system32\cmd.exe
     * + PATH=c:\windows\system32;c:\windows
     * + ProgramFiles=c:\Program Files
     * + SystemDrive=c:
     * + SYSTEMROOT=c:\windows
     * + TEMP=c:\windows\temp
     * + TMP=c:\windows\temp
     * + USERPROFILE=c:\windows\profiles\lhuillie
     * + windir=c:\windows
     * + winsysdir=c:\windows\system32 */
  }

  WinNT::ProcessStartupVars::~ProcessStartupVars() {}
};
