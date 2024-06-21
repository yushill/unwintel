/***************************************************************************
                           uwt/trans/core.cc
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

/** @file uwt/trans/core.cc
 *  @brief  implementation file for abstract interpretation core utils
*/

#include <uwt/trans/core.hh>
#include <uwt/trans/insnstmt.hh>
#include <iostream>
#include <sstream>
#include <vector>

namespace UWT
{
  char const* Core::ctxvar = 0;

  Core::Core( ActionNode* actions, unisim::util::symbolic::Expr const& entrypoint )
    : path(actions), next_insn_addr(entrypoint)
    , fcw(newRegRead(FCW())), ftop_source(new FTop), ftop(0)
    , mxcsr(newRegRead(MXCSR()))
  {
    for (SRegID reg; reg.next();)
      segment_registers[reg.idx()] = newRegRead( reg );

    for (EIRegID reg; reg.next();)
      regvalues[reg.idx()][0] = newRegRead( reg );

    for (FLAG reg; reg.next();)
      flagvalues[reg.idx()] = newRegRead( reg );

    for (FRegID reg; reg.next();)
      fpuregs[reg.idx()] = newRegRead( reg );
  }

  Core::Expr
  Core::eregread( unsigned reg, unsigned size, unsigned pos )
  {
    using unisim::util::symbolic::ExprNode;
    using unisim::util::symbolic::make_const;
    using unisim::util::symbolic::shift_type;

    uint32_t const value_mask = uint32_t(-1) >> (32-8*size);

    if (not regvalues[reg][pos].node)
      {
        // requested read is in the middle of a larger value
        unsigned src = pos;
        do { src = src & (src-1); } while (not regvalues[reg][src].node);
        unsigned shift = 8*(pos - src);
        return
          make_operation( "And",
                          make_operation( "Lsr", regvalues[reg][src], make_const<shift_type>( shift ) ),
                          make_const( value_mask )
                          );
      }
    else if (not regvalues[reg][(pos|size)&(GREGSIZE-1)].node)
      {
        // requested read is in lower bits of a larger value
        return
          make_operation( "And", regvalues[reg][pos], make_const( value_mask ) );
      }
    else if ((size > 1) and (regvalues[reg][pos|(size >> 1)].node))
      {
        // requested read is a concatenation of multiple source values
        Expr concat = regvalues[reg][pos];
        for (unsigned idx = 0; ++idx < size;)
          {
            if (not regvalues[reg][pos+idx].node)
              continue;
            concat = make_operation( "Or", make_operation( "Lsl", regvalues[reg][idx], make_const<shift_type>( 8*idx ) ), concat );
          }
        return concat;
      }

    // requested read is directly available
    return regvalues[reg][pos];
  }

  void
  Core::eregwrite( unsigned reg, unsigned size, unsigned pos, Expr const& xpr )
  {
    Expr nxt[GREGSIZE];

    for (unsigned ipos = pos, isize = size, cpos;
         cpos = (ipos^isize) & (GREGSIZE-1), (not regvalues[reg][ipos].node) or (not regvalues[reg][cpos].node);
         isize *= 2, ipos &= -isize
         )
      {
        nxt[cpos] = eregread( reg, isize, cpos );
      }

    for (unsigned ipos = 0; ipos < GREGSIZE; ++ipos)
      {
        if (nxt[ipos].node)
          regvalues[reg][ipos] = nxt[ipos];
      }

    regvalues[reg][pos] = xpr;

    for (unsigned rem = 1; rem < size; ++rem)
      {
        regvalues[reg][pos+rem] = 0;
      }
  }

