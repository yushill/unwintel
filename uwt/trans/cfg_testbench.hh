/***************************************************************************
                          uwt/trans/cfg_testbench.hh
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

/** @file uwt/trans/cfg_testbench.hh
    @brief  header file for translation engine

*/

#ifndef UWT_CFG_TESTBENCH_HH
#define UWT_CFG_TESTBENCH_HH

#include <uwt/trans/cfg.hh>
#include <uwt/fwd.hh>
#include <inttypes.h>

namespace UWT
{
  struct GraphMatch;

  struct CFG_TestBench : public CFG
  {
    CFG_TestBench(uintptr_t _test_idx) : test_idx(_test_idx) {}

    struct Seed
    {
      virtual void nextwave() = 0;
      virtual unsigned choose(unsigned) = 0;
      virtual ~Seed() {}
    };
    void generate( Seed& seed, unsigned size );

    void setroot(IBloc* ib);

    template<typename _InputIterator,
             typename = std::_RequireInputIter<_InputIterator>>
    void
    import(_InputIterator beg, _InputIterator end)
    {
      if (bloc_pool.size()) { struct IntErr {}; throw IntErr(); }
      bloc_pool.assign(beg, end);
      setroot(bloc_pool[0]);
    }


    void graphit( std::ostream& sink );

    virtual std::string graph_name() const override;

    static void test( uintptr_t count );

    static IBloc* newIBloc();

  private:
    uintptr_t test_idx;
  };

  struct Comprehensive : public CFG_TestBench::Seed
  {
    struct Choice
    {
      Choice(Choice* _tail, unsigned _count) : tail(_tail), head(0), count(_count), value(0) {}
      bool increment() { return ++value < count; }
      Choice* tail;
      Choice* head;
      unsigned count, value;
    };

    struct Stop {};

    void nextwave();

    unsigned choose( unsigned count )
    {
      if (not base.tail->head)
        base.tail->head = new Choice(base.tail,count);

      Choice* choice = base.tail = base.tail->head;

      if (choice->count != count) throw 0;

      return choice->value;
    }

    Comprehensive() : base(&base,2) {}

    Choice base;
  };

  struct Random : public CFG_TestBench::Seed
  {
    uint64_t seed;
    Random() : seed(0) {}
    unsigned choose( unsigned count ) { return mrand48() % count; }
    void nextwave() { static uintptr_t seed = 0; srand48(seed++); }
  };

} // end of namespace UWT

#endif // UWT_CFG_TESTBENCH_HH
