/***************************************************************************
                          uwt/trans/cfg.cc
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

/** @file uwt/trans/cfg.cc
    @brief  implementation file for translation engine

*/

#include <uwt/trans/cfg.hh>
#include <uwt/trans/ibloc.hh>
#include <uwt/trans/ctrlstmt.hh>
#include <uwt/trans/statement.hh>
#include <uwt/utils/logchannels.hh>
#include <uwt/utils/misc.hh>
#include <uwt/launchbox.hh>
#include <unisim/util/symbolic/ccode/ccode.hh>
#include <iostream>
#include <sstream>
#include <set>
#include <vector>
#include <algorithm>
#include <fstream>
#include <cassert>
#include <cstring>

namespace UWT
{
  CFG::~CFG()
  {
    for (IBloc* bloc : bloc_pool)
      bloc->set_exits( IBloc::Exits() );
    IBloc::unref(entry_bloc);
    for (IBloc* ibp : bloc_pool)
      delete ibp;
  }

  struct FunnelSink : public BaseStatement
  {
    typedef unisim::util::symbolic::ccode::Variable Variable;
    typedef unisim::util::symbolic::ccode::Variables Variables;
    typedef unisim::util::symbolic::ccode::SrcMgr SrcMgr;
    FunnelSink( Variable const& _var ) : var(_var), oterm(), inv(false) {}
    void translate(SrcMgr& sink) const override { sink << oterm << " = " << (inv ? "not " : "") << var.GetName() << ";\n"; }
    void SetCondTerm(char const* term, bool _inv) override { oterm = term; inv = _inv; }
    void GetVariables( unisim::util::symbolic::ccode::Variables& vars ) const override { vars.insert( var ); }
    unisim::util::symbolic::ccode::Variable var;
    char const* oterm;
    bool inv;
  };

  struct FunnelSource : public BaseStatement
  {
    typedef unisim::util::symbolic::ccode::Variable Variable;
    typedef unisim::util::symbolic::ccode::Variables Variables;
    typedef unisim::util::symbolic::ccode::SrcMgr SrcMgr;
    FunnelSource( BaseStatement* _prev, Variable const& _var, bool _inv ) : prev(_prev), iterm(), var(_var), inv(_inv) {}
    void translate(SrcMgr& sink) const override
    {
      if (prev) sink << var.GetName() << " = " << (inv ? "not " : "") << iterm << ";\n";
      else      sink << var.GetName() << " = " << (inv ? "true" : "false") << ";\n";
    }
    void SetCondTerm(char const* term, bool _inv) override { prev->SetCondTerm(iterm = term, _inv); inv ^= _inv; }
    BaseStatement* prev;
    char const* iterm;
    Variable const& var;
    bool inv;
  };

