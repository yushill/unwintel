/***************************************************************************
                           uwt/trans/core.hh
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

/** @file uwt/trans/core.hh
 *  @brief  header file for abstract interpretation core utils
*/

#ifndef UWT_CORE_HH
#define UWT_CORE_HH

#include <unisim/component/cxx/processor/intel/isa/intel.hh>
#include <unisim/component/cxx/processor/intel/modrm.hh>
#include <unisim/util/symbolic/ccode/ccode.hh>
#include <unisim/util/symbolic/symbolic.hh>
#include <uwt/trans/statement.hh>
#include <uwt/utils/misc.hh>
#include <uwt/x86/flags.hh>
#include <uwt/fwd.hh>
#include <map>
#include <set>
#include <limits>
#include <stdexcept>
#include <typeinfo>
#include <inttypes.h>

namespace UWT
{
  struct FIRoundBase : public unisim::util::symbolic::ccode::CNode
  {
    typedef unisim::util::symbolic::ccode::SrcMgr SrcMgr;
    typedef unisim::util::symbolic::ccode::CCode CCode;
    typedef unisim::util::symbolic::Expr Expr;
    typedef unisim::util::symbolic::ValueType ValueType;
    FIRoundBase( Expr const& _src, int _rmode ) : src(_src), rmode(_rmode) {} Expr src; int rmode;
    virtual void Repr( std::ostream& sink ) const;
    virtual void translate( SrcMgr& srcmgr, CCode& ccode ) const;
    virtual unsigned SubCount() const { return 1; }
    virtual Expr const& GetSub(unsigned idx) const { if (idx != 0) return ExprNode::GetSub(idx); return src; }
    int compare( FIRoundBase const& rhs ) const { return rmode - rhs.rmode; }
    virtual int cmp( ExprNode const& rhs ) const { return compare( dynamic_cast<FIRoundBase const&>( rhs ) ); }
  };

  template <typename fpT>
  struct FIRound : public FIRoundBase
  {
    typedef FIRound<fpT> this_type;
    FIRound( Expr const& src, int rmode ) : FIRoundBase(src, rmode) {}
    virtual this_type* Mutate() const override { return new this_type( *this ); }
    virtual ValueType GetType() const override { return unisim::util::symbolic::CValueType(fpT()); }
  };

  template <typename A, unsigned S> using TypeFor = typename unisim::component::cxx::processor::intel::TypeFor<A,S>;
  using unisim::component::cxx::processor::intel::GOw;
  using unisim::component::cxx::processor::intel::GObLH;
  using unisim::component::cxx::processor::intel::GOd;

  typedef unisim::util::arithmetic::Integer<4, false> uint128_t;
  typedef unisim::util::arithmetic::Integer<4, true>   int128_t;

  struct Core
  {
    typedef unisim::component::cxx::processor::intel::RMOp<Core> RMOp;
    typedef unisim::component::cxx::processor::intel::Operation<Core> Operation;
    typedef unisim::component::cxx::processor::intel::InputCode<Core> InputCode;

    typedef unisim::util::symbolic::SmartValue<bool> bit_t;

    typedef unisim::util::symbolic::SmartValue<uint8_t>   u8_t;
    typedef unisim::util::symbolic::SmartValue<uint16_t>  u16_t;
    typedef unisim::util::symbolic::SmartValue<uint32_t>  u32_t;
    typedef unisim::util::symbolic::SmartValue<uint64_t>  u64_t;
    typedef unisim::util::symbolic::SmartValue<uint128_t> u128_t;

    typedef unisim::util::symbolic::SmartValue<int8_t>   s8_t;
    typedef unisim::util::symbolic::SmartValue<int16_t>  s16_t;
    typedef unisim::util::symbolic::SmartValue<int32_t>  s32_t;
    typedef unisim::util::symbolic::SmartValue<int64_t>  s64_t;
    typedef unisim::util::symbolic::SmartValue<int128_t> s128_t;

