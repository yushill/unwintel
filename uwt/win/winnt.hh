/***************************************************************************
                              uwt/win/winnt.hh
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

/** @file uwt/win/winnt.hh
    @brief  decl. winNT emulation

*/

#ifndef UWT_WINNT_HH
#define UWT_WINNT_HH

#include <uwt/win/memmgr.hh>
#include <uwt/x86/memory.hh>
#include <uwt/x86/calldb.hh>
#include <uwt/utils/logchannels.hh>
#include <uwt/fwd.hh>
#include <vector>
#include <string>
#include <iosfwd>
#include <inttypes.h>

namespace UWT {
  typedef Context* (*main_thread_constructor_t)( Memory& _memory, CallDB& _calldb, WinNT& _os );

  struct WinNT
  {
    struct ProcessStartupVars
    {
      std::vector<std::string>  m_args;
      std::vector<std::string>  m_envs;

      ProcessStartupVars( std::string const& _imagepath );
      ~ProcessStartupVars();
    };

    enum exception_t { NotFound, EmulationCrash };

    WinNT( MainModule* _mainmodule );
    ~WinNT();

    WinNT&                        load();
    Module&                       loadmodule( char const* _name );

    Memory&                       mem() { return m_mem; }
    MemMgr&                       memmgr() { return m_memmgr; }
    std::vector<Module*>&         modules() { return m_modules; }
    Module&                       module( char const* _name );
    MainModule&                   mainmodule() { return m_mainmodule; };
    void                          setmtc( main_thread_constructor_t _mtc ) { m_mtc = _mtc; }
    void                          addmodule( Range& _range, Module* _module );
    void                          stack( Range const& _range, bool _dir, uint32_t _size, Module* _module );
    Range&                        stack() { return m_stack; }
    void                          heap( Range const& _range, bool _dir, uint32_t _size, Module* _module );
    Range&                        heap() { return m_heap; }
    ProcessStartupVars&           psv() { return m_psv; };

    bool                          execute( UWT::IterBox& );
    void                          trans( uint32_t _addr, UWT::IterBox& );
    DebugStream&                  loader_log() { return m_loader_log; }
    void                          close_loader_log() { m_loader_log.close(); }

  private:
    Memory                        m_mem;
    CallDB                        m_calldb;
    std::vector<Context*>         m_threads;
    main_thread_constructor_t     m_mtc;
    intptr_t                      m_rrctxid;
    MemMgr                        m_memmgr;
    MainModule&                   m_mainmodule;
    std::vector<Module*>          m_modules;
    Range                         m_stack;
    Range                         m_heap;
    ProcessStartupVars            m_psv;
    uint8_t                       m_major, m_minor;
    uint16_t                      m_build;
    DebugStream                   m_loader_log;
  };
};

#endif // UWT_WINNT_HH