  bool
  CFG::collapse()
  {
    /*** collapsing task graph ***/
    struct Iterative
    {
      struct GraphProps
      {
        GraphProps( char const* _title ) : title(_title) {} char const* title;
        virtual ~GraphProps() {}
        struct NodeColor { virtual ~NodeColor() {} virtual void paint( IBloc*, char const* ) const = 0; };
        virtual void node_colors( NodeColor const& nc ) const {};
      };

      struct GMProps : public GraphProps
      {
        GMProps( GraphMatch const& _matched ) : GraphProps(_matched.title), matched(_matched) {} GraphMatch const& matched;
        virtual void node_colors( NodeColor const& nc ) const override
        {
          for (IBloc* bloc : matched.blocs)
            nc.paint( bloc, "red" );

          for (unsigned choice = 0; choice < 2; ++choice)
            if (IBloc* bloc = matched.exits.get(choice))
              nc.paint( bloc, (choice ? "cyan" : "green") );
        }
      };

      void record_trans( GraphProps const& gprops )
      {
        lab.record_trans( gprops.title );
        DebugStream dbg("translation/cfg.dot");
        if (dbg.good())
          {
            generate_graph( dbg, gprops );
          }
      }

      void
      merge_subgraph( GraphMatch const& matched )
      {
        record_trans( GMProps(matched) );

        std::set<IBloc*> obsoletes( matched.blocs.begin(), matched.blocs.end() );

        IBloc* final = matched.blocs.front();

        obsoletes.erase( final );

        // Disconnect subgraph and count statements
        uintptr_t stmt_count = 0;
        for (auto&& bloc : matched.blocs)
          {
            stmt_count += bloc->size();
            bloc->set_exits( IBloc::Exits() );
          }

        final->reserve( stmt_count );

        for (auto&& bloc : matched.blocs)
          { if (bloc != final) final->append( *bloc ); }

        std::vector<IBloc*> new_bloc_pool;

        new_bloc_pool.reserve( lab.bloc_pool.size() - obsoletes.size() );

        for (IBloc* bloc : lab.bloc_pool)
          {
            std::set<IBloc*>::iterator obs = obsoletes.find( bloc );

            if (obs == obsoletes.end())
              { new_bloc_pool.push_back( bloc ); }
            else
              {
                delete bloc;
                obsoletes.erase( obs );
              }
          }

        if (obsoletes.size())
          { throw std::logic_error("Leaking node in obsolete pool"); }

        std::swap( lab.bloc_pool, new_bloc_pool );

        final->set_exits( matched.exits );
      }

      void
      generate_graph( std::ostream& sink, GraphProps const& gprops ) const
      {
        lab.refresh_props();

        sink << "digraph fullgraph {\n  node [ fontname=Helvetica, fontsize=12, penwidth=2 ];\n  edge [ penwidth=2 ];\n";

        struct GPNC : public GraphProps::NodeColor
        {
          GPNC( std::ostream& _sink ) : sink(_sink) {} std::ostream& sink;
          std::string nameof(IBloc* bloc) const { return strx::fmt( "\"n%u[%u]\"", bloc->get_ordering(), bloc->count_predecessors() ); }
          std::string idof(IBloc* bloc) const { return strx::fmt( "\"n%u[%u]\"", bloc->get_ordering(), bloc->count_predecessors() ); }
          virtual void paint( IBloc* bloc, char const* color ) const override
          { sink << "  " << nameof( bloc ) << " [style=filled,fillcolor=" << color << "];\n"; }
        } gfab( sink );

        gfab.paint( lab.entry_bloc, "lightgray" );

        gprops.node_colors( gfab );

        IBloc* first_bloc = lab.bloc_pool[0];
        for (IBloc* bloc : lab.bloc_pool)
          {
            // We have to put the entrypoint at the first place.
            if      (bloc == first_bloc) bloc = lab.entry_bloc;
            else if (bloc == lab.entry_bloc) bloc = first_bloc;

            for (uint32_t jmp = 0; jmp < 2; jmp++)
              {
                IBloc* next = bloc->get_exit(jmp);
                if (not next) continue;
                sink << "  " << gfab.nameof(bloc) << " -> " << gfab.nameof(next) << " [label=" << jmp << "];\n";
              }
          }

        sink << "  \"" << gprops.title << "\" [style=filled,fillcolor=yellow];\n";
        sink << "}\n";
      }

      bool
      step()
      {
        count += 1;
        // Sequential Bloc
        for (IBloc* origin : lab.bloc_pool)
          {
            GraphMatch group( "Seq", ",(,)!", origin );

            if (group.size() == 0) continue;

            merge_subgraph( group ); return true;
          }

        // If Then Bloc
        for (IBloc* origin : lab.bloc_pool)
          {
            GraphMatch group( "IfThen", "?,11", origin );

            if (group.size() == 0) continue;

            std::vector<IBloc*>::iterator itr = group.blocs.begin(), stop = group.blocs.end();

            // If node
            (**itr).last()->SetCondTerm("lcc", false);
            (**itr).append_stmt( ctrl::newIfThenStatement( (**itr).which( *(itr+1) ), "lcc" ) );
            ++itr;

            // Then node
            (**itr).append_stmt( ctrl::newEndIfStatement() );
            ++itr;

            // All node processed
            assert( itr == stop );

            merge_subgraph( group ); return true;
          }

        // If Then Else Bloc
        for (IBloc* origin : lab.bloc_pool)
          {
            GraphMatch group( "IfThenElse", "?,1,1", origin );

            if (group.size() == 0) continue;

            std::vector<IBloc*>::iterator itr = group.blocs.begin(), stop = group.blocs.end();

            // If bloc
            (**itr).last()->SetCondTerm("lcc", false);
            (**itr).append_stmt( ctrl::newIfThenStatement( (**itr).which( *(itr+1) ), "lcc" ) );
            ++itr;

            // Then bloc
            (**itr).append_stmt( ctrl::newElseStatement() );
            ++itr;

            // Else bloc
            (**itr).append_stmt( ctrl::newEndIfStatement() );
            ++itr;

            // All node processed
            assert( itr == stop );

            merge_subgraph( group ); return true;
          }

        // Generic Loop
        for (IBloc* origin : lab.bloc_pool)
          {
            GraphMatch group( "Loop", "*", origin );

            if (group.size() < 1) continue;

            IBloc* head = group.blocs.front();

            if (not group.exits.has( head ))
              continue;

            IBloc* tail = group.exits.get( not group.exits.which( head ) );
            group.exits = IBloc::Exits( tail, 0 );
            IBloc* last = group.blocs.back();

            std::vector<std::string> issues; /* issues */
            head->insert_stmt( ctrl::newSingleStatement("for (;;)\n{\n") );
            issues.push_back( "}\n" );

            for (IBloc* const& bloc : group.blocs)
              {
                bool do_break = tail and bloc->precede( tail );
                bool do_continue = bloc->precede( head );

                if (not bloc->get_exit(true))
                  {
                    /* single exit bloc, next is either continue or break */
                    if      (do_break)    { bloc->append_stmt( ctrl::newSingleStatement( "break;\n" ) ); }
                    else if (do_continue) {
                      if (bloc != last)     bloc->append_stmt( ctrl::newSingleStatement( "continue;\n" ) );
                    }
                    else throw std::logic_error( "Inner loop single exit blocs should 'continue' or 'break'." );
                    bloc->append_stmt( ctrl::newSingleStatement( issues.back().c_str() ) );
                    issues.pop_back();
                  }
                /* fork blocs */
                else if (do_break)
                  {
                    /* Break comes first: if ([not] cond) break [<else continue> or <{}>] */
                    bool alt = bloc->which( tail );
                    bloc->last()->SetCondTerm("lcc", false);
                    bloc->append_stmt( ctrl::newIfStatement( alt, "lcc", "break;\n" ) );
                    if (do_continue)
                      {
                        if (bloc != last)   bloc->append_stmt( ctrl::newSingleStatement( "continue;\n" ) );
                        bloc->append_stmt( ctrl::newSingleStatement( issues.back().c_str() ) );
                        issues.pop_back();
                      }
                  }
                else if (do_continue)
                  {
                    /* Continue comes first (else is implicit) */
                    bool alt = bloc->which( head );
                    bloc->last()->SetCondTerm("lcc", false);
                    bloc->append_stmt( ctrl::newIfStatement( alt, "lcc", "continue;\n" ) );
                  }
                else
                  {
                    bool alt = bloc->which( (&bloc)[1] );
                    bloc->last()->SetCondTerm("lcc", false);
                    bloc->append_stmt( ctrl::newIfStatement( alt, "lcc", "\n{\n" ) );
                    std::string midfix( "}\nelse\n{" );
                    issues.back().insert( 0, "}\n" );
                    issues.push_back( midfix );
                  }
              }
            merge_subgraph( group ); return true;
          }

        // If Then Return Bloc
        for (IBloc* origin : lab.bloc_pool)
          {
            GraphMatch group( "ExitIf", "?.1", origin );

            if (group.size() == 0) continue;

            std::vector<IBloc*>::iterator itr = group.blocs.begin(), stop = group.blocs.end();

            /* If node */
            (**itr).last()->SetCondTerm("lcc", false);
            (**itr).append_stmt( ctrl::newIfThenStatement( (**itr).which( *(itr+1) ), "lcc" ) );
            ++itr;

            /* End-If node */
            (**itr).append_stmt( ctrl::newEndIfStatement() );
            ++itr;

            /* All node processed */
            assert( itr == stop );

            merge_subgraph( group ); return true;
          }

        /* Fix hammocks by funneling 2 exits subgraphs */
        {
          struct IO2
          {
            IO2(Iterative& _itr, GraphMatch& _gmatch) : store(&_itr.lab.bloc_pool[0]), itr(_itr), gmatch(_gmatch), core(0), exit(0), size(_itr.lab.bloc_pool.size()) {}

            IO2(IO2 const& in, IBloc** buf) : store(buf), itr(in.itr), gmatch(in.gmatch), core(in.core), exit(in.exit), size(in.size) { std::copy(&in.store[0],&in.store[size],buf); }

            unsigned admit( unsigned what, unsigned& where ) { std::swap(store[where], store[what]); return where++; }

            void
            update_exit( unsigned target )
            {
              IBloc* nexts[2] = {store[target]->get_exit(0), store[target]->get_exit(1)};
              for (unsigned pivot = exit; pivot < size; ++pivot)
                for (unsigned choice = 0; choice < 2; ++choice)
                  if (store[pivot] == nexts[choice])
                    admit( pivot, exit );
            }

            IBloc* recur() const
            {
              for (unsigned target = core; target < exit; ++target)
                {
                  if (store[target] == itr.lab.entry_bloc)
                    continue; /* Root node cannot be added */

                  /* Step in a new working copy - core <= target < exit - */
                  IBloc* buf[size];
                  IO2 cpy(*this, &buf[0]);
                  cpy.process(target);
                }
              return 0;
            }

            void
            process( unsigned target )
            {
              /* Move target from exit to core, updating exit and admiting
               * any predecessors not already in core.
               */
              unsigned spread = core;
              target = admit( target, spread );
              update_exit( target );

              /* Locate spread predecessors */
              while (spread > core)
                {
                  unsigned pivot = core;

                  for (unsigned tail = 0; tail < size; ++tail)
                    {
                      for (IBloc* head : store[tail]->get_exits())
                        {
                          if (head != store[pivot])
                            continue;
                          if (tail < spread)
                            continue;
                          if (store[tail] == itr.lab.entry_bloc)
                            return; /* Root node cannot be added */
                          if (tail >= exit)
                            tail = admit( tail, exit );
                          tail = admit( tail, spread );
                          update_exit( tail );
                        }
                    }
                  admit( pivot, core );
                }

              /* if subgraph size is already above existing candidate
               * prune recursion */
              if (uintptr_t gmsize = gmatch.blocs.size())
                if (gmsize <= core)
                  return;

              if (not candidate())
                recur();
            }

            bool
            candidate()
            {
              /* [funneling candidates]: exactly 2 exit nodes and more than one node in subgraph */
              if (core < 2 or (exit - core) != 2)
                return false;

              /* [funneling candidates]: exit blocs are targeted by at least 2 subgraph blocs */
              IBloc::Exits graphexits( store[core+0], store[core+1] );
              {
                unsigned exit_preds = 0;
                for (unsigned idx = 0; idx < core; ++idx)
                  if (graphexits.common( store[idx]->get_exits() ))
                    exit_preds += 1;
                if (exit_preds < 2)
                  return false;
              }

              gmatch.blocs.resize(core);
              std::copy(&store[0], &store[core], &gmatch.blocs[0]);
              gmatch.exits = graphexits;

              return true;
            }

            IBloc* run() const
            {
              IBloc* buf[size];
              for (unsigned idx = 0; idx < size; ++idx)
                {
                  IO2 cpy(*this, &buf[0]);
                  cpy.admit(idx,cpy.core);
                  cpy.exit++; /* cpy.admit(0,cpy.exit); */
                  cpy.update_exit(0);
                  if (IBloc* funnel = cpy.recur())
                    return funnel;
                }
              return 0;
            }

            IBloc** store;
            Iterative& itr;
            GraphMatch& gmatch;
            unsigned core, exit, size;
          };

          GraphMatch group( "Funnel" );

          IO2( *this, group ).run();

          if (group.size())
            {
              record_trans( GMProps(group) );

              struct Fnm : public std::ostringstream { Fnm() { static int id = 0; (*this) << "lfc" << id++; } };

              //group.blocs[0]->insert_stmt( fv );

              FunnelSink* fsink = new FunnelSink(Fnm().str());
              IBloc* funnel = new IBloc( fsink );
              funnel->set_exits( group.exits );
              lab.bloc_pool.push_back(funnel);

              for (IBloc* bloc : group.blocs)
                {
                  IBloc::Exits const& blocexits( bloc->get_exits() );

                  if (IBloc* cex = blocexits.common( group.exits ))
                    {
                      bool choice = blocexits.which(cex);
                      bool inverted = group.exits.which(cex) ^ choice;
                      bool dual = blocexits.get(not choice) == group.exits.get(not (inverted ^ choice));
                      bool straight = blocexits.has(0);

                      if (dual)
                        {
                          bloc->last()->SetCondTerm( fsink->var.GetName(), inverted );
                          bloc->set_exits( IBloc::Exits(funnel,0) );
                        }
                      else
                        {
                          BaseStatement* fsrc = straight ?
                            new FunnelSource(            0, fsink->var, inverted ^ choice ) :
                            new FunnelSource( bloc->last(), fsink->var, inverted );
                          bloc->append_stmt( fsrc );
                          bloc->set_exit( choice, funnel );
                        }
                    }
                }

              return true;
            }
        }

        /* Fix multi entry cycles by duplicating cycle node(s) */
        {
          struct IMC
          {
            IMC( Iterative& _itr ) : itr(_itr) {}

            struct Path
            {
              Path(IBloc* _head) : head(_head), tail(_head), m_from(-1) {}
              Path(IBloc* _head, IBloc* _tail, uintptr_t _from, bool alt) : head(_head), tail(_tail), m_from(_from*2+alt) {}
              // bool operator < ( Path const& rhs ) const { return head < rhs.head or (head == rhs.head and tail < rhs.tail); }
              Path* from( std::vector<Path>& paths ) const { return m_from >= 0 ? &paths[m_from / 2] : 0; }
              bool is_head() const { return m_from < 0; }
              bool  alt() const { return m_from % 2; }
              IBloc*   head;
              IBloc*   tail;
              intptr_t m_from;
            };

            struct Edge
            {
              IBloc* origin;
              bool   branch;

              Edge() : origin(0), branch() {}
              Edge( IBloc* _origin, unsigned _branch ) : origin(_origin), branch(_branch) {}
              IBloc* get_exit() const { return origin->get_exit(branch); }
              void set_exit(IBloc* exit) const { return origin->set_exit(branch, exit); }

              int compare( Edge const& update ) const
              {
                if (not origin)
                  return +1;

                IBloc* end0 = get_exit();
                IBloc* end1 = update.get_exit();

                if (int delta = end0->count_successors() - end1->count_successors())
                  return delta;

                if (int delta = end0->count_predecessors() - end1->count_predecessors())
                  return delta;

                if (int delta = origin->count_successors() - update.origin->count_successors())
                  return delta;

                if (int delta = origin->get_ordering() - update.origin->get_ordering())
                  return delta;

                if (int delta = end0->get_ordering() - end1->get_ordering())
                  return delta;

                return int(end0->size() - end1->size());
              }
            };

            struct Solution
            {
              // std::vector<Edge> champions;
              Edge champion;

              void sort() {}
              // {
              //   struct DetCmp
              //   {
              //     bool operator () ( Edge const& a, Edge const& b ) const
              //     {
              //       if (int delta = a.origin->get_ordering() - b.origin->get_ordering())
              //         return (delta < 0);
              //       return a.branch < b.branch;
              //     }
              //   };
              //   std::sort( champions.begin(), champions.end(), DetCmp() );
              // }

              void update( Edge const& rival ) { if (champion.compare(rival) > 0) champion = rival; }
              // {
              //   if (valid())
              //     {
              //       if (int delta = champions.front().compare( rival ))
              //         {
              //           if (delta < 0) return;
              //           champions.clear();
              //         }
              //     }
              //   champions.push_back( rival );
              // }

              // bool valid() const { return champions.size(); }
              bool valid() const { return champion.origin; }
              //Edge const& edge() const { return champions[imccomp.choose(champions.size())]; }
              Edge const& edge() const { return champion; }
              void newcycle() const {}
            };

            void raid( Solution& solution, std::vector<IBloc*> const& blocs, std::set<IBloc*> const& cycle )
            {
              std::set<IBloc*> core, front, spread( {blocs[0]} );

              for (unsigned depth = 0, maxdepth = blocs.size(); ++depth < maxdepth;)
                {
                  std::swap(front, spread);
                  spread.clear();
                  core.insert(front.begin(), front.end());

                  for (IBloc* origin : front)
                    {
                      for (unsigned alt = 0; alt < 2; ++alt)
                        {
                          IBloc* ending = origin->get_exit(alt);
                          if      (not ending) break;
                          else if (core.count( ending ))
                            continue;
                          else if (cycle.count( ending ))
                            solution.update( Edge( origin, alt ) );
                          else
                            spread.insert( ending );
                        }
                    }
                }
            }

            void cycle_dup(std::vector<Path>& core, Path const* p, Edge&& tail, uintptr_t len)
            {
              struct CGP : GraphProps
              {
                CGP( std::vector<Path>& _ptab, Path const* _tail ) : GraphProps("Duplication"), ptab(_ptab), tail(_tail) {}

                virtual void node_colors( NodeColor const& nc ) const override
                {
                  Path const* p = tail;
                  nc.paint(p->tail, "blue");
                  while (not (p = p->from(ptab))->is_head())
                    nc.paint(p->tail, "purple");
                  nc.paint(p->tail, "red");
                }
                std::vector<Path>& ptab;
                Path const* tail;
              };

              itr.record_trans( CGP(core, p) );

              IBloc* duptail = tail.origin;
              for (bool alt; (alt = p->alt()), (p = p->from(core));)
                {
                  IBloc* dup = p->tail->duplicate();
                  itr.lab.bloc_pool.push_back(dup);
                  dup->set_exit(alt,duptail);
                  duptail = dup;
                }
              tail.set_exit(duptail);
              itr.lab.purge();
            }

            bool process()
            {
              std::vector<Path> core(itr.lab.bloc_pool.begin(),itr.lab.bloc_pool.end());
              Solution solution;

              for (uintptr_t spread = 0, len = 0, end = itr.lab.bloc_pool.size(); not solution.valid() and len < end; ++len)
                {
                  uintptr_t front = spread;
                  spread = core.size();

                  for (;front < spread; front += 1)
                    {
                      Path const path = core[front];
                      for (unsigned alt = 0; alt < 2; ++alt)
                        {
                          IBloc* tail = path.tail->get_exit(alt);
                          if      (not tail) break;
                          if      (path.tail == tail)
                            {
                              // Really, shouldn't happen: self-cycle
                              // should have been merged by this time.
                              throw "internal error";
                            }
                          else if (path.head == tail)
                            {
                              // Found a valid cycle ... checking reducibility
                              if (path.head->dominates(path.tail))
                                continue;

                              // // [TRIAL] Whole Cycle dup
                              // return cycle_dup(core, &path, Edge(path.tail, alt), len), true;

                              // [TRIAL] Worst Incoming Edge
                              solution.newcycle();
                              // Locate pathological (cycle) incoming paths
                              std::set<IBloc*> cycle;
                              for (Path const* p = &path; p; p = p->from(core))
                                cycle.insert( p->tail );
                              raid( solution, itr.lab.bloc_pool, cycle );

                              // // [TRIAL] Back edge dup (not working)
                              // solution.update( Edge(path.tail, alt) );
                            }
                          else
                            {
                              if (path.tail->get_ordering() > tail->get_ordering())
                                continue; // Seeking for canonical cycles (with increasing ordering).
                              core.emplace_back( path.head, tail, front, alt );
                            }
                        }
                    }
                }
              solution.sort();

              if (not solution.valid())
                return false;

              Edge const& worst = solution.edge();

              struct EGP : GraphProps
              {
                EGP( Edge const& _edge ) : GraphProps("Duplication"), edge(_edge) {} Edge const& edge;
                virtual void node_colors( NodeColor const& nc ) const override
                {
                  nc.paint(edge.origin,"blue");
                  nc.paint(edge.get_exit(),"red");
                }
              };

              itr.record_trans( EGP(worst) );

              IBloc* dup = worst.get_exit()->duplicate();
              worst.set_exit(dup);
              itr.lab.bloc_pool.push_back(dup);

              return true;
            }

            Iterative& itr;
          } imc( *this );

          lab.refresh_props();
          if (imc.process())
            return true;
        }

        // Something went wrong at least one of the previous merging
        // methods should have worked...
        struct BadGraph : GraphProps { BadGraph() : GraphProps("BadGraph") {} };
        DebugStream sink("err/graph.dot");
        generate_graph( sink, BadGraph() );

        for (IBloc* origin : lab.bloc_pool)
          {
            GraphMatch group( "X", "*", origin );

            if (group.size() < 2) continue;

            DebugStream sink("err/subgraph.dot");
            generate_graph( sink, GMProps(group) );
          }

        return false;
      }

      Iterative( CFG& _lab ) : lab(_lab), count(0) {}

      CFG& lab;
      uintptr_t count;
    };

    uintptr_t minsize = bloc_pool.size();

    for (Iterative iterative( *this ); (bloc_pool.size() > 1) or (entry_bloc->count_predecessors() > 1);)
      {
        if (not iterative.step())
          return false;
        uintptr_t size = bloc_pool.size();
        if      (minsize > size)
          minsize = size;
        else if (5*minsize <= size)
          { std::cerr << "Uncontrolled inflation.\n"; return false; }
      }

    return true;
  }