    typedef unisim::util::symbolic::SmartValue<float> f32_t;
    typedef unisim::util::symbolic::SmartValue<double> f64_t;
    typedef unisim::util::symbolic::SmartValue<long double> f80_t;

    typedef u32_t addr_t;

    typedef GOd   GR;
    typedef u32_t gr_type;

    typedef unisim::util::symbolic::ccode::CNode CNode;
    typedef unisim::util::symbolic::ccode::SrcMgr SrcMgr;
    typedef unisim::util::symbolic::ccode::CCode CCode;
    typedef unisim::util::symbolic::ccode::Update Update;
    typedef unisim::util::symbolic::ccode::RegReadBase RegReadBase;
    typedef unisim::util::symbolic::ccode::RegWriteBase RegWriteBase;
    typedef unisim::util::symbolic::ccode::ActionNode ActionNode;
    typedef unisim::util::symbolic::Expr Expr;
    typedef unisim::util::symbolic::ValueType ValueType;

    Core( ActionNode* actions, Expr const& entrypoint );

    ActionNode*     path;

    bool close( Core const& ref );

    struct OfCore { OfCore( Core& _core ) : core( _core ) {} Core& core; };

    static Core* FindRoot( unisim::util::symbolic::Expr const& expr )
    {
      if (OfCore const* node = dynamic_cast<OfCore const*>( expr.node ))
        return &node->core;

      for (unsigned idx = 0, end = expr->SubCount(); idx < end; ++idx)
        if (Core* found = FindRoot( expr->GetSub(idx)))
          return found;
      return 0;
    }

    struct EIRegID
      : public unisim::util::identifier::Identifier<EIRegID>
      , public unisim::util::symbolic::WithValueType<EIRegID>
    {
      typedef uint32_t value_type;
      enum Code { eax = 0, ecx = 1, edx = 2, ebx = 3, esp = 4, ebp = 5, esi = 6, edi = 7, end } code;

      char const* c_str() const
      {
        switch (code)
          {
          case eax: return "eax";
          case ecx: return "ecx";
          case edx: return "edx";
          case ebx: return "ebx";
          case esp: return "esp";
          case ebp: return "ebp";
          case esi: return "esi";
          case edi: return "edi";
          case end: break;
          }
        return "NA";
      }

      void GetName(std::ostream&, bool) const;

      EIRegID() : code(end) {}
      EIRegID( Code _code ) : code(_code) {}
      EIRegID( char const* _code ) : code(end) { init( _code ); }
    };

    struct FLAG
      : public UWT::FLAG
      , public unisim::util::symbolic::WithValueType<FLAG>
    {
      typedef bool value_type;

      void GetName(std::ostream&, bool) const;

      FLAG() : UWT::FLAG() {}
      FLAG(Code _code) : UWT::FLAG(_code) {}
      FLAG(char const* _code) : UWT::FLAG(_code) {}
    };


    struct SRegID
      : public unisim::util::identifier::Identifier<SRegID>
      , public unisim::util::symbolic::WithValueType<SRegID>
    {
      typedef uint16_t value_type;
      enum Code { es, cs, ss, ds, fs, gs, end } code;

      char const* c_str() const
      {
        switch (code)
          {
          case  es: return "es";
          case  cs: return "cs";
          case  ss: return "ss";
          case  ds: return "ds";
          case  fs: return "fs";
          case  gs: return "gs";
          case end: break;
          }
        return "NA";
      }

      void GetName(std::ostream&, bool) const;

      SRegID() : code(end) {}
      SRegID( Code _code ) : code(_code) {}
      SRegID( char const* _code ) : code(end) { init( _code ); }
    };

