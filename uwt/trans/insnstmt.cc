/***************************************************************************
                          uwt/trans/insnstmt.cc
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

/** @file uwt/trans/insnstmt.cc
    @brief  implementation file for translation engine

*/

#include <uwt/trans/insnstmt.hh>
#include <uwt/trans/ctrlstmt.hh>
#include <uwt/trans/core.hh>
// #include <uwt/utils/logchannels.hh>
#include <unisim/component/cxx/processor/intel/types.hh>
#include <unisim/component/cxx/processor/intel/isa/intel.tcc>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace UWT
{
  struct PrefixError
  {
    uint32_t m_start, m_offendingbyteoffset;
    PrefixError( uint32_t _start, uint32_t _offendingbyteoffset )
      : m_start( _start ), m_offendingbyteoffset( _offendingbyteoffset ) {}
  };

  InsnStatement::InsnStatement( uint32_t addr, uint8_t* codebuf, unsigned bufsize )
    : op(0)
    , actions(new ActionNode)
    , branch()
    , needed_flags()
  {
    {
      unisim::component::cxx::processor::intel::Mode mode( 0, 1, 1 );

      Core::InputCode ic( mode, codebuf, Core::OpHeader( addr ) );

      op = unisim::component::cxx::processor::intel::getoperation( ic );

      Expr insn_addr( unisim::util::symbolic::make_const( addr ) );

      Core reference( actions, insn_addr );

      for (bool end = false; not end;)
        {
          Core core( actions, insn_addr );
          core.step( op );
          end = core.close( reference );
        }

      actions->simplify();

      struct TargetsMiner
      {
        Branch branch;
        Expr naddr;
        TargetsMiner(Expr const& nia) : naddr(nia) {}

        void setmode(Core::ipproc_t hint)
        {
          switch (hint)
            {
            default: throw 0;
            case Core::ipjmp: branch.mode = branch.jump; break;
            case Core::ipcall: branch.mode = branch.call; break;
            case Core::ipret: branch.mode = branch.ret; break;
            }
        }

        void process( ActionNode* node, Expr cond )
        {
          for (auto itr = node->updates.begin(), end = node->updates.end(); itr != end; ++itr)
            {
              if (auto rw = dynamic_cast<Core::EIPWrite const*>( itr->node ))
                {
                  Expr addr( rw->value );
                  if ((addr != naddr) or (not cond.good()))
                    {
                      if (not branch.addr.good())
                        {
                          branch.addr = addr;
                          branch.cond = cond;
                          setmode(rw->hint);
                        }
                      else if (branch.addr == addr)
                        {
                          branch.cond = unisim::util::symbolic::make_operation( "Or", cond, branch.cond );
                        }
                      else
                        throw 0;
                    }
                  itr = node->updates.erase(itr);
                  return;
                }
            }

          using unisim::util::symbolic::make_operation;

          for (unsigned choice = 0; choice < 2;  ++choice)
            if (ActionNode* nxt = node->nexts[choice])
              {
                Expr nodecond  = choice ? node->cond : make_operation( "Not", node->cond );
                process( nxt, cond.good() ? make_operation( "And", nodecond, cond ) : nodecond );
              }
            else
              throw 0;
        }
      } tm( unisim::util::symbolic::make_const(GetAddr() + GetLength()) );

      tm.process( actions, Expr() );

      branch = tm.branch;

      struct Needed : public Core::RegWriteBase
      {
        typedef unisim::util::symbolic::ccode::SrcMgr SrcMgr;
        Needed( Expr const& what ) : RegWriteBase( what ) {}
        virtual Needed* Mutate() const override { return new Needed( *this ); }
        virtual void GetRegName(std::ostream& sink) const override { sink << "needed"; }
        virtual void translate( SrcMgr& srcmgr, CCode& ccode ) const override {}
        virtual int cmp( ExprNode const& rhs ) const override { return 0; }
      };

      if (branch.cond.good()) actions->updates.insert( new Needed( branch.cond ) );
      if (branch.addr.good()) actions->updates.insert( new Needed( branch.addr ) );

      actions->simplify();

    }
  }

  InsnStatement::~InsnStatement()
  {
    delete op;
    delete actions;
  }

  void
  InsnStatement::GetFlagStats( Flags& in_flags, Flags& out_flags ) const
  {
    struct FlagInspector
    {
      FlagInspector( Flags& _in_flags, Flags& _out_flags )
        : in_flags(_in_flags), out_flags(_out_flags)
      {}

      void Process( Expr const& expr )
      {
        for (int idx = expr->SubCount(); --idx >=0;)
          Process( expr->GetSub(idx) );
        if (auto fr = dynamic_cast<unisim::util::symbolic::ccode::RegRead<Core::FLAG> const*>( expr.node ))
          in_flags |= fr->id;
      }

      void get_flag_stats( ActionNode const* node )
      {
        for (auto && update : node->updates)
          {
            if (auto fw = dynamic_cast<unisim::util::symbolic::ccode::RegWrite<Core::FLAG> const*>( update.node ))
              out_flags |= fw->id;
            Process( update );
          }

        if (node->cond.good())
          Process( node->cond );

        for (unsigned choice = 0; choice < 2;  ++choice)
          if (ActionNode* nxt = node->nexts[choice])
            get_flag_stats( nxt );
      }

      Flags& in_flags;
      Flags& out_flags;
    } fi(in_flags, out_flags);

    fi.get_flag_stats( actions );
  }

  struct FlagSwitch
  {
    FlagSwitch() : flags(), deadcode(false) {}
    FlagSwitch( Flags const& _flags ) : flags( _flags ), deadcode( false ) {}
    Flags flags;
    bool deadcode;
    void  enter( FLAG flag ) { deadcode = not flags.contains( flag ); }
    void  leave() { deadcode = false; }
  };

  uint32_t  InsnStatement::GetAddr() const { return op->address; }
  uint32_t  InsnStatement::GetLength() const { return op->length; }

  void
  InsnStatement::translate( SrcMgr& sink ) const
  {

    sink << "/***/ EIP( " << unisim::util::symbolic::ccode::hex( GetAddr() ) << " ) /***/;\n";

    sink << "{\n";

    actions->simplify();

    actions->commit_stats();

    CCode ccode;

    struct Coder
    {
      Coder( SrcMgr& _srcmgr, CCode& _ccode ) : srcmgr(_srcmgr), ccode(_ccode) {}

      void generate(ActionNode* node)
      {
        // Ordering Sub Expressions by size of expressions (so that
        // smaller expressions are factorized in larger ones)
        struct CSE : public std::multimap<unsigned,Expr>
        {
          void Process( Expr const& expr ) { insert( std::make_pair( CountSubs( expr ), expr ) ); }
          unsigned CountSubs( Expr const& expr )
          {
            unsigned sum = 1;
            for (unsigned idx = 0, end = expr->SubCount(); idx < end; ++idx)
              sum += CountSubs( expr->GetSub(idx) );
            return sum;
          }
        } cse;

        for (std::map<Expr,unsigned>::const_iterator itr = node->sestats.begin(), end = node->sestats.end(); itr != end; ++itr)
          {
            // Check if reused and not already defined
            if ((itr->second > 1) and not ccode.tmps.count( itr->first ))
              cse.Process(itr->first);
          }

        for (std::multimap<unsigned,Expr>::const_iterator itr = cse.begin(), end = cse.end(); itr != end; ++itr)
          {
            ccode.make_temp( srcmgr, itr->second );
          }

        for (auto && update : node->updates)
          {
            Core::Update const& ud = dynamic_cast<Core::Update const&>( *update.node );

            for (unsigned idx = 0, end = ud.SubCount(); idx < end; ++idx)
              {
                Expr value = ud.GetSub(idx);

                if (value->AsConstNode() or ccode.tmps.count( value ))
                  continue;

                ccode.make_temp( srcmgr, value );
              }
          }

        if (node->cond.good())
          {
            std::map<Expr,Variable> saved_tmps( ccode.tmps );
            srcmgr << "if (" << ccode(node->cond) << ")\n";
            if (ActionNode* next = node->nexts[true])
              {
                srcmgr << "{\n";
                generate( next );
                srcmgr << "}\n";
                ccode.tmps = saved_tmps;
              }
            else
              throw 0;
            if (ActionNode* next = node->nexts[false])
              {
                srcmgr << "else\n{\n";
                generate( next );
                srcmgr << "}\n";
                ccode.tmps = saved_tmps;
              }
          }

        // Commit architectural updates
        for (auto && update : node->updates)
          {
            srcmgr << ccode(update);
          }
      }

      SrcMgr& srcmgr;
      CCode&   ccode;
    } coder(sink, ccode);

    coder.generate( actions );

    {
      /*** Handling instruction control flow ***/
      Expr addr(branch.addr), cond(branch.cond);
      Branch::mode_t mode(branch.mode);
      auto banode = dynamic_cast<unisim::util::symbolic::ConstNode<uint32_t> const*>( addr.node );

      if (cond.good())
        {
          if (not banode)
            throw 0;
          sink << cvar << " = " << (cinv ? "not " : "") << ccode(cond) << ";\n";
        }
      else if      (mode == Branch::call)
        sink << " if ((" << Core::ctxvar << ".EIP = "
             <<             Core::ctxvar << ".call( " << ccode(addr) << " )( " << Core::ctxvar << " )) != "
             << unisim::util::symbolic::ccode::hex(GetAddr() + GetLength()) << ") return " << Core::ctxvar << ".EIP;\n";
      else if (mode == Branch::ret)
        sink << "return " << ccode(addr) << ";\n";
      else if (not banode)
        sink << "return " << Core::ctxvar << ".call( " << ccode(addr) << " )( " << Core::ctxvar << ");\n";
    }

    sink << "}\n";
  }

  void
  InsnStatement::SetNeededFlags( Flags const& flags )
  {
    needed_flags = flags;

    struct FlagFilter
    {
      FlagFilter( Flags const& flags )
        : needed_flags(flags)
      {}

      void apply( ActionNode* node )
      {
        for (auto && itr = node->updates.begin(), && end = node->updates.end(); itr != end;)
          {
            if (auto fw = dynamic_cast<unisim::util::symbolic::ccode::RegWrite<Core::FLAG> const*>( itr->node ))
              if (not needed_flags.contains(fw->id))
                { itr = node->updates.erase(itr); continue; }
            ++ itr;
          }

        for (unsigned choice = 0; choice < 2; ++choice)
          if (ActionNode* nxt = node->nexts[choice]) apply( nxt );
      }

      Flags const& needed_flags;
    } ff(flags);

    ff.apply( actions );
  }

  void
  InsnStatement::Repr( std::ostream& sink ) const
  {
    sink << "InsnStatement\n{"
         << "\n  address: " << std::hex << op->address << "\n";

    actions->Repr( sink );
    branch.Repr( sink );

    sink << "}\n";
  }

  void
  InsnStatement::Branch::Repr( std::ostream& sink ) const
  {
    sink << "Branch\n{";
    if (cond.good()) cond->Repr( sink << "\ncond: " );
    if (addr.good()) addr->Repr( sink << "\naddr: " );
    switch (mode)
      {
      case jump: sink << "\nmode: jump"; break;
      case call: sink << "\nmode: call"; break;
      case ret:  sink << "\nmode: ret"; break;
      }
    sink << "\n}\n";
  }

} /* end of namespace UWT */
