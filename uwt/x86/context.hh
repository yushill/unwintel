/***************************************************************************
                             uwt/x86/context.hh
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

/** @file uwt/x86/context.hh
    @brief  header file for context structs

*/

#ifndef UWT_CONTEXT_HH
#define UWT_CONTEXT_HH

#include <uwt/fwd.hh>
#include <iosfwd>
#include <pthread.h>
#include <inttypes.h>

namespace UWT {
  union GRegs {
    struct DRegs {
      uint32_t EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI;
    } D;
#if __BYTE_ORDER == __BIG_ENDIAN
    struct WRegs {
      uint16_t t0, AX, t1, CX, t2, DX, t3, BX, t4, SP, t5, BP, t6, SI, t7, DI;
    } W;
    struct BRegs {
      uint8_t  t1, t0, AH, AL, t3, t2, CH, CL, t5, t4, DH, DL, t7, t6, BH, BL;
      uint8_t  tb, ta, t9, t8, tf, te, td, tc, tj, ti, th, tg, tn, tm, tl, tk;
    } B;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    struct WRegs {
      uint16_t AX, t0, CX, t1, DX, t2, BX, t3, SP, t4, BP, t5, SI, t6, DI, t7;
    } W;
    struct BRegs {
      uint8_t  AL, AH, t0, t1, CL, CH, t2, t3, DL, DH, t4, t5, BL, BH, t6, t7;
      uint8_t  t8, t9, ta, tb, tc, td, te, tf, tg, th, ti, tj, tk, tl, tm, tn;
    } B;
#endif
  };

  template <class UINT,uint32_t t_SIZE>
  struct BinField
  {
    UINT&               m_word;
    uint32_t            m_pos;

    BinField( UINT& _word, uint32_t _pos )
      : m_word( _word ), m_pos( _pos ) {}

    operator            uint32_t() const { return (m_word >> m_pos) & ((1 << t_SIZE)-1); }
    void                operator = ( uint32_t _newbits ) {
      uint32_t oldbits = (m_word >> m_pos) & ((1 << t_SIZE)-1);
      m_word ^= ((oldbits ^ _newbits) << m_pos);
    }
  };

  struct Context
  {
    struct Stop {};

    // Architectural State (x86 basics)
    uint32_t                EIP; ///< Extended Instrution Pointer
    GRegs                   G;   ///< Basic Integer Registers

    // Eflags
    uint32_t                EFLAGS;
    enum eflags_sh_t { CF = 0, PF = 2, AF = 4, ZF = 6, SF = 7, DF = 10, OF = 11 };
    BinField<uint32_t,1>    weflag( eflags_sh_t _flag ) { return BinField<uint32_t,1>( EFLAGS, _flag ); }
    bool                    reflag( eflags_sh_t _flag ) { return ((EFLAGS >> _flag) & 0x01) != 0; }

    // x87 Architectural State...
    double                  FPUR[8];
    uint16_t                FPUControl;
    uint16_t                FPUStatus;
    uint16_t                FPUTag;
    uint16_t                FPUOpcode;
    uint32_t                FPUIP;
    uint32_t                FPUOP;

    enum fpuflags_sh_t
      {
        FPUIE = 0, FPUDE = 1, FPUZE = 2, FPUOE = 3, FPUUE = 4, FPUPE = 5, FPUSF = 6, FPUES = 7,
        FPUC0 = 8, FPUC1 = 9, FPUC2 = 10, FPUC3 = 14, FPUB  = 15
      };

    unsigned int            fputop() { return (FPUStatus >> 11) & 0x07; }
    void                    fputop( unsigned int _top ) { FPUStatus &= ~(0x07<<11); FPUStatus |= (_top & 0x07) << 11; }
    void                    fpuinctop() { uint32_t ntop = ((FPUStatus >> 11)+1) & 0x7; FPUStatus &= ~(0x07<<11); FPUStatus |= ntop << 11; }
    void                    fpudectop() { uint32_t ntop = ((FPUStatus >> 11)-1) & 0x7; FPUStatus &= ~(0x07<<11); FPUStatus |= ntop << 11; }
    double                  rfpust( int32_t _idx ) { return FPUR[(((FPUStatus >> 11) + _idx) & 0x07)]; }
    double&                 wfpust( int32_t _idx );
    void                    wfpuabs( unsigned reg, double value ) { FPUR[reg] = value; }
    bool                    rfpuflag( fpuflags_sh_t _flag ) { return ((FPUStatus >> _flag) & 0x01) != 0; }
    BinField<uint16_t,1>    wfpuflag( fpuflags_sh_t _flag ) { return BinField<uint16_t,1>( FPUStatus, _flag ); }
    void                    fpuload32p( uint32_t _addr );
    void                    fpuload64p( uint32_t _addr );
    void                    fpuload80p( uint32_t _addr );
    void                    fpustore32p( uint32_t _addr );
    void                    fpustore64p( uint32_t _addr );
    void                    fpustore80p( uint32_t _addr );
    double                  rfpum32( uint32_t _addr );
    double                  rfpum64( uint32_t _addr );

    // Exception interface
    void                    interrupt( uint8_t _opcode, uint8_t _vector );

    // Link to memory
    Memory&               mem;
    uint16_t                CS, DS, SS, ES, FS, GS; // Segment selectors

    // Functions map
    CallDB&                 calldb;
    hostcode_t              call( uint32_t _addr );

    // Operating System
    WinNT&                  os;

    // Context State
    enum exception_t { IncoherentEIP, };

    Context( Memory& _memory, CallDB& _calldb, WinNT& _os );
    virtual ~Context() noexcept(false);

    /*****************************************/
    /*** Non preemptive scheduling context ***/
    /*****************************************/
  public:
    enum State { Running, Exited, CallException, FatalError, };

    /* scheduler method */
    State               cont();

    /* schedulee method */
    void                  exit() { throw Stop(); }
    void                  yield();

  private:
    void                  lock( unsigned int _idx, int /*_thr*/ );
    void                  unlock( unsigned int _idx, int /*_thr*/ );

    static void*          run( void* _obj );

    static uint8_t const  s_turns = 3;

    State               m_state;
    pthread_t             m_callee;
    pthread_mutex_t       m_rotmutex[s_turns];
    uint8_t               m_turn; // [0,1,2]
    bool                  m_waitstop;
  };

  struct Ctrl32 {
    Context&              m_ctx;
    uint32_t Context::*   m_reg;

    Ctrl32( Context& _ctx, uint32_t Context::* _reg ) : m_ctx( _ctx ), m_reg( _reg ) {}
    Ctrl32&                 operator=( uint32_t _value );
    operator                uint32_t();
  };

  struct Ctrl16 {
    Context&              m_ctx;
    uint16_t Context::*   m_reg;

    Ctrl16( Context& _ctx, uint16_t Context::* _reg ) : m_ctx( _ctx ), m_reg( _reg ) {}
    Ctrl16&                 operator=( uint16_t _value );
    operator                uint16_t();
  };
};

#endif // UWT_CONTEXT_HH
