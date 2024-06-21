/***************************************************************************
                               uwt/iterbox.hh
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

/** @file uwt/iterbox.hh
    @brief  header file for the iteration/bootstrap mechanism

*/

#ifndef UWT_ITERBOX_HH
#define UWT_ITERBOX_HH

#include <ctime>

#include <uwt/win/winnt.hh>
#include <uwt/launchbox.hh>

#include <uwt/fwd.hh>
#include <iosfwd>

namespace UWT
{
  struct IterBox : public LaunchBox
  {
    IterBox( MainModule* _mainmodule );
    int                   run();

    IterBox&              import( Module* (*_import) ( void ) );

    void                  restart();
    WinNT&                os() { return winos; }

  private:
    WinNT                 winos;
    int                   step;
    time_t                nextsecond;

    void                  waitnewtimestamp();
  };

}

#endif // UWT_ITERBOX_HH