  bool
  Core::close( Core const& ref )
  {
    bool complete = path->close();
    path->updates.insert( Expr( new EIPWrite( next_insn_addr, next_insn_mode ) ) );

    // Scalar integer registers
    struct { bool operator()( Expr const* a, Expr const* b )
      { for (unsigned ipos = 0; ipos < GREGSIZE; ++ipos) { if (a[ipos] != b[ipos]) return true; } return false; }
    } regdiff;
    for (EIRegID reg; reg.next();)
      if (regdiff( regvalues[reg.idx()], ref.regvalues[reg.idx()] ))
        path->updates.insert( newRegWrite( reg, eregread( reg.idx(), GREGSIZE, 0 ) ) );

    // Flags
    for (FLAG reg; reg.next();)
      if (flagvalues[reg.idx()] != ref.flagvalues[reg.idx()])
        path->updates.insert( newRegWrite( reg, flagvalues[reg.idx()] ) );

    // Segment registers
    for (SRegID reg; reg.next();)
      if (segment_registers[reg.idx()] != ref.segment_registers[reg.idx()])
        path->updates.insert( newRegWrite( reg, segment_registers[reg.idx()] ) );

    // FPU Control Word
    if (fcw != ref.fcw)
      path->updates.insert( newRegWrite(FCW(), fcw) );

    if (mxcsr.expr != ref.mxcsr.expr)
      path->updates.insert( newRegWrite(MXCSR(), mxcsr.expr) );

    // FPU registers
    for (FRegID reg; reg.next();)
      if (fpuregs[reg.idx()] != ref.fpuregs[reg.idx()])
        path->updates.insert( new FRegWrite( *this, fpuregs[reg.idx()], Core::ftop_update(ftop_source, reg.idx()).expr ) );

    if (ftop)
      path->updates.insert( new FTopWrite( Core::ftop_update(ftop_source, ftop).expr ) );

    for (auto && update : updates)
      path->updates.insert( update );

    return complete;
  }

  void
  Core::setnip( addr_t eip, ipproc_t ipproc )
  {
    next_insn_addr = eip.expr;
    next_insn_mode = ipproc;
  }

  bool
  Core::concretize(unisim::util::symbolic::Expr cond)
  {
    if (unisim::util::symbolic::ConstNodeBase const* cnode = cond.ConstSimplify())
      return dynamic_cast<unisim::util::symbolic::ConstNode<bool> const&>(*cnode).value;

    bool predicate = path->proceed( cond );
    path = path->next( predicate );
    return predicate;
  }

  void
  FIRoundBase::translate( SrcMgr& sink, CCode& ccode ) const
  {
    sink << "FIRound( " << ccode(src) << " )";
  }

  void
  FIRoundBase::Repr( std::ostream& sink ) const
  {
    sink << "FIRound<";
    GetType().Repr(sink);
    sink << ">(" << src << ", " << rmode << ")";
  }

  void
  Core::FLAG::GetName( std::ostream& sink, bool read ) const
  {
    sink << Core::ctxvar << '.' << (read ? 'r' : 'w');
    switch (code)
      {
      case CF: sink << "eflag( Context::CF )"; break;
      case PF: sink << "eflag( Context::PF )"; break;
      case AF: sink << "eflag( Context::AF )"; break;
      case ZF: sink << "eflag( Context::ZF )"; break;
      case SF: sink << "eflag( Context::SF )"; break;
      case DF: sink << "eflag( Context::DF )"; break;
      case OF: sink << "eflag( Context::OF )"; break;
      case C0: sink << "fpuflag( Context::FPUC0 )"; break;
      case C1: sink << "fpuflag( Context::FPUC1 )"; break;
      case C2: sink << "fpuflag( Context::FPUC2 )"; break;
      case C3: sink << "fpuflag( Context::FPUC3 )"; break;
      default: throw *this;
      }
  }

  void
  Core::EIPWrite::GetRegName(std::ostream& sink) const
  {
    sink << "eip";
  }

  void
  Core::EIRegID::GetName( std::ostream& sink, bool read ) const
  {
    sink << ctxvar << ".G.D.E" << &"AX\0CX\0DX\0BX\0SP\0BP\0SI\0DI"[(idx()%8)*3];
  }

  void
  Core::SRegID::GetName( std::ostream& sink, bool read ) const
  {
    sink << "Ctrl16(" << ctxvar << ", &Context::" << &"ES\0CS\0SS\0DS\0FS\0GS\0??\0??"[(idx()%8)*3] << ")";
  }

