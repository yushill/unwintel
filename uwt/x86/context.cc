/***************************************************************************
                             uwt/x86/context.cc
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

/** @file uwt/x86/context.cc
    @brief  implementation of context

*/

#include <uwt/x86/context.hh>
#include <uwt/x86/memory.hh>
#include <uwt/x86/calldb.hh>
#include <uwt/utils/logchannels.hh>
#include <uwt/utils/misc.hh>
#include <iostream>
#include <stdexcept>
#include <cassert>

#define FIXME_FMT( FMT, ... ) strx::fmt( "%s:%d: (%s): " FMT, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__ ).c_str()
#define FIXME_STR( FMT ) strx::fmt( "%s:%d: (%s): " FMT, __FILE__, __LINE__, __PRETTY_FUNCTION__ ).c_str()

namespace UWT
{
  Context::Context( Memory& _memory, CallDB& _calldb, WinNT& _os )
    : EIP( 0 ), EFLAGS( 0x2 ),
      FPUControl( 0x37f ), FPUStatus( 0 ), FPUTag( 0xffff ), FPUOpcode( 0 ), FPUIP( 0 ), FPUOP( 0 ),
      mem( _memory ), CS( 0x0b ), DS( 0x0b ), SS( 0x0b ), ES( 0x0b ), FS( 0x0b ), GS( 0x0b ),
      calldb( _calldb ), os( _os ), m_state( Running )
  {
    /*** x86 state init ***/
    G.D.EAX = 0; G.D.ECX = 0; G.D.EDX = 0; G.D.EBX = 0;
    G.D.ESP = 0; G.D.EBP = 0; G.D.ESI = 0; G.D.EDI = 0;
    for( int32_t idx = 0; idx < 8; idx++ )
      FPUR[idx] = 0;

    /*** Non preemptive scheduling context init ***/
    // Initializing rotating mutexes
    for( int idx = 0; idx < s_turns; idx++ ) {
      pthread_mutex_init( &m_rotmutex[idx], 0 );
      this->lock( idx, 0 );
    }

    // Launching side thread
    {
      pthread_attr_t attr;
      pthread_attr_init( &attr );
      pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );
      m_turn = 0;
      int status = pthread_create( &m_callee, &attr, Context::run, (void*)this );
      pthread_attr_destroy(&attr);
      if (status != 0)
        throw std::runtime_error("internal error");
    }
  }

  Context::~Context() noexcept(false)
  {
    if (m_state == Running)
      throw std::logic_error("internal error");
    for( int idx = 0; idx < s_turns; idx++ )
      pthread_mutex_destroy( &m_rotmutex[idx] );
  }

  void
  Context::lock( unsigned int _idx, int /*_thr*/ )
  {
    unsigned int idx = _idx % s_turns;
    //std::cerr << "T" << _thr << ": L" << idx << " ?\n";
    pthread_mutex_lock( &m_rotmutex[idx] );
    //std::cerr << "T" << _thr << ": L" << idx << " !\n";
  }

  void
  Context::unlock( unsigned int _idx, int /*_thr*/ )
  {
    unsigned int idx = _idx % s_turns;
    //std::cerr << "T" << _thr << ": L" << idx << " ?\n";
    pthread_mutex_unlock( &m_rotmutex[idx] );
    //std::cerr << "T" << _thr << ": L" << idx << " !\n";
  }

  Context::State
  Context::cont()
  {
    /* Non-preemptive stepping method */
    // We save the current turn before any side thread modification
    unsigned int turn = m_turn;
    this->unlock( turn + 1, 0 ); // Side continue signal
    this->lock( turn, 0 );       // Self stop signal
    // return to main program appropriately
    if (m_state != Running) {
      // waiting for side termination
      pthread_join( m_callee, 0 );
      // releasing all locks
      this->unlock( 0, 0 );
      this->unlock( 1, 0 );
      this->unlock( 2, 0 );
    }
    return m_state;
  }

  void*
  Context::run( void* _obj )
  {
    /* Side method */
    Context& ctx = *(static_cast<Context*>( _obj ));
    assert( ctx.m_turn == 0 );

    // Initializing rotating mutex
    ctx.lock( 1, 1 ); // Self stop signal
    try {
      // Running transmulations
      ctx.call( ctx.EIP )( ctx );
    }
    catch( CallDB::TransRequest req ) {
      ctx.EIP = req.m_addr;
      ctx.m_state = CallException;
    }
    catch (Stop const&) {
      ctx.m_state = Exited;
      /* pass */
    }
    catch (...) {
      std::cerr << "Error: erroneous behaviour (unhandled exception in child thread)" << std::endl;
      ctx.m_state = FatalError;
    }

    if (ctx.m_state == Running) {
      std::cerr << "Error: returning to inexistent parent context" << std::endl;
      ctx.m_state = FatalError;
    }

    // Terminating child thread.
    unsigned int turn = (ctx.m_turn + 1) % s_turns; // next turn
    ctx.unlock( turn + 2, 1 ); // Main continue signal
    pthread_exit( 0 );
    return 0;
  }

  void
  Context::yield()
  {
    /* Side method */
    m_turn = (m_turn+1) % s_turns; // Next turn
    this->unlock( m_turn + 2, 1 ); // Main continue signal
    this->lock( m_turn + 1, 1 );   // Self stop signal
    // Continue ?
    if (m_state != Running)
      throw Stop();
  };

  void
  Context::interrupt( uint8_t _opcode, uint8_t _vector )
  {
    switch (_opcode)
      {
      case 0xcc: // int3     : (3) trap to debugger
      case 0xce: // intO     : (4) trap to ovorflow handler
      case 0xcd: // int imm8 : (n) trap user
        std::cerr << FIXME_FMT( "not implemented interruption code=%d\n", uint32_t( _opcode ) );
        m_state = FatalError;
        throw Stop();
        break;
      default:
        assert( false );
        break;
      }
  }

  void
  Context::fpuload32p( uint32_t _addr )
  {
    uint32_t ntop = ((FPUStatus >> 11)-1) & 0x7;
    FPUStatus &= ~(0x7<<11);
    FPUStatus |= ntop << 11;
    FPUTag &= ~(0x3 << (ntop*2));

    union { float f; uint32_t u; } word;
    word.u = 0;
    uint8_t buf[4];
    mem.read( _addr, buf, 4 );
    for( int idx = 4; (--idx) >= 0; ) word.u = (word.u << 8) | buf[idx];
    FPUR[ntop] = double( word.f );
  }

  void
  Context::fpuload64p( uint32_t _addr )
  {
    uint32_t ntop = ((FPUStatus >> 11)-1) & 0x7;
    FPUStatus &= ~(0x7<<11);
    FPUStatus |= ntop << 11;
    FPUTag &= ~(0x3 << (ntop*2));

    union { double d; uint64_t u; } word;
    word.u = 0;
    uint8_t buf[8];
    mem.read( _addr, buf, 8 );
    for( int idx = 8; (--idx) >= 0; ) word.u = (word.u << 8) | buf[idx];
    FPUR[ntop] = double( word.d );
  }

  void
  Context::fpuload80p( uint32_t _addr )
  {
    std::cerr << FIXME_STR( "Unimplemented." );
    throw Stop();
  }

  void
  Context::fpustore32p( uint32_t _addr )
  {
    uint32_t top = (FPUStatus >> 11) & 0x7;
    union { float f; uint32_t u; } word;
    word.f = float( FPUR[top] );

    uint8_t buf[4];
    for( int idx = -1; (++idx) < 4; ) { buf[idx] = (word.u >> (idx*8)) & 0xff; }
    mem.write( _addr, buf, 4 );
    FPUStatus &= ~(0x7<<11);
    FPUStatus |= ((top+1)&0x7) << 11;
  }

  void
  Context::fpustore64p( uint32_t _addr )
  {
    uint32_t top = (FPUStatus >> 11) & 0x7;
    union { float f; uint32_t u; } word;
    word.f = float( FPUR[top] );

    uint8_t buf[8];
    for( int idx = -1; (++idx) < 8; ) { buf[idx] = (word.u >> (idx*8)) & 0xff; }
    mem.write( _addr, buf, 8 );
    FPUStatus &= ~(0x7<<11);
    FPUStatus |= ((top+1)&0x7) << 11;
  }

  double
  Context::rfpum32( uint32_t _addr )
  {
    union { float f; uint32_t u; } word;
    word.u = 0;
    uint8_t buf[4];
    mem.read( _addr, buf, 4 );
    for( int idx = 4; (--idx) >= 0; ) word.u = (word.u << 8) | buf[idx];
    return double( word.f );
  }

  double
  Context::rfpum64( uint32_t _addr )
  {
    union { double d; uint64_t u; } word;
    word.u = 0;
    uint8_t buf[8];
    mem.read( _addr, buf, 8 );
    for( int idx = 8; (--idx) >= 0; ) word.u = (word.u << 8) | buf[idx];
    return double( word.d );
  }

  void
  Context::fpustore80p( uint32_t _addr )
  {
    std::cerr << FIXME_STR( "Unimplemented." );
    throw Stop();
  }

  double&
  Context::wfpust( int32_t _idx )
  {
    _idx = (((FPUStatus >> 11) + _idx) & 0x07);
    FPUTag &= ~(0x3 << (_idx*2));
    return FPUR[_idx];
  }

  hostcode_t
  Context::call( uint32_t _addr )
  {
    CallDB::Entry& cdbentry = this->calldb[_addr];
    //uint32_t retaddr = mem.r<4>( G.D.ESP );
    //tprinf( cerr, "Call to: %#x with: (%%esp) = %#x\n", cdbentry.m_ip, retaddr );
    return cdbentry.m_hostcode;
  }

  Ctrl32&
  Ctrl32::operator=( uint32_t _value )
  {
    return *this;
  }

  Ctrl32::operator uint32_t()
  {
    return m_ctx.*m_reg;
  }

  Ctrl16&
  Ctrl16::operator=( uint16_t _value )
  {
    if( m_reg == &Context::FPUControl ) {
      if( ((_value >> 0)  & 1) != 1 ) std::cerr << "FIXME: Unsupported field value for fpu control word (IM)" << std::endl;
      if( ((_value >> 1)  & 1) != 1 ) std::cerr << "FIXME: Unsupported field value for fpu control word (DM)" << std::endl;
      if( ((_value >> 2)  & 1) != 1 ) std::cerr << "FIXME: Unsupported field value for fpu control word (ZM)" << std::endl;
      if( ((_value >> 3)  & 1) != 1 ) std::cerr << "FIXME: Unsupported field value for fpu control word (OM)" << std::endl;
      if( ((_value >> 4)  & 1) != 1 ) std::cerr << "FIXME: Unsupported field value for fpu control word (UM)" << std::endl;
      if( ((_value >> 5)  & 1) != 1 ) std::cerr << "FIXME: Unsupported field value for fpu control word (PM)" << std::endl;
      if( ((_value >> 8)  & 3) != 2 ) std::cerr << "FIXME: " << strx::fmt( "Unsupported fpu precision (PC=%d).", ((_value >> 8)  & 3) ) << std::endl;
      if( ((_value >> 10) & 3) != 0 ) std::cerr << "FIXME: Unsupported field value for fpu control word (RC)" << std::endl;
      if( ((_value >> 11) & 1) != 0 ) std::cerr << "FIXME: Unsupported field value for fpu control word (X)" << std::endl;
      m_ctx.FPUControl = _value;
    } else if( m_reg == &Context::CS or m_reg == &Context::DS or m_reg == &Context::SS or
               m_reg == &Context::ES or m_reg == &Context::FS or m_reg == &Context::GS ) {
//       cerr << "SD <- {"
//            << "Index:" << ((_value>>3)&0x1fff) << ','
//            << "TI:" << ((_value>>2)&0x1) << ','
//            << "RPL:" << ((_value>>0)&0x3)
//            << "}\n";
      assert( _value == 0x0b );
      m_ctx.*m_reg = _value;
    } else if( m_reg == &Context::FPUTag ) {
      m_ctx.FPUTag = _value;
    } else {
      assert( false );
    }
    return *this;
  }

  Ctrl16::operator uint16_t()
  {
    if( m_reg == &Context::FPUTag ) {
      assert( false );
    }
    return m_ctx.*m_reg;
  }

};