  void
  CFG::purge()
  {
    auto li = bloc_pool.begin(), end = bloc_pool.end();
    for (auto ri = end; li < ri;)
      {
        if ((**li).count_predecessors() >= 1)
          ++li;
        else
          std::swap(*li, *--ri);
      }
    for (auto di = li; di < end; ++di)
      {
        delete *di;
      }
    bloc_pool.erase(li, end);
  }

  void
  CFG::refresh_props()
  {
    // Refresh ordering and dominance properties, as
    // these properties are systematically broke by graph
    // transformations.

    // refreshing node ordering with reverse post order
    for (auto && bloc : bloc_pool)
      { bloc->set_ordering( -1 ); bloc->set_ipar( 0 ); }

    {
      struct
      {
        unsigned refresh( IBloc* bloc, unsigned left, IBloc* parent = 0 )
        {
          if (bloc->get_ordering() != unsigned(-1))
            return left;
          bloc->set_ipar( parent );
          bloc->set_ordering( unsigned(-2) ); // mark as visited

          for (unsigned choice = 0; choice < 2; ++choice)
            if (IBloc* next = bloc->get_exit(choice))
              left = refresh( next, left, bloc ); else break;

          // reverse post
          bloc->set_ordering( --left );

          return left;
        }
      } ordering;

      unsigned chk = ordering.refresh( bloc_pool[0], bloc_pool.size() );
      if (chk != 0) throw 0; // sanity check
      // struct PtrLessByOrdering { bool operator () ( IBloc const* l, IBloc const* r ) { return l->get_ordering() < r->get_ordering(); } };
      // std::sort( bloc_pool.begin(), bloc_pool.end(), PtrLessByOrdering() );
      std::sort( bloc_pool.begin(), bloc_pool.end(), []( IBloc const* l, IBloc const* r ) { return l->get_ordering() < r->get_ordering(); } );
      if (entry_bloc != bloc_pool[0]) throw 0;
    }

    // Computing dominance using the algorithm described in "A Simple,
    // Fast Dominance Algorithm". Keith D. Cooper, Timothy J. Harvey,
    // and Ken Kennedy.

    // For the sake of algorithm termination, root is the
    // immediate dominator of itself
    for (IBloc* bloc : bloc_pool)
      bloc->set_idom( 0 );
    entry_bloc->set_idom( entry_bloc );

    for (bool stop = false; (stop = not stop); )
      {
        for (IBloc* bloc : bloc_pool)
          {
            if (bloc == entry_bloc)
              continue;

            IBloc* nidom = 0;

            for (IBloc* pred : bloc_pool)
              {
                if (not pred->precede(bloc))
                  continue;
                if (not pred->get_idom())
                  continue;
                if (not nidom)
                  { nidom = pred; continue; }

                IBloc* b1 = nidom;
                IBloc* b2 = pred;

                while (b1 != b2) {
                  while (b1->get_ordering() > b2->get_ordering())
                    b1 = b1->get_idom();
                  while (b2->get_ordering() > b1->get_ordering())
                    b2 = b2->get_idom();
                }
                nidom = b1;
              }

            if (bloc->get_idom() != nidom)
              {
                bloc->set_idom( nidom );
                stop = false;
              }
          }
      }
  }

