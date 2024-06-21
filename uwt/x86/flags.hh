/***************************************************************************
                              uwt/x86/flags.hh
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

/** @file uwt/x86/flags.hh
    @brief  header file for flags manip

*/

#ifndef UWT_FLAGS_HH
#define UWT_FLAGS_HH

#include <unisim/util/symbolic/symbolic.hh>
#include <inttypes.h>

#include <iosfwd>

namespace UWT
{
  struct FLAG
    : public unisim::util::identifier::Identifier<FLAG>
  {
    enum Code { CF = 0, PF, AF, ZF, SF, DF, OF, C0, C1, C2, C3, end } code;

    char const* c_str() const
    {
      switch (code)
        {
        case CF: return "cf";
        case PF: return "pf";
        case AF: return "af";
        case ZF: return "zf";
        case SF: return "sf";
        case DF: return "df";
        case OF: return "of";
        case C0: return "c0";
        case C1: return "c1";
        case C2: return "c2";
        case C3: return "c3";
        case end: break;
        }
      return "NA";
    }

    FLAG() : code(end) {}
    FLAG( Code _code ) : code(_code) {}
    FLAG( char const* _code ) : code(end) { init(_code); }
  };

  struct Flags
  {
    Flags();
    Flags( FLAG::Code flag );
    Flags( FLAG flag );

    void                  clear();
    Flags&                operator = ( Flags const& flags );

    Flags                 operator & ( Flags const& flags ) const;
    Flags                 operator | ( Flags const& flags ) const;
    Flags                 operator ~ () const;

    Flags&                operator |= ( Flags const& flags );

    bool                  operator != ( Flags const& flags ) const;

    void                  dump( std::ostream& ) const;
    bool                  changeto( Flags const& );
    bool                  notnull() const;
    bool                  contains( FLAG _flag ) const;

    enum { count = FLAG::end };

    struct repr
    {
      repr( Flags const& _flags ) : flags(_flags) {} Flags const& flags;
      friend std::ostream& operator << (std::ostream& sink, repr const& fr);
    };

  private:
    uint32_t              bits[((count+31)/32)];
  };

  Flags operator | ( FLAG::Code, FLAG::Code );

};

#endif // UWT_FLAGS_HH
