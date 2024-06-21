/***************************************************************************
                           uwt/trans/statement.hh
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

/** @file uwt/trans/statement.hh
 *  @brief  header file for generic statement objects
 */

#ifndef UWT_STATEMENT_HH
#define UWT_STATEMENT_HH

#include <uwt/x86/flags.hh>
#include <uwt/fwd.hh>
#include <map>
#include <set>
#include <inttypes.h>

namespace unisim {
namespace util {
namespace symbolic {
namespace ccode {
  struct Variables;
  struct SrcMgr;
} /* end of namespace unisim */
} /* end of namespace util */
} /* end of namespace symbolic */
} /* end of namespace ccode */

namespace UWT
{
  struct BaseStatement
  {

    BaseStatement() : refs(0) {}
    virtual ~BaseStatement() {};

    void Retain() { ++refs; }
    void Release() { if (--refs == 0) delete this; }

    virtual void                translate( unisim::util::symbolic::ccode::SrcMgr& _sink ) const = 0;
    virtual void                GetFlagStats( Flags& in_flags, Flags& out_flags ) const {}
    virtual void                SetNeededFlags( Flags const& _flags ) { throw 0; }
    virtual void                SetCondTerm(char const*,bool) { throw 0; };
    virtual void                GetVariables( unisim::util::symbolic::ccode::Variables& vars ) const {}

  private:
    unsigned refs;
  };

  struct Statement
  {
    Statement() : node() {} BaseStatement* node;
    Statement( Statement const& expr ) : node( expr.node ) { if (node) node->Retain(); }
    Statement( BaseStatement* const& _node ) : node( _node ) { if (node) node->Retain(); }
    Statement&  operator = ( Statement const& expr )
    {
      if (expr.node) expr.node->Retain();
      BaseStatement* old_node = node;
      node = expr.node;
      if (old_node) old_node->Release();
      return *this;
    }
    ~Statement() { if (node) node->Release(); }
    BaseStatement const* operator->() const { return node; }
    BaseStatement* operator->() { return node; }
  };
};

#endif // UWT_STATEMENT_HH

