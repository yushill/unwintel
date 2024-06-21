/***************************************************************************
                                 uwt/fwd.hh
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

/** @file uwt/fwd.hh
    @brief  forward declarations

*/

#ifndef UWT_FWD_HH
#define UWT_FWD_HH

#include <inttypes.h>

namespace UWT {
  // X86
  class Context;
  typedef uint32_t (*hostcode_t)( Context& );
  class Memory;
  class Flags;
  class CallDB;

  // TRANS
  struct IBloc;
  struct FunctionLab;
  struct BaseStatement;
  struct InsnStatement;

  // WIN
  class WinNT;
  namespace PEF { class PEDigger; }

  // STRAP
  class Module;
  class MainModule;
  class IterBox;
};

#endif // UWT_FWD_HH
