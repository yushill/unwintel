/***************************************************************************
                              tests/scan.cc
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

#include <uwt/trans/cfg_testbench.cc>
#include <uwt/trans/cfg.cc>
#include <uwt/trans/ibloc.cc>
#include <uwt/trans/ctrlstmt.cc>
#include <uwt/launchbox.cc>
#include <uwt/utils/logchannels.cc>
#include <uwt/utils/misc.cc>
#include <unisim/util/symbolic/symbolic.cc>
#include <unisim/util/symbolic/ccode/ccode.cc>
#include <unisim/util/dbgate/client/client.cc>

int main()
{
  uintptr_t maxsteps = 0x1000000;

  // struct Constant : public UWT::CFG_TestBench::Seed
  // {
  //   void nextwave() { idx = 0; }
  //   unsigned choose(unsigned int count)
  //   {
  //     struct { unsigned count, choice; }
  //     table[] =
  //       {
  //        /* Nodes */ {7,1},   {5,0},   {4,0},   {3,0},   {2,0},
  //        /* Edges */ {2,0}, {0,4},   {2,0}, {0,5},   {2,0}, {0,1},   {2,0}, {0,0},   {2,0}, {0,1},   {2,0}, {0,1}, {2,0}, {0,4},   {2,0}, {0,6}, {2,0}, {0,6},
  //       };
  //     auto const& res = table[idx++];
  //     if ((res.count != 0 and res.count  != count) or
  //         (res.count == 0 and res.choice >= count)) throw 0;
  //     return res.choice;
  //   }
  //   int idx;
  // } seed;

  UWT::Comprehensive seed;
  unsigned maxdups = 0;
  for (uintptr_t step = 0; step < maxsteps; ++step)
    {
      //LaunchBox::trans_graphs = true;

      if ((step%0x1000) == 0)            { std::cerr << "\r\e[K" << step << " processed graphs [ongoing]"; std::cerr.flush(); }

      // poll for a new graph
      UWT::CFG_TestBench graph(step);
      try { graph.generate(seed,6); }
      catch (UWT::Comprehensive::Stop const&) { std::cerr << "\r\e[K" << step << " processed graphs [finished]" << std::endl; break; }

      if (graph.reducible())
        continue;

      std::ostringstream dot;
      graph.graphit( dot );

      if (not graph.collapse())
        throw 0;

      unsigned dups = graph.trs["Duplication"];
      if (dups <= maxdups) continue;
      maxdups = dups;

      std::ofstream sink(strx::fmt( "graph%u.dot", maxdups ));
      sink << dot.str();
    }

  return 0;
}