    struct FRegID
      : public unisim::util::identifier::Identifier<FRegID>
      , public unisim::util::symbolic::WithValueType<FRegID>
    {
      typedef double value_type;
      enum Code { st0=0, st1, st2, st3, st4, st5, st6, st7, end } code;
      char const* c_str() const { return &"st0\0st1\0st2\0st3\0st4\0st5\0st6\0st7"[(unsigned(code) % 8)*4]; }

      void GetName(std::ostream&, bool ) const;

      FRegID() : code(end) {}
      FRegID( Code _code ) : code(_code) {}
      FRegID( char const* _code ) : code(end) { init( _code ); }
    };

    template <typename RID> Expr newRegRead( RID _id ) { return new unisim::util::symbolic::ccode::RegRead<RID>(_id); }

    template <typename RID> Expr newRegWrite( RID _id, Expr const& _value ) { return new unisim::util::symbolic::ccode::RegWrite<RID>(_id, _value); }

    struct OpHeader
    {
      uint32_t address;
      OpHeader( uint32_t _address ) : address(_address) {}
    };

    int hash();

    u64_t                       tscread() { throw 0; return u64_t( 0 ); }

    Expr                        flagvalues[FLAG::end];


    bit_t                       flagread( FLAG flag ) { return bit_t(flagvalues[flag.idx()]); }
    void                        flagwrite( FLAG flag, bit_t fval ) { flagvalues[flag.idx()] = fval.expr; }
    void                        flagwrite( FLAG flag, bit_t fval, bit_t ) { flagvalues[flag.idx()] = fval.expr; }

    Expr                        segment_registers[6];

    u16_t                       segregread( unsigned idx ) { return u16_t(segment_registers[idx]); }
    void                        segregwrite( unsigned idx, u16_t value ) { segment_registers[idx] = value.expr; }

    static unsigned const GREGCOUNT = 8;
    static unsigned const GREGSIZE = 4;

    Expr                        regvalues[GREGCOUNT][GREGSIZE];

    Expr                        eregread( unsigned reg, unsigned size, unsigned pos );
    void                        eregwrite( unsigned reg, unsigned size, unsigned pos, Expr const& xpr );

    template <class GOP>
    typename TypeFor<Core,GOP::SIZE>::u regread( GOP const&, unsigned idx )
    {
      return typename TypeFor<Core,GOP::SIZE>::u( gr_type( eregread(idx, GOP::SIZE / 8, 0) ) );
    }

    u8_t regread( GObLH const&, unsigned idx ) { return u8_t( gr_type( eregread(idx%4, 1, (idx >> 2) & 1) ) ); }

    template <class GOP>
    void regwrite( GOP const&, unsigned idx, typename TypeFor<Core,GOP::SIZE>::u const& val )
    {
      eregwrite( idx, GOP::SIZE / 8, 0, gr_type(val).expr );
    }

    void regwrite( GObLH const&, unsigned idx, u8_t val ) { eregwrite( idx%4, 1, (idx>>2) & 1, val.expr ); }

    enum ipproc_t { ipjmp = 0, ipcall, ipret };
    Expr                        next_insn_addr;
    ipproc_t                    next_insn_mode;

    addr_t                      getnip() { return addr_t(next_insn_addr); }
    void                        setnip( addr_t eip, ipproc_t ipproc = ipjmp );

    struct EIPWrite : public unisim::util::symbolic::ccode::RegWriteBase
    {
      EIPWrite( Expr const& _value, ipproc_t _hint ) : RegWriteBase( _value ), hint(_hint) {}
      virtual EIPWrite* Mutate() const override { return new EIPWrite( *this ); }
      virtual void GetRegName(std::ostream& sink) const override;
      virtual void translate( SrcMgr& srcmgr, CCode& ccode ) const override { /* can't be here */ throw *this; }
      int compare( EIPWrite const& rhs ) const { return int(hint) - int(rhs.hint); }
      virtual int cmp( ExprNode const& rhs ) const override { return compare( dynamic_cast<EIPWrite const&>( rhs ) ); }
      ipproc_t hint;
    };

