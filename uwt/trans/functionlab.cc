/***************************************************************************
                          uwt/trans/functionlab.cc
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

/** @file uwt/trans/functionlab.cc
    @brief  implementation file for translation engine

*/

#include <uwt/trans/functionlab.hh>
#include <uwt/trans/core.hh>
#include <uwt/trans/ibloc.hh>
#include <uwt/trans/ctrlstmt.hh>
#include <uwt/trans/insnstmt.hh>
#include <uwt/iterbox.hh>
#include <uwt/x86/context.hh>
#include <uwt/x86/memory.hh>
#include <uwt/utils/logchannels.hh>
#include <uwt/utils/misc.hh>
#include <unisim/util/dbgate/client/client.hh>
#include <iostream>
#include <sstream>
#include <set>
#include <vector>
#include <algorithm>
#include <fstream>
#include <cassert>
#include <cstring>

namespace UWT
{
  FunctionLab::FunctionLab( uint32_t address )
    : CFG()
    , entry_address( address )
  {}

  FunctionLab::~FunctionLab()
  {
  }

  void
  FunctionLab::decode( Memory& current_memory )
  {
    assert( entry_bloc == 0 );

    DebugStream dstr("translation/decoder.log");

    if (dstr.good())
      dstr << "Decode @" << std::hex << entry_address;

    struct Recursive
    {
      struct Node { Node() : insn(0), ibloc(0) {} InsnStatement* insn; IBloc* ibloc; };

      Recursive( Memory& _memory, std::vector<IBloc*>& _iblocs, std::ostream& _dstr )
        : memory(_memory), iblocs(_iblocs), dstr(_dstr)
      {}

      IBloc* decode( uint32_t addr )
      {
        IBloc  res;
        IBloc* prev_bloc = &res;
        bool   prev_alt = false;

        for (;;)
          {
            Node& node = fetched[addr];

            if (not node.insn)
              {
                uint8_t codebuf[15];
                memory.read( addr, &codebuf[0], sizeof (codebuf) );
                node.insn =  new InsnStatement( addr, &codebuf[0], sizeof (codebuf) );
                node.ibloc = new IBloc( node.insn );
                if (dstr.good())
                  dstr << std::hex << addr << "\tn" << std::dec << iblocs.size() << "\n";
                iblocs.push_back( node.ibloc );
              }

            prev_bloc->set_exit( prev_alt, node.ibloc );
            if (node.ibloc->count_predecessors() > 1)
              break;

            prev_bloc = node.ibloc;
            prev_alt = false;

            addr = node.insn->GetAddr() + node.insn->GetLength();

            auto branch = node.insn->GetBranch();

            if (branch.cond.good())
              {
                IBloc* next_bloc = this->decode( addr );
                prev_bloc->set_exit( false, next_bloc );
                prev_alt = true;
                if (branch.mode != branch.jump)
                  throw 0; // No conditionnal call or ret in intel ISA
              }

            if      (branch.mode == branch.call)
              continue;
            else if (branch.mode == branch.ret)
              break;
            else if (auto baddr = dynamic_cast<unisim::util::symbolic::ConstNode<uint32_t> const*>( branch.addr.node ))
              addr = baddr->value;
            else /* Indirect jump*/
              break;
          }
        return res.get_exit(0);
      }

      std::map<uint32_t,Node> fetched;
      Memory&                 memory;
      std::vector<IBloc*>&    iblocs;
      std::ostream&           dstr;
    } recursive( current_memory, bloc_pool, dstr );

    entry_bloc = recursive.decode( entry_address );
    IBloc::ref(entry_bloc);
  }

  void
  FunctionLab::translate( char const* _modname, char const* _fncname, std::ostream& _sink )
  {
    // Writing source
    unisim::util::symbolic::ccode::SrcMgr srcmgr( _sink );
    Core::ctxvar = "_ctx";

    srcmgr
      << "#include <main_exe/breakpoint.hh>\n"
      << "#include <uwt/x86/context.hh>\n"
      << "#include <uwt/x86/memory.hh>\n"
      << "#include <inttypes.h>\n\n"
      << "using namespace UWT;\n\n"
      << "namespace " << _modname << " {\n"
      << "uint32_t\n" << _fncname << "( Context& " << Core::ctxvar << " )\n{\n";

    IBloc* ibloc = bloc_pool[0];

    unisim::util::symbolic::ccode::Variables vars;

    for (uintptr_t idx = 0; idx < ibloc->size(); idx++)
      {
        ibloc->at(idx)->GetVariables(vars);
      }

    srcmgr << vars;

    for (uintptr_t idx = 0; idx < ibloc->size(); idx++)
      {
        ibloc->at(idx)->translate( srcmgr );
      }
    srcmgr << "}\n};\n";
  }

  void
  FunctionLab::decompile()
  {
    // Computing flags production/consumption to adjust flag
    // generation to exact needs.  FIXME: We should check whether all
    // needed flags are served (i.e. callee doesn't need flags from
    // the caller)
    {
      struct FlagStats
      {
        FlagStats() = delete;
        FlagStats( Flags const& in_flags, Flags const& out_flags )
          : m_needs(in_flags), m_begprods(out_flags), m_endprods(out_flags)
        {}

        bool
        update( Flags const& next_needs )
        {
          bool changed = false;

          changed |= m_endprods.changeto( next_needs & m_begprods );
          changed |= m_needs.changeto( m_needs | (next_needs & ~m_begprods) );

          return changed;
        }

        Flags m_needs, m_begprods, m_endprods;
      };

      std::map<IBloc*, FlagStats> flmap;

      for (IBloc* bloc : bloc_pool)
        {
          Flags in_flags, out_flags;
          bloc->at(0)->GetFlagStats( in_flags, out_flags );
          flmap.emplace( std::piecewise_construct, std::forward_as_tuple( bloc ), std::forward_as_tuple( in_flags, out_flags ) );
        }

      uintptr_t const ibcount = bloc_pool.size();
      for (uintptr_t idx = 0, last_update = ibcount-1; ; idx = (idx+1) % ibcount)
        {
          IBloc* bloc = bloc_pool[idx];
          Flags next_needs;
          if (IBloc* nat = bloc->get_exit(0))
            next_needs |= flmap.at(nat).m_needs;
          if (IBloc* odd = bloc->get_exit(1))
            next_needs |= flmap.at(odd).m_needs;

          if (flmap.at(bloc).update( next_needs ))
            last_update = idx; // Mark as updated
          else if (idx == last_update)
            break; // No more updates
        }

      for (auto && kv : flmap)
        kv.first->at(0)->SetNeededFlags( kv.second.m_endprods );
    }

    bool success = collapse();
    if (not success) throw 0;
  }

  std::string
  FunctionLab::graph_name() const
  {
    return strx::fmt( "cg%08x", entry_address );
  }

} // end of namespace UWT