  void
  Core::FRegID::GetName( std::ostream& sink, bool read ) const
  {
    sink << ctxvar << '.' << (read ? 'r' : 'w') << "fpust( " << std::dec << idx() << " )";
  }

  void
  Core::FRegWrite::Repr( std::ostream& sink ) const
  {
    sink << "FRegWrite( " << freg << ", " << value << " )";
  }

  void Core::FRegWrite::translate( SrcMgr& sink, CCode& ccode ) const
  {
    sink << Core::ctxvar << ".wfpuabs( " << ccode(freg) << ", " << ccode(value) << " );\n";
  }

  void
  Core::LoadBase::Repr( std::ostream& sink ) const
  {
    sink << "Load" << (8*bytes) << "(" << (int)segment << "," << addr << ")";
  }

  void
  Core::LoadBase::translate( SrcMgr& sink, CCode& ccode ) const
  {
    /* TODO: process segment */
    sink << Core::ctxvar << ".mem.r<" << unisim::util::symbolic::ccode::dec(bytes) << ">( " << ccode(addr) << " )";
  }

  void
  Core::Store::Repr( std::ostream& sink ) const
  {
    sink << "Store" << (8*bytes) << "( " << (int)segment << ", " << addr << ", " << value <<  " )";
  }

  void
  Core::Store::translate( SrcMgr& sink, CCode& ccode ) const
  {
    /* TODO: process segment */
    sink << Core::ctxvar << ".mem.w<" << unisim::util::symbolic::ccode::dec(bytes) << ">( " << ccode(addr) << " ) = " << ccode(value) << ";\n";
  }

  void
  Core::Interrupt::Repr( std::ostream& sink ) const
  {
    sink << "Interrupt(" << op << ", " << code <<  ")";
  }

  void
  Core::Interrupt::translate( SrcMgr& sink, CCode& ccode ) const
  {
    sink << Core::ctxvar << ".interrupt(" << unisim::util::symbolic::ccode::hex(op) << ", " << unisim::util::symbolic::ccode::hex(code) << ");\n";
  }

  void
  Core::FCW::GetName(std::ostream& sink, bool) const
  {
    sink << "Ctrl16( " << Core::ctxvar << ", &Context::FPUControl )";
  }

  void
  Core::MXCSR::GetName(std::ostream& sink, bool) const
  {
    sink << "Ctrl32( " << Core::ctxvar << ", &Context::MXCSR )";
  }

  void
  Core::FTop::Repr( std::ostream& sink ) const
  {
    sink << "FpuStackTop";
  }

  void
  Core::FTop::translate( SrcMgr& sink, CCode& ccode ) const
  {
    sink << Core::ctxvar << ".fputop()";
  }

  void
  Core::FTopWrite::GetRegName(std::ostream& sink) const
  {
    sink << "ftop";
  }

  void
  Core::FTopWrite::translate( SrcMgr& sink, CCode& ccode ) const
  {
    sink << Core::ctxvar << ".fputop( " << ccode(value) << " );\n";
  }

  void
  Core::noexec( Operation const& op )
  {
    std::cerr
      << "error: no execute method in `" << typeid(op).name() << "'\n"
      << std::hex << op.address << ":\t";
    op.disasm( std::cerr );
    std::cerr << '\n';
    throw 0;
  }

  void
  Core::_DE() { std::cerr << "Unimplemented division error trap.\n"; }

  void eval_div( Core& arch, Core::u64_t& hi, Core::u64_t& lo, Core::u64_t const& divisor )    { throw std::runtime_error( "operation unvailable in 32 bit mode" ); }
  void eval_div( Core& arch, Core::s64_t& hi, Core::s64_t& lo, Core::s64_t const& divisor )    { throw std::runtime_error( "operation unvailable in 32 bit mode" ); }
  void eval_mul( Core& arch, Core::u64_t& hi, Core::u64_t& lo, Core::u64_t const& multiplier ) { throw std::runtime_error( "operation unvailable in 32 bit mode" ); }
  void eval_mul( Core& arch, Core::s64_t& hi, Core::s64_t& lo, Core::s64_t const& multiplier ) { throw std::runtime_error( "operation unvailable in 32 bit mode" ); }
}