    void                        fnanchk( f64_t value ) {};

    struct FCW : public unisim::util::symbolic::WithValueType<FCW>
    {
      typedef uint16_t value_type;
      void GetName(std::ostream& sink, bool read) const;
      int cmp(FCW const&) const { return 0; }
    };

    Expr                        fcw;
    int                         fcwreadRC() const { return 0; }
    u16_t                       fcwread() const { return u16_t(fcw); }
    void                        fcwwrite( u16_t value ) { fcw = value.expr; }
    void                        finit()
    {
      // FPU initialization
      flagwrite( FLAG::C0, bit_t(0) );
      flagwrite( FLAG::C1, bit_t(0) );
      flagwrite( FLAG::C2, bit_t(0) );
      flagwrite( FLAG::C3, bit_t(0) );
      fcwwrite( u16_t(0x037f) );
      // TODO: Complete FPU initialization
      // ftop = 0
      // fpustatus = 0
      // fputag = 0xffff
      // fpuoptr = 0
      // fpuiptr = 0
      // fpuopcode = 0
    }

    void                        fxam() { throw 0; }

    struct LoadBase : public CNode
    {
      LoadBase( unsigned _bytes, unsigned _segment, Expr const& _addr ) : addr(_addr), bytes(_bytes), segment(_segment) {}
      virtual void Repr( std::ostream& sink ) const override;
      virtual void translate( SrcMgr& srcmgr, CCode& ccode ) const override;
      virtual unsigned SubCount() const override { return 1; }
      virtual Expr const& GetSub(unsigned idx) const override { if (idx != 0) return ExprNode::GetSub(idx); return addr; }
      virtual int cmp( ExprNode const& rhs ) const override { return compare( dynamic_cast<LoadBase const&>( rhs ) ); }
      int compare( LoadBase const& rhs ) const
      {
        if (intptr_t delta = int(bytes) - int(rhs.bytes)) return delta;
        return int(segment) - int(rhs.segment);
      }

      Expr addr;
      uint8_t bytes, segment;
    };

    template <typename dstT>
    struct Load : public LoadBase
    {
      typedef Load<dstT> this_type;
      Load( unsigned bytes, unsigned segment, Expr const& addr ) : LoadBase(bytes, segment, addr) {}
      virtual this_type* Mutate() const override { return new this_type( *this ); }
      virtual ValueType GetType() const override { return unisim::util::symbolic::CValueType(dstT()); }
    };

    struct FRegWrite : public Update
    {
      FRegWrite( Core& core, Expr const& _value, Expr const& _freg )
        : value(_value), freg(_freg)
      {}
      virtual FRegWrite* Mutate() const override { return new FRegWrite(*this); }
      virtual void Repr( std::ostream& sink ) const override;
      virtual void translate( SrcMgr& srcmgr, CCode& ccode ) const override;
      virtual unsigned SubCount() const override { return 2; }
      virtual Expr const& GetSub(unsigned idx) const override { switch (idx) { case 0: return value; case 1: return freg; } return ExprNode::GetSub(idx); }
      int cmp( ExprNode const& rhs ) const override { return 0; }
      Expr value, freg;
    };

    struct Store : public Update
    {
      Store( unsigned _bytes, unsigned _segment, Expr const& _addr, Expr const& _value )
        : addr( _addr ), value( _value ), bytes(_bytes), segment(_segment)
      {}
      virtual Store* Mutate() const override { return new Store( *this ); }
      virtual void Repr( std::ostream& sink ) const override;
      virtual void translate( SrcMgr& srcmgr, CCode& ccode ) const override;
      virtual unsigned SubCount() const override { return 2; }
      virtual Expr const& GetSub(unsigned idx) const override { switch (idx) { case 0: return addr; case 1: return value; } return ExprNode::GetSub(idx); }
      int cmp( ExprNode const& rhs ) const override { return compare( dynamic_cast<Store const&>( rhs ) ); }
      int compare( Store const& rhs ) const
      {
        if (intptr_t delta = int(bytes) - int(rhs.bytes)) return delta;
        return int(segment) - int(rhs.segment);
      }

