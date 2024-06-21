/***************************************************************************
                          uwt/trans/cfg_textbench.cc
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

/** @file uwt/trans/cfg_textbench.cc
    @brief  implementation file for translation engine

*/

#include <uwt/trans/cfg_testbench.hh>
#include <uwt/trans/ibloc.hh>
#include <uwt/trans/ctrlstmt.hh>
#include <uwt/trans/statement.hh>
#include <uwt/utils/logchannels.hh>
#include <uwt/utils/misc.hh>
#include <uwt/launchbox.hh>
#include <unisim/util/symbolic/ccode/ccode.hh>
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
  IBloc* CFG_TestBench::newIBloc()
  {
    struct Nop : public BaseStatement
    {
      typedef unisim::util::symbolic::ccode::SrcMgr    SrcMgr;
      virtual void translate( SrcMgr& _sink ) const override {}
      virtual void SetCondTerm(char const*, bool) override {}
    };

    return new IBloc( new Nop );
  }

  void Comprehensive::nextwave()
  {
    Choice* choice = base.tail;

    if (choice->head) throw 0;

    if (choice == &base) return;

    while (not choice->increment())
      {
        choice = choice->tail;
        delete choice->head;
        choice->head = 0;
        if (choice == &base) throw Stop();
      }

    base.tail = &base;
  }

  void CFG_TestBench::generate( CFG_TestBench::Seed& seed, unsigned size )
  {
    seed.nextwave();

    // #1 generate the spanning tree
    struct GenBloc { IBloc* bloc; unsigned subs; } gblocs[size];

    struct
    {
      IBloc* process( Seed& seed, GenBloc* head, unsigned count )
      {
        IBloc* bloc = newIBloc();
        head->bloc = bloc;
        head->subs = count;

        if (--count > 0)
          {
            unsigned tcnt = count > 1 ? seed.choose( count ) : 0;

            if (not tcnt)
              {
                bloc->set_exit( false, process( seed, &head[1], count ) );
              }
            else
              {
                unsigned fcnt = count - tcnt;
                bloc->set_exit( false, process( seed, &head[1],      fcnt ) );
                bloc->set_exit(  true, process( seed, &head[1+fcnt], tcnt ) );
              }
          }

        return bloc;
      }
    } trespan;
    IBloc* root = trespan.process( seed, &gblocs[0], size );

    // #2 populating with additional random non-tree edges
    // (preserving the spanning tree)
    for (uintptr_t oidx = 0; oidx < size; ++oidx)
      {
        IBloc* const origin = gblocs[oidx].bloc;
        for (unsigned choice = 0; choice < 2; ++choice)
          {
            if (origin->get_exit(choice))
              continue;
            if (seed.choose( 2 ))
              break;

            // In order to preserve the spanning tree, the new
            // edge must not got from the origin node to a
            // node not already visited during spanning tree
            // construction; meaning: eidx < (oidx+subs@oidx)
            // [0 .. x-1]: cross edges,
            // [x .. oidx-1]: retreating edges,
            // [oidx .. oidx+subs-1]: forward edges
            uintptr_t eidx = seed.choose( oidx + gblocs[oidx].subs );
            IBloc* ending = gblocs[eidx].bloc;

            if (ending == origin->get_exit(not choice))
              break; // do not keep solutions with twin branchs

            origin->set_exit(choice, ending);
          }
      }

    // Populate the lab with blocs and entry point
    if (bloc_pool.size()) throw 0;
    bloc_pool.resize(size);
    for (uintptr_t idx = 0; idx < size; ++idx)
      bloc_pool[idx] = gblocs[idx].bloc;
    setroot(root);
  }

  void
  CFG_TestBench::setroot(IBloc* root)
  {
    entry_bloc = root;
    IBloc::ref(entry_bloc);
  }

  void
  CFG_TestBench::graphit( std::ostream& sink )
  {
    refresh_props();

    sink << "digraph fullgraph {\n  node [ fontname=Helvetica, fontsize=12, penwidth=2 ];\n  edge [ penwidth=2 ];\n";

    for (IBloc* bloc : bloc_pool)
      sink << "  n" << bloc->get_ordering() << "\n";

    for (IBloc* bloc : bloc_pool)
      {
        for (unsigned choice = 0; choice < 2; ++choice)
          if (IBloc* next = bloc->get_exit(choice))
            sink << "  n" << bloc->get_ordering() << " -> n" << next->get_ordering() << " [label=" << choice << "];\n";
        if (IBloc* idom = bloc->get_idom())
          if (idom != bloc)
            sink << "  n" << idom->get_ordering() << " -> n" << bloc->get_ordering() << " [color=\"red\"];\n";
      }

    sink << "}\n";
  }

  std::string CFG_TestBench::graph_name() const { std::ostringstream buf; buf << "test" << test_idx; return buf.str(); }

  void CFG_TestBench::test( uintptr_t maxsteps )
  {
    // struct Constant : public Seed
    // {
    //   void nextwave() { idx = 0; }
    //   unsigned choose(unsigned int count)
    //   {
    //     struct { unsigned count, choice; }
    //     table[] =
    //       {
    //        /* Nodes */
    //        {7,1}, {5,0}, {4,0}, {3,0}, {2,0},
    //        /* Edges */
    //        {2,0}, {0,4},   {2,0}, {0,5},   {2,0}, {0,1},   {2,0}, {0,0},   {2,0}, {0,1},   {2,0}, {0,1}, {2,0}, {0,4},   {2,0}, {0,6}, {2,0}, {0,6},
    //       };
    //     auto const& res = table[idx++];
    //     if ((res.count != 0 and res.count  != count) or
    //         (res.count == 0 and res.choice >= count)) throw 0;
    //     return res.choice;
    //   }
    //   int idx;
    // } seed;

    Comprehensive seed;
    for (uintptr_t step = 0; step < maxsteps; ++step)
      {
        //LaunchBox::trans_graphs = (step == 0);
        //LaunchBox::trans_graphs = true;

        if ((step%0x1000) == 0)            { std::cerr << "\r\e[K" << step << " processed graphs [ongoing]"; std::cerr.flush(); }

        // poll for a new graph
        CFG_TestBench graph(step);
        try { graph.generate(seed,8); }
        catch (Comprehensive::Stop const&) { std::cerr << "\r\e[K" << step << " processed graphs [finished]" << std::endl; break; }

        // if (graph.reducible())
        //   continue;

        // graph.graphit( Log::filechan( "graph", "toto" ) );
        // Log::delchan("graph");

        if (graph.collapse())
          continue;

        static struct Report
        {
          Report()
            : sink( "report.html" )
            , separator("")
          {
            sink << "<html><head><title>Report</title><style type = \"text/css\">"
                 << "table { border-collapse: collapse; border: solid 1px black; } table td { text-align: center; vertical-align: middle; position: relative; }"
                 << "</style></head><body><table border=\"1\">\n<tr>";
          }
          ~Report() { close(); }
          void close()
          {
            if (not sink.good()) return;
            sink << "</tr>\n</table></body></html>\n";
            sink.close();
          }
          bool record( std::string const& k )
          {
            if (not keys.insert(k).second)
              return false;
            uintptr_t imgidx = keys.size();
            imgname = strx::fmt( "img%03d.dot", imgidx );
            sink << separator << "<td><img src=\"" << imgname << ".png\" /></td>";
            separator = (imgidx % 6) ? "" : "</tr>\n<tr>";
            return true;
          }

          std::set<std::string> keys;
          std::string imgname;
          std::ofstream sink;
          char const* separator;
        } report;

        std::string key = graph.getkey();

        if (not report.record(key))
          {
            return;
          }

        {
          std::ofstream _( report.imgname.c_str() );
          graph.graphit( _ );
        }

        if (report.keys.size() >= 1 /*144*/)
          {
            report.close();
            throw 0;
          }
      }
  }
}
// end of namespace UWT

