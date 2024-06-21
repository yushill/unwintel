/***************************************************************************
                             uwt/trans/ibloc.cc
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

/** @file uwt/trans/ibloc.cc
    @brief  implementation file for instruction blocs

*/

#include <uwt/trans/ibloc.hh>
#include <uwt/trans/functionlab.hh>
#include <uwt/trans/statement.hh>
#include <uwt/trans/insnstmt.hh>
#include <uwt/utils/misc.hh>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cassert>

namespace UWT
{
  IBloc::IBloc( Statement const& inst )
    : statements(), exit_blocs(), ipar(0), idom(0), ordering(unsigned(-1)), prevcount(0)
  {
    statements.push_back( inst );
  }

  IBloc*
  IBloc::duplicate()
  {
    IBloc* res = new IBloc( statements );
    res->set_exits( exit_blocs );
    return res;
  }

  IBloc::~IBloc() noexcept(false)
  {
    if (prevcount) throw std::logic_error("CFG leak");
    unref( exit_blocs.get(0) );
    unref( exit_blocs.get(1) );
  }

  void
  IBloc::set_exits( Exits const& exits )
  {
    ref( exits.get(0) );
    ref( exits.get(1) );
    unref( exit_blocs.get(0) );
    unref( exit_blocs.get(1) );
    exit_blocs = exits;
  }

  void
  IBloc::set_exit( bool alt, IBloc* bloc )
  {
    ref( bloc );
    unref( exit_blocs.get(alt) );
    exit_blocs.set(alt, bloc);
  }

  IBloc&
  IBloc::append_stmt( Statement const& inst )
  {
    statements.push_back( inst );

    return *this;
  }

  IBloc&
  IBloc::append( IBloc const& ibloc )
  {
    statements.insert( statements.end(), ibloc.statements.begin(), ibloc.statements.end() );

    return *this;
  }

  IBloc&
  IBloc::insert_stmt( Statement const& inst )
  {
    statements.insert( statements.begin(), inst );

    return *this;
  }

  bool
  IBloc::Exits::which( IBloc* bloc ) const
  {
    if (bloc == odd)
      return true;
    if (bloc != nat)
      throw std::out_of_range("no such next ibloc*");
    return false;
  }

  GraphMatch::GraphMatch( char const* _title, char const* pattern, IBloc* first )
    : blocs(), exits(), title(_title)
  {
    test( pattern, first, 0, 0 );
  }

  bool
  GraphMatch::set_exit( bool alt, IBloc* bloc )
  {
    if (IBloc* existing = exits.get(alt))
      { return existing == bloc; }

    exits.set( alt, bloc );
    return true;
  }

  bool
  GraphMatch::test( char const* pattern, IBloc* next_bloc, Link const* matched, Link const* back_blocs )
  {
    char cc = *pattern++;

    if (matched) {
      switch (cc) {
        // Middle nodes are uniquely entered and unique (isn't that the same ?)
      case '?': case ',': case '.': case '!':
        if ((next_bloc->count_predecessors() > 1) or matched->has( next_bloc ))
          return false;
        break;
      }
    }

    switch (cc)
      {
      case '?':
        // Conditionnal branch node
        if (not next_bloc->get_exit(1))
          return false;

        for (int reversed = int(false); reversed <= int(true); ++reversed)
          {
            Link next_matched(next_bloc, matched);
            Link next_back_blocs(next_bloc->get_exit(not reversed), back_blocs);
            IBloc::Exits saved_exits( exits );
            if (test(pattern, next_bloc->get_exit(reversed), &next_matched, &next_back_blocs))
              return true;
            exits = saved_exits;
          }

        return false;

      case '*':
        // Whole Bloc Catcher

        if ((not matched or (next_bloc->count_predecessors() == 1)) and next_bloc->get_exit(0))
          {
            // Any middle node
            Link next_matched(next_bloc, matched);

            IBloc::Exits saved_exits( exits );

            if (next_bloc->get_exit(1))
              {
                // A fork
                Link next_back_blocs(next_bloc->get_exit(1), back_blocs);
                if (test("*", next_bloc->get_exit(0), &next_matched, &next_back_blocs))
                  return true;
              }
            else
              {
                // A cont
                if (test("*", next_bloc->get_exit(0), &next_matched, back_blocs))
                  return true;
              }

            exits = saved_exits;
          }

        if (not matched)
          return false;

        // Any exit node
        if (set_exit( false, next_bloc ) or set_exit( true, next_bloc ))
          return close( back_blocs ? "*" : "", matched, back_blocs );

        return false;

      case ',':
        // Sequential operation
        if (next_bloc->get_exit( 1 ) or not next_bloc->get_exit( 0 ))
          return false;

        {
          Link next_matched(next_bloc, matched);
          return test(pattern, next_bloc->get_exit(0), &next_matched, back_blocs);
        }
      case '.':
        // End node
        if (next_bloc->get_exit( 1 ) or next_bloc->get_exit( 0 ))
          return false;

        {
          Link next_matched(next_bloc, matched);
          return close( pattern, &next_matched, back_blocs );
        }

      case '!':
        // Any closing node
        if (not set_exit( 0, next_bloc->get_exit(0) ) or not set_exit( 1, next_bloc->get_exit(1) ))
          throw std::logic_error("closing node");

        {
          Link next_matched(next_bloc, matched);
          return close( pattern, &next_matched, back_blocs );
        }

      case '1': case '2':
        // Exit node
        if (not set_exit( cc == '2', next_bloc ))
          return false;

        return close( pattern, matched, back_blocs );

      case '0':
        // Opening node
        if (not matched or (matched->first() != next_bloc))
          return false;

        return close( pattern, matched, back_blocs );

      case '(':
        // Begin List Item
        {
          IBloc::Exits saved_exits( exits );
          if (test( pattern, next_bloc, matched, back_blocs ))
            return true;
          exits = saved_exits;
        }

        for (int depth = 1; depth > 0; )
          {
            if (*pattern == '(') depth++;
            if (*pattern == ')') depth--;
            pattern++;
          }
        return test( pattern, next_bloc, matched, back_blocs );

      case ')':
        // End List Item
        --pattern; /* reposition */
        for (int depth = 1; depth > 0; )
          {
            --pattern;
            if (*pattern == '(') depth--;
            if (*pattern == ')') depth++;
          }

        return test( pattern, next_bloc, matched, back_blocs );
      }

    throw std::logic_error("Pattern Error");
  }

  bool
  GraphMatch::close( char const* pattern, Link const* matched, Link const* back_blocs )
  {
    // Reached a final node
    if (back_blocs)
      return test(pattern, back_blocs->bloc, matched, back_blocs->back );

    if (*pattern)
      return false;

    // Pattern is matched
    matched->fill(*this,0);

    return true;
  }

  long
  GraphMatch::Link::fill(GraphMatch& m, long idx) const
  {
    if (back)
      { idx = back->fill(m,idx+1); }
    else
      { m.blocs.resize(idx+1); idx = 0; }

    m.blocs[idx] = bloc;

    return idx+1;
  }

} // end of namespace UWT