      Expr addr, value;
      uint8_t bytes, segment;
    };

    struct Interrupt : public Update
    {
      Interrupt( unsigned _op, unsigned _code )
        : op(_op), code( _code )
      {}
      virtual Interrupt* Mutate() const override { return new Interrupt( *this ); }
      virtual void Repr( std::ostream& sink ) const override;
      virtual void translate( SrcMgr& srcmgr, CCode& ccode ) const override;
      virtual unsigned SubCount() const override { return 0; }
      int cmp( ExprNode const& rhs ) const override { return compare( dynamic_cast<Interrupt const&>( rhs ) ); }
      int compare( Interrupt const& rhs ) const
      {
        if (int delta = int(op) - int(rhs.op)) return delta;
        return int(code) - int(rhs.code);
      }

      uint8_t op, code;
    };

    std::set<Expr> updates;

    template<unsigned OPSIZE>
    typename TypeFor<Core,OPSIZE>::u
    memread( unsigned seg, addr_t const& addr )
    {
      typedef typename TypeFor<Core,OPSIZE>::u u_type;
      typedef typename u_type::value_type uval_type;
      return u_type( Expr( new Load<uval_type>( OPSIZE/8, seg, addr.expr ) ) );
    }

    template <unsigned OPSIZE>
    void
    memwrite( unsigned seg, addr_t const& addr, typename TypeFor<Core,OPSIZE>::u val )
    {
      updates.insert( new Store( OPSIZE/8, seg, addr.expr, val.expr ) );
    }

    struct FTop : public CNode
    {
      virtual FTop* Mutate() const { return new FTop(*this); }
      virtual unsigned SubCount() const { return 0; }
      virtual int cmp(ExprNode const&) const override { return 0; }
      virtual ValueType GetType() const { return unisim::util::symbolic::CValueType(uint8_t()); }
      virtual void Repr( std::ostream& sink ) const;
      virtual void translate( SrcMgr& srcmgr, CCode& ccode ) const;
    };

    struct FTopWrite : public RegWriteBase
    {
      FTopWrite( Expr const& ftop ) : RegWriteBase( ftop ) {}
      virtual FTopWrite* Mutate() const override { return new FTopWrite(*this); }
      virtual void GetRegName(std::ostream& sink) const override;
      virtual void translate( SrcMgr& srcmgr, CCode& ccode ) const override;
      virtual int cmp( ExprNode const& ) const override { return 0; }
    };

    Expr                        ftop_source;
    unsigned                    ftop;
    static u8_t                 ftop_update( Expr const& ftopsrc, unsigned ftop ) { if (ftop == 0) return ftopsrc; return (u8_t(ftopsrc) + u8_t(ftop)) & u8_t(7); }
    u8_t                        ftopread() { return ftop_update( ftop_source, ftop ); }

    Expr                        fpuregs[8];
    void                        fpush( f64_t value ) { ftop = (ftop+8-1) % 8; fpuregs[ftop] = value.expr; }
    void                        fwrite( unsigned idx, f64_t value ) { fpuregs[(ftop + idx) % 8] = value.expr; }
    f64_t                       fpop() { f64_t res( fpuregs[ftop] ); ftop = (ftop+1) % 8; return res; }
    f64_t                       fread( unsigned idx ) { return f64_t(fpuregs[(ftop + idx) % 8]); }

    void                        fmemwrite32( unsigned seg, addr_t const& addr, f32_t val ) { updates.insert( new Store(  4, seg, addr.expr, val.expr ) ); }
    void                        fmemwrite64( unsigned seg, addr_t const& addr, f64_t val ) { updates.insert( new Store(  8, seg, addr.expr, val.expr ) ); }
    void                        fmemwrite80( unsigned seg, addr_t const& addr, f80_t val ) { updates.insert( new Store( 10, seg, addr.expr, val.expr ) ); }

