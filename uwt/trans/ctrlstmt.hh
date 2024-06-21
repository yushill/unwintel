/***************************************************************************
                           uwt/trans/ctrlstmt.hh
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

/** @file uwt/trans/ctrlstmt.hh
    @brief  header file for control flow translation objects

*/

#ifndef UWT_CTRLSTMT_HH
#define UWT_CTRLSTMT_HH

#include <uwt/trans/statement.hh>
#include <inttypes.h>

namespace UWT
{
namespace ctrl
{
  struct Identifier
  {
    Identifier() : id( ++last_id ) {} uintptr_t id;
    static uintptr_t last_id;
  };

  Statement newLabelStatement( Identifier const& label );
  Statement newGotoStatement( Identifier const& label );

  Statement newEndIfStatement();
  Statement newDoStatement();
  Statement newEndDoAlwaysStatement();
  Statement newElseStatement();
  Statement newSingleStatement( char const* code );

  Statement newIfStatement( bool natural, char const* term, char const* );
  Statement newIfGotoStatement( bool natural, char const* term, Identifier const& label );
  Statement newIfThenStatement( bool natural, char const* term );
}
};

#endif // UWT_CTRLSTMT_HH
