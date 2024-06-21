/***************************************************************************
                               uwt/iterbox.cc
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

/** @file uwt/iterbox.cc
    @brief  implementation file for the iteration/bootstrap mechanism

*/

#include <uwt/iterbox.hh>
#include <uwt/launchbox.hh>
#include <uwt/win/winnt.hh>
#include <uwt/utils/logchannels.hh>
#include <uwt/utils/misc.hh>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <cerrno>
#include <cassert>
#include <unistd.h>

extern char** environ;

namespace UWT
{
  IterBox::IterBox( MainModule* _mainmodule )
    : winos( _mainmodule ), step( 0 ), nextsecond( time( 0 ) + 1 )
  {}

  int
  IterBox::run()
  {
    // Logging system setup
    //    Log::ChannelGroup changrp( strx::abspath( "logs").c_str() );

    // Pure environment variables setup
    for (char **envp = environ; *envp; ++envp)
      if (char const* arg = UWT::popfirst("UWT_", *envp))
        configure( arg, true );

    // boostrapper binary name setup
    if (char const* appname_cstr = this->getenv("APPNAME"))
      {
        if (not startswith("./app", appname_cstr))
          {
            std::cerr << "Internal error: APPNAME argument corrupted (" << appname_cstr << ").\n";
            return 1;
          }

        std::string appname( appname_cstr );
        if (appname[5] != '\0')
          appname[5] ^= '\x01';


        this->setenv( "APPNAME", appname.c_str() );
      }
    else
      {
        std::cerr << "Internal error: no APPNAME argument.\n";
        return 1;
      }

    if (char const* step_s = this->getenv( "STEP" ))
      {
        step = atoi( step_s );
        std::cerr << "\e[4mIteration " << step << "\e[m" << std::endl;
      }
    else
      {
        std::cerr << "Internal error: no STEP argument\n";
        return 1;
      }

    // WinTel state initialization with images module
    try
      {
        // Useless to log loader info more than once
        if (step)
          winos.close_loader_log();

        winos.load();
      }
    catch( ... )
      {
        std::cerr << "Error: loading crashed, aborting now...\n";
        return 1;
      }

    // MAKE programs sometimes have low time resolution (doesn't go under the second)
    waitnewtimestamp();

    try
      {
        // WinTel transmulation
        if (not winos.execute(*this) and not step_once)
          restart();
      }
    catch (...)
      {
        std::cerr << "Error: Transmulation crashed, aborting now...\n";
        return 1;
      }

    return 0;
  }

  void
  IterBox::restart()
  {
    // Increment step
    this->setenv( "STEP", strx::fmt( "%d", step + 1 ).c_str() );

    char const* appname = this->getenv( "APPNAME" );
    char const* args[] = {"[UnWinTel: strap]", 0};

    execvp( appname, (char* const*)( args ) );
    std::cerr << strerror( errno ) << std::endl;
    assert( not "possible" );
  }

  void
  IterBox::waitnewtimestamp()
  {
    timespec tenmili = { 0, 10000000 };

    while( time( 0 ) < nextsecond ) nanosleep( &tenmili, 0 );
  }

  IterBox&
  IterBox::import( Module* (*_import) ( void ) )
  {
    winos.modules().push_back( _import() );
    return *this;
  }
};