    f32_t                       fmemread32( unsigned seg, addr_t const& addr ) { return f32_t( Expr( new Load<float>( 4, seg, addr.expr ) ) ); }
    f64_t                       fmemread64( unsigned seg, addr_t const& addr ) { return f64_t( Expr( new Load<double>(  8, seg, addr.expr ) ) ); }
    f80_t                       fmemread80( unsigned seg, addr_t const& addr ) { return f80_t( Expr( new Load<long double>( 10, seg, addr.expr ) ) ); }

    // template <unsigned OPSIZE>
    // typename TypeFor<Core,OPSIZE>::u
    // regread( unsigned idx )
    // {
    //   typedef typename TypeFor<Core,OPSIZE>::u u_type;

    //   if (OPSIZE==8)                    return u_type( eregread( idx%4, 1, (idx>>2) & 1 ) );
    //   if ((OPSIZE==16) or (OPSIZE==32)) return u_type( eregread( idx, OPSIZE/8, 0 ) );
    //   throw 0;
    //   return u_type(0);
    // }

    // template <unsigned OPSIZE>
    // void
    // regwrite( unsigned idx, typename TypeFor<Core,OPSIZE>::u const& value )
    // {
    //   if  (OPSIZE==8)                   return eregwrite( idx%4, 1, (idx>>2) & 1, value.expr );
    //   if ((OPSIZE==16) or (OPSIZE==32)) return eregwrite( idx, OPSIZE/8, 0, value.expr );
    //   throw 0;
    // }

    template <unsigned OPSIZE>
    typename TypeFor<Core,OPSIZE>::u
    pop()
    {
      // TODO: handle stack address size
      u32_t sptr = regread( GOd(), 4 );
      regwrite( GOd(), 4, sptr + u32_t( OPSIZE/8 ) );
      return memread<OPSIZE>( unisim::component::cxx::processor::intel::SS, sptr );
    }

    template <unsigned OPSIZE>
    void
    push( typename TypeFor<Core,OPSIZE>::u const& value )
    {
      // TODO: handle stack address size
      u32_t sptr = regread( GOd(), 4 ) - u32_t( OPSIZE/8 );
      memwrite<OPSIZE>( unisim::component::cxx::processor::intel::SS, sptr, value );
      regwrite( GOd(), 4, sptr );
    }

    void shrink_stack( addr_t const& offset ) { regwrite( GOd(), 4, regread( GOd(), 4 ) + offset ); }
    void grow_stack( addr_t const& offset ) { regwrite( GOd(), 4, regread( GOd(), 4 ) - offset ); }

    template <class GOP>
    typename TypeFor<Core,GOP::SIZE>::u
    rmread( GOP const& g, RMOp const& rmop )
    {
      if (not rmop.ismem())
        return regread( g, rmop.ereg() );

      return memread<GOP::SIZE>( rmop->segment, rmop->effective_address( *this ) );
    }

    template <class GOP>
    void
    rmwrite( GOP const& g, RMOp const& rmop, typename TypeFor<Core,GOP::SIZE>::u const& value )
    {
      if (not rmop.ismem())
        return regwrite( g, rmop.ereg(), value );

      return memwrite<GOP::SIZE>( rmop->segment, rmop->effective_address( *this ), value );
    }

    template <unsigned OPSIZE>
    typename TypeFor<Core,OPSIZE>::f
    fpmemread( unsigned seg, addr_t const& addr )
    {
      typedef typename TypeFor<Core,OPSIZE>::f f_type;
      typedef typename f_type::value_type f_ctype;
      return f_type( Expr( new Load<f_ctype>( OPSIZE/8, seg, addr.expr ) ) );
    }

