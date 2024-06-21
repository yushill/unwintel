/***************************************************************************
                               uwt/launchbox.hh
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

/** @file uwt/launchbox.hh
    @brief header file for top launching module
*/

#ifndef UWT_LAUNCHBOX_HH
#define UWT_LAUNCHBOX_HH

// #include <vector>
#include <string>
// #include <iosfwd>
// #include <inttypes.h>

namespace UWT
{
  struct LaunchBox
  {
    LaunchBox();

    struct Abort { Abort( int _code ) : code(_code) {} int code; };

    void configure( std::string const& arg, bool with_internal = false );

    static void        setenv( char const* k, char const* v );
    static char const* getenv( char const* k );
    static char const* default_app_name();

    std::string                 framework;
    bool                        quiet_make, step_once;

  protected:
    struct Switch
    {
      struct Args { virtual bool match( Switch& ) = 0; virtual ~Args() {} };

      virtual char const* name() const = 0;
      virtual void describe( std::ostream& sink ) const = 0;
      virtual ~Switch() {}
    };

    struct Parameter : public Switch
    {
      virtual void normalize( LaunchBox& uwt, std::string& value ) const {};
      static  bool yesno( std::string& value );
    };

    void parameters( LaunchBox::Switch::Args& args );
    void internal_parameters( LaunchBox::Switch::Args& args );
  };
}

#endif // UWT_LAUNCHBOX_HH