  bool
  CFG::reducible()
  {
    // compute immediate dominators
    refresh_props();

    /* Checking if graph is reducible */
    for (IBloc* origin : bloc_pool)
      {
        for (unsigned choice = 0; choice < 2; ++choice)
          {
            IBloc* ending = origin->get_exit(choice);
            if (not ending)
              break;
            if (ending->get_ordering() >= origin->get_ordering())
              continue; /* not a Retreating-Edge (Forward- or Cross-). */
            /* For the retreating edge to be a back edge, ending node
             * should be in dominator list of origin node. */
            if (not ending->dominates(origin))
              return false;
          }
      }

    // everything went well
    return true;
  }

  std::string
  CFG::getkey() const
  {
    struct SymBloc
    {
      SymBloc* nexts[2];
      SymBloc* sides[2];
      SymBloc* preds;

      int label;
      SymBloc() : nexts(), sides(), preds(), label() {}
      int get_symclass(int alt) const { if (SymBloc* x = this->nexts[alt]) return x->label; return 0; }
      int compare_center( SymBloc* with ) const { return this->label - with->label; }
      SymBloc* nextpred( SymBloc const* target ) const { return target == nexts[0] ? sides[0] : sides[1]; }
      int compare_neighborhood( SymBloc* with ) const
      {
        std::map<int,int> comparisons;
        comparisons[this->get_symclass(0)] +=1;
        comparisons[this->get_symclass(1)] +=1;
        comparisons[with->get_symclass(0)] -=1;
        comparisons[with->get_symclass(1)] -=1;

        for (SymBloc* pred = this->preds; pred; pred = pred->nextpred( this ))
          comparisons[-pred->label] += 1;
        for (SymBloc* pred = with->preds; pred; pred = pred->nextpred( with ))
          comparisons[-pred->label] -= 1;

        for (auto&& c : comparisons)
          if (c.second)
            return c.second;

        // identical
        return 0;
      }

    };

    unsigned size = bloc_pool.size();
    SymBloc  bloc_storage[size];
    SymBloc* blocs[size];

    {
      std::map<IBloc*,SymBloc*> conv;
      for (unsigned idx = 0, end = size; idx < end; ++idx)
        conv[bloc_pool[idx]] = blocs[idx] = &bloc_storage[idx];

      for (IBloc* bloc : bloc_pool)
        {
          SymBloc* origin = conv[bloc];
          for (unsigned choice = 0; choice < 2; ++choice)
            {
              if (IBloc* next = bloc->get_exit(choice))
                {
                  SymBloc* ending = conv[next];
                  origin->nexts[choice] = ending;
                  origin->sides[choice] = ending->preds;
                  ending->preds = origin;
                }
            }
          origin->label = (bloc == entry_bloc ? 1 : 2);
        }
    }

    struct SymLess
    {
      bool operator() ( SymBloc* a, SymBloc* b ) const
      {
        if (int delta = a->compare_center(b)) return delta < 0;
        return a->compare_neighborhood(b) < 0;
      }
    };

    struct SymUpdate
    {
      SymUpdate() : stop(false) {} bool stop;
      int operator() ( SymBloc* a, SymBloc* b )
      {
        if (int delta = a->compare_center(b)) return delta < 0;
        if (int delta = a->compare_neighborhood(b)) { stop = false; return delta < 0; }
        return 0;
      }
      bool run() { if (stop) return false; stop = true; return true; }
    };

    for (SymUpdate update; update.run(); )
      {
        std::sort( &blocs[0], &blocs[size], SymLess() );

        int last_symclass = 1;
        for (unsigned idx = 1; idx < size; ++idx)
          {
            int new_symclass = last_symclass + update(blocs[idx-1],blocs[idx]);
            blocs[idx-1]->label = last_symclass;
            last_symclass = new_symclass;
          }
        blocs[size-1]->label = last_symclass;
      }

    // Now we have a canonical node order, compute the key
    for (unsigned idx = 0; idx < size; ++idx)
      {
        blocs[idx]->label = idx+1;
      }

    std::ostringstream buf;
    char const* sep = "";
    for (unsigned idx = 0; idx < size; ++idx)
      {
        SymBloc* bloc = blocs[idx];
        unsigned exits[2];
        for (unsigned alt = 0; alt < 2; ++alt)
          if (SymBloc* x = bloc->nexts[alt])
            exits[alt] = x->label;
          else
            exits[alt] = 0;
        if (exits[false] > exits[true])
          std::swap(exits[false], exits[true]);
        buf << sep << exits[false] << '|' << exits[true];
        sep = ",";
      }
    return buf.str();
  }
} // end of namespace UWT