    template <unsigned OPSIZE>
    typename TypeFor<Core,OPSIZE>::f
    frmread( RMOp const& rmop )
    {
      typedef typename TypeFor<Core,OPSIZE>::f f_type;
      if (not rmop.ismem()) return f_type( fread( rmop.ereg() ) );
      return this->fpmemread<OPSIZE>( rmop->segment, rmop->effective_address( *this ) );
    }

    template <unsigned OPSIZE>
    void
    fpmemwrite( unsigned seg, addr_t const& addr, typename TypeFor<Core,OPSIZE>::f const& value )
    {
      updates.insert( new Store( OPSIZE/8, seg, addr.expr, value.expr ) );
    }

    template <unsigned OPSIZE>
    void
    frmwrite( RMOp const& rmop, typename TypeFor<Core,OPSIZE>::f const& value )
    {
      if (not rmop.ismem()) return fwrite( rmop.ereg(), f64_t( value ) );
      fpmemwrite<OPSIZE>( rmop->segment, rmop->effective_address( *this ), value );
    }

    struct MXCSR
      : public unisim::util::symbolic::WithValueType<FLAG>
    {
      typedef uint16_t value_type;
      void GetName(std::ostream&, bool) const;
      int cmp(MXCSR const&) const { return 0; }
    };

    u32_t mxcsr;

    static unsigned const VREGCOUNT = 8;

    template <class VALUE>
    VALUE vmm_memread( unsigned seg, addr_t const& addr, VALUE const& )
    {
      throw 0;
      return VALUE();
    }

    template <class VALUE>
    void vmm_memwrite( unsigned seg, addr_t const& addr, VALUE const& )
    {
      throw 0;
    }

    template <class VR, class VALUE>
    VALUE vmm_read( VR const&, RMOp const& rm, unsigned sub, VALUE const& )
    {
      throw 0;
      return VALUE();
    }

    template <class VR, class VALUE>
    VALUE vmm_read( VR const&, unsigned gn, u8_t const& sub, VALUE const& )
    {
      throw 0;
      return VALUE();
    }

    template <class VR, class VALUE>
    VALUE vmm_read( VR const&, unsigned gn, unsigned sub, VALUE const& )
    {
      throw 0;
      return VALUE();
    }

    template <class VR, class VALUE>
    void vmm_write( VR const&, RMOp const& rm, unsigned sub, VALUE const& )
    {
      throw 0;
    }

    template <class VR, class VALUE>
    void vmm_write( VR const&, unsigned gn, unsigned sub, VALUE const& )
    {
      throw 0;
    }

    template<unsigned OPSIZE>
    typename TypeFor<Core,OPSIZE>::u
    xmm_uread( unsigned reg, unsigned sub )
    {
      throw 0;
      return typename TypeFor<Core,OPSIZE>::u( 0 );
    }

    template<unsigned OPSIZE>
    void
    xmm_uwrite( unsigned reg, unsigned sub, typename TypeFor<Core,OPSIZE>::u const& val )
    {
      throw 0;
    }

    template<unsigned OPSIZE>
    typename TypeFor<Core,OPSIZE>::u
    xmm_uread( RMOp const& rmop, unsigned sub )
    {
      if (not rmop.ismem()) return xmm_uread<OPSIZE>( rmop.ereg(), sub );
      return memread<OPSIZE>( rmop->segment, rmop->effective_address( *this ) + u32_t(sub*OPSIZE/8) );
    }

    template<unsigned OPSIZE>
    void
    xmm_uwrite( RMOp const& rmop, unsigned sub, typename TypeFor<Core,OPSIZE>::u const& val )
    {
      if (not rmop.ismem()) return xmm_uwrite<OPSIZE>( rmop.ereg(), sub, val );
      return memwrite<OPSIZE>( rmop->segment, rmop->effective_address( *this ) + u32_t(sub*OPSIZE/8), val );
    }

