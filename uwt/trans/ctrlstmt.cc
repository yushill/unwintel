/***************************************************************************
                           uwt/trans/ctrlstmt.cc
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

/** @file uwt/trans/ctrlstmt.cc
    @brief  implementation file for control flow translation objects

*/

#include <uwt/trans/ctrlstmt.hh>
#include <unisim/util/symbolic/ccode/ccode.hh>
#include <cassert>
#include <cstdio>
#include <stdexcept>

namespace UWT {
namespace ctrl {

  uintptr_t Identifier::last_id = 0;

  struct LabelString
  {
    LabelString( uint32_t id )
    {
      unsigned size = snprintf( buffer, sizeof (buffer), "TLabel%u", id );
      assert( size < sizeof (buffer) );
    }

    operator                  char const*() const { return buffer; }

    char                      buffer[16];
  };


  Statement newGotoStatement( Identifier const& _label )
  {
    struct _ : public BaseStatement
    {
      _( unsigned _label ) : label(_label) {} unsigned label;
      virtual void translate( unisim::util::symbolic::ccode::SrcMgr& sink ) const override { sink << "goto " << LabelString( label ) << ";\n"; }
    };
    return new _(_label.id);
  }

  Statement newLabelStatement( Identifier const& _label )
  {
    struct _ : public BaseStatement
    {
      _( unsigned _label ) : label(_label) {} unsigned label;
      virtual void translate( unisim::util::symbolic::ccode::SrcMgr& sink ) const override { sink << LabelString( label ) << ":\n"; }
    };
    return new _(_label.id);
  }

  Statement newEndIfStatement()
  {
    struct _ : public BaseStatement
    { virtual void translate( unisim::util::symbolic::ccode::SrcMgr& sink ) const override { sink << "}\n"; } };
    return new _;
  }

  Statement newDoStatement()
  {
    struct _ : public BaseStatement
    { virtual void translate( unisim::util::symbolic::ccode::SrcMgr& sink ) const override { sink << "do {\n"; } };
    return new _;
  }

  Statement newEndDoAlwaysStatement()
  {
    struct _ : public BaseStatement
    { virtual void translate( unisim::util::symbolic::ccode::SrcMgr& sink ) const override { sink << "} while( true );\n"; } };
    return new _;
  }

  Statement newElseStatement()
  {
    struct _ : public BaseStatement
    { virtual void translate( unisim::util::symbolic::ccode::SrcMgr& sink ) const override { sink << "} else {\n"; } };
    return new _;
  }

  Statement newSingleStatement( char const* code )
  {
    struct _ : public BaseStatement
    {
      virtual void translate( unisim::util::symbolic::ccode::SrcMgr& sink ) const override { sink << code.c_str(); }
      _( char const* _code ) : code(_code) {} std::string code;
    };
    return new _( code );
  }

  struct CondStatement : public BaseStatement
  {
    virtual void translate( unisim::util::symbolic::ccode::SrcMgr& sink ) const override
    {
      if (not term)
        throw std::logic_error("instruction has a conditional control exit but no control condition\n");

      before( sink );

      if (not natural) sink << "not (";

      sink << term;

      if (not natural) sink << ")";

      after( sink );
    }
    void GetVariables( unisim::util::symbolic::ccode::Variables& vars ) const override { vars.insert( term ); }

    virtual void before( unisim::util::symbolic::ccode::SrcMgr& sink ) const = 0;
    virtual void after( unisim::util::symbolic::ccode::SrcMgr& sink ) const = 0;

    CondStatement( bool _natural, char const* _term ) : natural(_natural), term(_term) {} bool natural; char const* term;
  };

  Statement newIfStatement( bool natural, char const* term, char const* code )
  {
    struct _ : public CondStatement
    {
      void before( unisim::util::symbolic::ccode::SrcMgr& sink ) const { sink << "if( "; }
      void  after( unisim::util::symbolic::ccode::SrcMgr& sink ) const { sink << " ) " << code.c_str(); }
      _(bool _natural, char const* _term, char const* _code) : CondStatement(_natural, _term), code(_code) {} std::string code;
    };
    return new _(natural, term, code);
  }


  Statement newIfGotoStatement( bool natural, char const* term, Identifier const& label )
  {
    struct _ : public CondStatement
    {
      void before( unisim::util::symbolic::ccode::SrcMgr& sink ) const { sink << "if( "; }
      void  after( unisim::util::symbolic::ccode::SrcMgr& sink ) const { sink << " ) go""to " << LabelString( label ) << ";\n"; }
      _(bool _natural, char const* _term, unsigned _label) : CondStatement(_natural, _term), label(_label) {} unsigned label;
    };
    return new _(natural, term, label.id);
  }

  Statement newIfThenStatement( bool natural, char const* term )
  {
    struct _ : public CondStatement
    {
      void before( unisim::util::symbolic::ccode::SrcMgr& sink ) const { sink << "if( "; }
      void  after( unisim::util::symbolic::ccode::SrcMgr& sink ) const { sink << " ) {\n"; }
      _(bool _natural, char const* _term) : CondStatement(_natural, _term) {}
    };
    return new _(natural, term);
  }


} /* end of namespace ctrl */
} /* end of namespace UWT */

