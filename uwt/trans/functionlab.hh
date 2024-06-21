/***************************************************************************
                          uwt/trans/functionlab.hh
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

/** @file uwt/trans/functionlab.hh
    @brief  header file for translation engine

*/

#ifndef UWT_FUNCTIONLAB_HH
#define UWT_FUNCTIONLAB_HH

#include <uwt/trans/cfg.hh>
#include <uwt/fwd.hh>
#include <vector>
#include <string>
#include <map>
#include <iosfwd>
#include <inttypes.h>

namespace UWT
{
  struct GraphMatch;

  struct FunctionLab : public CFG
  {
    enum Exception { GraphIncoherency, InternalError };

    FunctionLab( uint32_t _addr );
    ~FunctionLab();

    void                        decode( Memory& current_memory );
    void                        decompile();
    void                        translate( char const* _modname, char const* _fncname, std::ostream& _sink );

    virtual std::string         graph_name() const override;

  private:
    uint32_t                    entry_address;
  };

} // end of namespace UWT

#endif // UWT_FUNCTIONLAB_HH