    template <unsigned OPSIZE>
    typename TypeFor<Core,OPSIZE>::f
    xmm_fread( unsigned reg, unsigned sub )
    {
      throw 0;
      return typename TypeFor<Core,OPSIZE>::f( 0 );
    }

    template <unsigned OPSIZE>
    void
    xmm_fwrite( unsigned reg, unsigned sub, typename TypeFor<Core,OPSIZE>::f const& val )
    {
      throw 0;
    }

    template <unsigned OPSIZE>
    typename TypeFor<Core,OPSIZE>::f
    xmm_fread( RMOp const& rmop, unsigned sub )
    {
      if (not rmop.ismem()) return xmm_fread<OPSIZE>( rmop.ereg(), sub );
      return fpmemread<OPSIZE>( rmop->segment, rmop->effective_address( *this ) + addr_t(sub*OPSIZE/8) );
    }

    template <unsigned OPSIZE>
    void
    xmm_fwrite( RMOp const& rmop, unsigned sub, typename TypeFor<Core,OPSIZE>::f const& val )
    {
      if (not rmop.ismem()) return xmm_fwrite<OPSIZE>( rmop.ereg(), sub, val );
      fpmemwrite<OPSIZE>( rmop->segment, rmop->effective_address( *this ) + addr_t(sub*OPSIZE/8), val );
    }

    void     interrupt( unsigned op, unsigned code )
    {
      updates.insert( new Interrupt( op, code ) );
    }
    void     syscall()                   { throw 0; }
    void     cpuid()                     { throw 0; }
    void     xgetbv()                    { throw 0; }
    u32_t    mxcsread()                  { throw 0; return u32_t(); };
    void     mxcswrite( u32_t const& )   { throw 0; }
    void     unimplemented()             { throw 0; }
    void     noexec( Operation const& op );
    void     stop() { throw 0; }
    void     _DE();

    void xsave(unisim::component::cxx::processor::intel::XSaveMode, bool, u64_t, RMOp const&) { throw 0; }
    void xrstor(unisim::component::cxx::processor::intel::XSaveMode, bool, u64_t, RMOp const&) { throw 0; }
    // bool Cond( bit_t b ) { return false; }

    template <typename T>
    bool Test( unisim::util::symbolic::SmartValue<T> const& cond )
    {
      return concretize(bit_t(cond).expr);
    }
    bool concretize(Expr cond);

    void
    step( Operation const* op )
    {
      setnip( getnip() + u32_t(op->length) );
      op->execute( *this );
    }

    static char const* ctxvar;
  };

  void eval_div( Core& arch, Core::u64_t& hi, Core::u64_t& lo, Core::u64_t const& divisor );
  void eval_div( Core& arch, Core::s64_t& hi, Core::s64_t& lo, Core::s64_t const& divisor );
  void eval_mul( Core& arch, Core::u64_t& hi, Core::u64_t& lo, Core::u64_t const& multiplier );
  void eval_mul( Core& arch, Core::s64_t& hi, Core::s64_t& lo, Core::s64_t const& multiplier );

} /* end of namespace UWT */

namespace unisim {
namespace component {
namespace cxx {
namespace processor {
namespace intel {
  double sine( double );
  double cosine( double );
  double tangent( double );
  double arctan( double angle );
  double logarithm( double );
  double square_root( double );

  template <typename FTP>
  FTP firound( FTP const& src, int x87frnd_mode ) { return FTP( unisim::util::symbolic::Expr(new UWT::FIRound<typename FTP::value_type>( src.expr, x87frnd_mode )) ); }

} /* end of namespace intel */
} /* end of namespace processor */
} /* end of namespace cxx */
} /* end of namespace component */
namespace util {
namespace symbolic {

} /* end of namespace symbolic */
} /* end of namespace util */
} /* end of namespace unisim */

#endif // UWT_CORE_HH

