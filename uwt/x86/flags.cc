/***************************************************************************
                              uwt/x86/flags.cc
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

/** @file uwt/x86/flags.cc
    @brief  implementation file for flags manip

*/

#include <uwt/x86/flags.hh>
#include <ostream>
#include <cassert>

namespace UWT
{

  Flags::Flags()
    : bits()
  {}

  Flags::Flags( FLAG flag )
    : bits()
  {
    int idx = flag.code;
    bits[idx/32] |= (1ul<<(idx%32));
  }

  Flags::Flags( FLAG::Code flag )
    : bits()
  {
    int idx = flag;
    bits[idx/32] |= (1ul<<(idx%32));
  }

  Flags
  Flags::operator & ( Flags const& flags ) const
  {
    Flags ret;

    for (int idx = ((count+31)/32); --idx >= 0;)
      ret.bits[idx] = bits[idx] & flags.bits[idx];
    return ret;
  }

  Flags&
  Flags::operator |= ( Flags const& flags )
  {
    for (int idx = ((count+31)/32); --idx >= 0;)
      bits[idx] |= flags.bits[idx];

    return *this;
  }

  Flags&
  Flags::operator = ( Flags const& flags )
  {
    for (int idx = ((count+31)/32); --idx >= 0;)
      bits[idx] = flags.bits[idx];
    return *this;
  }

  Flags
  Flags::operator ~ () const
  {
    Flags ret;
    for (int idx = ((count+31)/32); --idx >= 0;)
      ret.bits[idx] = ~(bits[idx]);
    return ret;
  }

  bool
  Flags::notnull() const
  {
    for (int idx = ((count+31)/32); --idx >= 0;)
      if (bits[idx] != 0) return true;
    return false;
  }

  bool
  Flags::contains( FLAG flag ) const
  {
    return (bits[flag.idx()/32] & (1ul<<(flag.idx()%32))) != 0;
  }

  bool
  Flags::operator != ( Flags const& flags ) const
  {
    for (int idx = ((count+31)/32); --idx >= 0;)
      if (bits[idx] != flags.bits[idx])
        return true;
    return false;
  }

  bool
  Flags::changeto( Flags const& flags )
  {
    bool changed = false;
    for (int idx = ((count+31)/32); --idx >= 0;)
      {
        if (bits[idx] == flags.bits[idx])
          continue;
        bits[idx] = flags.bits[idx];
        changed = true;
      }
    return changed;
  }

  void
  Flags::dump( std::ostream& _out ) const
  {
    for (int idx = 0; idx < count; idx++)
      _out << ((bits[idx/32]>>(idx%32))&0x01);
    _out << '\n';
  }

  std::ostream& operator << (std::ostream& sink, Flags::repr const& fr)
  {
    sink << "{";
    char const* sep = "";
    for (FLAG flag; flag.next();)
      {
        if (not fr.flags.contains( flag ))
          continue;
        sink << sep << flag.c_str();
        sep = ",";
      }
    sink << "}";
    return sink;
  }

  Flags
  operator | ( FLAG::Code f1, FLAG::Code f2 )
  {
    return Flags( f1 ) | Flags( f2 );
  }

  Flags
  Flags::operator | ( Flags const& flags ) const
  {
    Flags ret(*this);

    for (int idx = ((count+31)/32); --idx >= 0;)
      ret.bits[idx] |= flags.bits[idx];

    return ret;
  }
};
