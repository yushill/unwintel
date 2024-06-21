/***************************************************************************
                              uwt/launchbox.cc
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

#include <uwt/launchbox.hh>
#include <uwt/trans/functionlab.hh>
#include <uwt/utils/misc.hh>
// #include <uwt/utils/logchannels.hh>
#include <iostream>
// #include <fstream>
// #include <sys/stat.h>
// #include <sys/types.h>
// #include <unistd.h>

namespace UWT
{
  LaunchBox::LaunchBox()
    : framework()
    , quiet_make(true)
    , step_once(false)
  {
  }

  void
  LaunchBox::parameters( LaunchBox::Switch::Args& args )
  {
    {
      static struct : Parameter
      {
        char const* name() const { return "FRAMEWORK"; }
        void describe( std::ostream& sink ) const { sink << "Root directory of unwintel framework"; }
        void normalize( LaunchBox& u, std::string& value ) const { u.framework = value = strx::abspath( value.c_str() ); }
      } _;

      if (args.match(_)) return;
    }

    {
      static struct : Parameter
      {
        char const* name() const { return "QUIET_MAKE"; }
        void describe( std::ostream& sink ) const { sink << "Mute 'make' invocation (-s)"; }
        void normalize( LaunchBox& u, std::string& value ) const { u.quiet_make = yesno( value ); }
      } _;

      if (args.match(_)) return;
    }

    {
      static struct : Parameter
      {
        char const* name() const { return "STEP_ONCE"; }
        void describe( std::ostream& sink ) const { sink << "Switch for single iteration mode"; }
        void normalize( LaunchBox& u, std::string& value ) const { u.step_once = yesno( value ); }
      } _;

      if (args.match(_)) return;
    }
  }

  void
  LaunchBox::internal_parameters( LaunchBox::Switch::Args& args )
  {
    {
      static struct : LaunchBox::Parameter
      {
        char const* name() const { return "APPNAME"; }
        void describe( std::ostream& sink ) const { sink << "Name of the app to compile and run"; }
      } _; if (args.match(_)) return;
    }
    {
      static struct : LaunchBox::Parameter
      {
        char const* name() const { return "MODULE"; }
        void describe( std::ostream& sink ) const { sink << "Main module name for transmulation"; }
      } _; if (args.match(_)) return;
    }
    {
      static struct : LaunchBox::Parameter
      {
        char const* name() const { return "STEP"; }
        void describe( std::ostream& sink ) const { sink << "Recursive transmulation level"; }
      } _; if (args.match(_)) return;
    }
  }

  void
  LaunchBox::configure( std::string const& arg, bool with_internal )
  {
    uintptr_t assign = arg.find( '=' );
    if (assign != std::string::npos)
      {
        std::string key( arg.begin(), arg.begin() + assign );
        std::string val( arg.begin() + assign + 1, arg.end() );

        struct PA : Switch::Args
        {
          PA( LaunchBox& _u, std::string const& _k, std::string& _v ) : u(_u), k(_k), v(_v), found(false) {}
          virtual bool match( Switch& s )
          {
            if (k.compare(s.name()) != 0)
              return false;
            dynamic_cast<Parameter&>( s ).normalize( u, v );
            u.setenv( k.c_str(), v.c_str() );
            return (found = true);
          }
          LaunchBox& u; std::string const& k; std::string& v; bool found;
        } pa( *this, key, val );

        parameters( pa );
        if (pa.found)
          return;
        if (with_internal)
          internal_parameters( pa );
        if (pa.found)
          return;
      }

    std::cerr << "Unknown argument: " << arg << std::endl;
    throw Abort( 1 );
  }

  bool
  LaunchBox::Parameter::yesno( std::string& value )
  {
    uintptr_t start = value.find_first_not_of("\f\n\r\t\v ");
    char first = tolower( (start != std::string::npos) ? value[start] : '0' );
    if ((first == 'n') or (first == '0') or (first == 'f')) {
      value = "no";
      return false;
    }

    value = "yes";
    return true;
  }

  void
  LaunchBox::setenv( char const* k, char const* v )
  {
    std::string uwtkey( "UWT_" );
    uwtkey += k;
    ::setenv( uwtkey.c_str(), v, true );
  }

  char const*
  LaunchBox::getenv( char const* k )
  {
    std::string uwtkey( "UWT_" );
    uwtkey += k;
    return ::getenv( uwtkey.c_str() );
  }


  // void
  // LaunchBox::version()
  // {
  //   std::cerr << '\t' << m_displayname << " v0.2\n" << "built date : " __DATE__ "\n";
  // }

  char const*
  LaunchBox::default_app_name()
  {
#ifdef _WIN32
    return "./app0";
#else
    return "./app";
#endif
  }
}
