/***************************************************************************
                          uwt/trans/insnstmt.hh
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

/** @file uwt/trans/insnstmt.hh
    @brief  header file for Sole Instruction Statements

*/

#ifndef UWT_INSNSTMT_HH
#define UWT_INSNSTMT_HH

#include <uwt/trans/statement.hh>
#include <set>
#include <inttypes.h>

namespace unisim {
  namespace component { namespace cxx { namespace processor { namespace intel {
          template <typename CORE> struct Operation;
  } } } }

  namespace util { namespace symbolic { namespace ccode {
    struct ActionNode;
    struct SrcMgr;
    struct CCode;
    struct Variable;
  } } }
}

namespace UWT
{
  struct Core;

  struct InsnStatement : public BaseStatement
  {
    typedef unisim::util::symbolic::ccode::ActionNode ActionNode;
    typedef unisim::util::symbolic::ccode::SrcMgr     SrcMgr;
    typedef unisim::util::symbolic::ccode::CCode      CCode;
    typedef unisim::util::symbolic::ccode::Variable   Variable;
    typedef unisim::util::symbolic::Expr       Expr;
    typedef unisim::util::symbolic::ExprNode   ExprNode;

    struct Branch
    {
      Expr cond, addr;
      enum mode_t { jump, call, ret } mode;
      Branch() : cond(), addr(), mode(jump) {}
      void Repr( std::ostream& ) const;
    };

    InsnStatement( uint32_t addr, uint8_t* codebuf, unsigned bufsize );
    ~InsnStatement();

    void              translate( unisim::util::symbolic::ccode::SrcMgr& sink ) const override;
    void              GetFlagStats( Flags& in_flags, Flags& out_flags ) const override;
    void              SetNeededFlags( Flags const& flags ) override;
    void              SetCondTerm(char const* term, bool inv) override { cvar = term; cinv = inv; }

    uint32_t          GetAddr() const;
    uint32_t          GetLength() const;
    Branch const&     GetBranch() const { return branch; }
    void              Repr( std::ostream& ) const;

    typedef unisim::component::cxx::processor::intel::Operation<Core> Operation;

  private:
    Operation*          op;
    ActionNode*         actions;
    Branch              branch;
    char const*         cvar;
    bool                cinv;
    Flags               needed_flags;

    friend Core;
  };

};

#endif // UWT_INSNSTMT_HH
