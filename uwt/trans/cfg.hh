/***************************************************************************
                          uwt/trans/cfg.hh
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

/** @file uwt/trans/cfg.hh
    @brief  header file for translation engine

*/

#ifndef UWT_CFG_HH
#define UWT_CFG_HH

#include <uwt/fwd.hh>
#include <vector>
#include <map>
#include <string>
#include <inttypes.h>

namespace UWT
{
  struct GraphMatch;

  struct CFG
  {
    CFG() : bloc_pool(), entry_bloc(0) {}
    ~CFG();

    bool                        collapse();
    virtual std::string         graph_name() const = 0;

    bool                        reducible();

    void record_trans( std::string const& trans ) { trs[trans] += 1; }
    std::map<std::string,uintptr_t> trs;

  protected:
    void                        purge();
    void                        refresh_props();
    std::string                 getkey() const;

    std::vector<IBloc*>         bloc_pool;
    IBloc*                      entry_bloc;
  };

} // end of namespace UWT

#endif // UWT_CFG_HH
