/***************************************************************************
                             uwt/trans/ibloc.hh
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

/** @file uwt/trans/ibloc.hh
    @brief  header file for instruction blocs

*/

#ifndef UWT_IBLOC_HH
#define UWT_IBLOC_HH

#include <uwt/trans/statement.hh>
#include <uwt/fwd.hh>
#include <vector>
#include <string>
#include <inttypes.h>

namespace UWT
{
  /**
   * @brief IBloc objects gather merged Statements object.
   */
  struct IBloc
  {
    struct Exits
    {
      Exits() : nat(0), odd(0) {}
      Exits( IBloc* _nat, IBloc* _odd ) : nat(_nat), odd(_odd) {}

      void     set( bool alt, IBloc* bloc ) { if (alt) odd = bloc; else nat = bloc; }
      IBloc*   get( bool alt ) const { if (alt) return odd; return nat; }
      bool     which( IBloc* bloc ) const;
      bool     has( IBloc* bloc ) const { return ((nat == bloc) or (odd == bloc)); }
      IBloc*   common( Exits const& x ) const { return has( x.nat ) ? x.nat : has( x.odd ) ? x.odd : 0; }
      unsigned count() const { return unsigned(nat != 0) + unsigned(odd != 0); }

      struct iterator
      {
        iterator( Exits const& _x, unsigned _c ) : x(_x), c(_c) {}
        bool operator != (iterator const& rhs) { return c != rhs.c; }
        unsigned next() { unsigned rc = c; c = (c == 0 and x.odd) ? 1 : 2; return rc; }
        iterator operator ++ () { return iterator(x, next()); }
        IBloc* operator *() { return x.get(c); }
        Exits const& x;
        unsigned c;
      };
      iterator begin() const { return iterator(*this,nat ? 0 : odd ? 1 : 2); }
      iterator end() const { return iterator(*this,2); }

    private:
      IBloc* nat;
      IBloc* odd;
    };

    /**
     * @brief IBloc default constructor
     *
     * Creates an empty IBloc object
     */
    IBloc() : statements(), exit_blocs(), ipar(0), idom(0), ordering(-1), prevcount(0) {}

    /**
     * @brief IBloc single statement constructor
     * @param _ordering the starting order
     * @param inst the starting statement
     *
     * Creates an IBloc object with one starting statement.
     */
    IBloc( Statement const& inst );

    /**
     * @brief IBloc multiple statement constructor
     * @param stmts the starting statements
     *
     * Creates an IBloc object with multiple starting statements.
     */
    IBloc( std::vector<Statement> const& stmts )
      : statements(stmts), exit_blocs(), ipar(0), idom(0), ordering(-1), prevcount(0)
    {}

    /**
     * @brief IBloc destructor
     *
     * Destroys an IBloc object
     */
    ~IBloc() noexcept(false);

    uintptr_t             size() const { return statements.size(); }
    void                  reserve(uintptr_t capacity) { statements.reserve(capacity); }
    void                  clear() { statements.clear(); ordering = -1; }
    unsigned              get_ordering() const { return ordering; }
    void                  set_ordering(unsigned _ordering) { ordering = _ordering; }
    IBloc*                get_idom() const { return idom; }
    void                  set_idom(IBloc* _idom) { idom = _idom; }
    IBloc*                get_ipar() const { return ipar; }
    void                  set_ipar(IBloc* _ipar) { ipar = _ipar; }

    bool                  dominates( IBloc const* bloc ) const
    {
      for (IBloc const* dom; bloc != this; bloc = dom)
        {
          dom = bloc->idom;
          if (dom == bloc)
            return false; // Reached root node
        }
      return true;
    }

    /**
     * @brief appends one statement at the end.
     */
    IBloc&                append_stmt( Statement const& inst );

    /**
     * @brief inserts one statement at the begining
     */
    IBloc&                insert_stmt( Statement const& );

    /**
     * @brief appends the content of another IBloc at the end
     */
    IBloc&                append( IBloc const& _ibloc );

    /**
     * @brief returns the first statement of this IBloc
     */
    BaseStatement*        at(uintptr_t idx) { return statements.at(idx).node; };
    BaseStatement*        last() { return statements.back().node; }

    // CFG methods
    /**
     * @brief increases the references count of this IBloc
     */
    static IBloc*         ref( IBloc* ib ) { if (ib != 0) { ib->prevcount++; } return ib; }

    /**
     * @brief decreases the references count of this IBloc
     */
    static IBloc*         unref( IBloc* ib ) { if (ib != 0) { ib->prevcount--; } return 0; }

    /**
     * @brief returns the number of incoming edges
     */
    unsigned              count_predecessors() const { return prevcount; }

    /**
     * @brief returns the number of outgoing edges
     */
    unsigned              count_successors() const { return exit_blocs.count(); }

    /** @brief returns the one of the next referenced IBloc objects */
    IBloc*                get_exit( bool alt ) const { return exit_blocs.get(alt); }

    /** @brief returns the next IBlocs */
    Exits const&          get_exits() const { return exit_blocs; }

    /** @brief sets one of the next iblocs (exits) */
    void                  set_exit( bool alt, IBloc* );

    /** @brief sets next iblocs (exits)  */
    void                  set_exits( Exits const& _exits );

    /**
     * @brief returns the nature of the given next ibloc (true:
     * straight, or false: alternative)
     */
    bool                  which( IBloc* bloc ) const { return exit_blocs.which(bloc); }

    /** @brief tests wheter this IBloc can directly exits to the given bloc */
    bool                  precede( IBloc* bloc ) const { return exit_blocs.has(bloc); }

    /**
     * @brief tests whether this IBloc object has no next IBloc objects
     */
    bool                  deadend() const { return not (exit_blocs.get(false) or exit_blocs.get(true)); }

    /**
     * @brief tests whether this IBloc object has both next IBloc objects
     */
    bool                  choice() const { return exit_blocs.get(false) and exit_blocs.get(true); }

    /**
     * @brief duplicate the IBloc. A new IBloc is created copying: (1)
     * statement and (2) outgoing links. Remainder is lost/reset:
     * incoming links, labels...
     */
    IBloc*                duplicate();

  private:
    // CFG info
    std::vector<Statement>  statements; //< The statements of this object
    Exits                   exit_blocs; //< The outgoing links to exit blocs
    IBloc*                  ipar;       //< The immediate spanning tree parent (maintained externaly)
    IBloc*                  idom;       //< The immediate dominator (maintained externaly)
    unsigned                ordering;   //< The current ordering (maintained externaly)
    unsigned                prevcount;  //< The number of predecessor IBloc objects
  };

  struct GraphMatch
  {
    GraphMatch( char const* _title ) : title(_title) {}
    GraphMatch( char const* _title, char const* pattern, IBloc* first );

    uintptr_t             size() const { return blocs.size(); }

    std::vector<IBloc*>   blocs;

    IBloc::Exits          exits;

    char const*           title;

  private:
    struct Link
    {
      IBloc* bloc;
      Link const*  back;

      Link( IBloc* _bloc, Link const* _back ) : bloc(_bloc), back(_back) {}
      long fill( GraphMatch& m, long size ) const;
      bool has( IBloc* _bloc ) const { return (bloc == _bloc) or (back and back->has( _bloc )); }
      IBloc* first() const { return back ? back->first() : bloc; }
    };

    bool                  test( char const* pattern, IBloc* next_bloc, Link const* matched, Link const* back_bloc );
    bool                  close( char const* pattern, Link const* matched, Link const* back_blocs );
    bool                  set_exit( bool alt, IBloc* bloc );
  };
};

#endif /* UWT_IBLOC_HH */
