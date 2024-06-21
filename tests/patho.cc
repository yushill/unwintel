/***************************************************************************
                              tests/patho.cc
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
#include <array>

int main(int argc, char** argv)
{
  UWT::CFG_TestBench graph(0);

  auto x459100 = graph.newIBloc();
  auto x453510 = graph.newIBloc();
  auto x44f010 = graph.newIBloc();
  auto x452b00 = graph.newIBloc();
  auto x450930 = graph.newIBloc();
  auto x450830 = graph.newIBloc();

  x459100->set_exits(UWT::IBloc::Exits(x453510, x459100));
  x453510->set_exits(UWT::IBloc::Exits(x44f010, x450930));
  x44f010->set_exits(UWT::IBloc::Exits(x452b00, x453510));
  x452b00->set_exits(UWT::IBloc::Exits(x450930, x459100));
  x450930->set_exits(UWT::IBloc::Exits(x450830, x44f010));
  x450830->set_exits(UWT::IBloc::Exits(x453510, x452b00));

  std::array<UWT::IBloc*,6> nodes{x459100, x453510, x44f010, x452b00, x450930, x450830};


  graph.import(nodes.begin(), nodes.end());

  graph.graphit(std::cout);

  if (not graph.collapse())
    throw 0;

  //   unsigned dups = graph.trs["Duplication"];
  //   if (dups <= maxdups) continue;
  //   maxdups = dups;

  //   std::ofstream sink(strx::fmt( "graph%u.dot", maxdups ));
  //   sink << dot.str();

  //   std::cerr << std::endl;
  //   return 0;
  // }

  return 0;
}
