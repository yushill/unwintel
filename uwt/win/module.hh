/***************************************************************************
                             uwt/win/module.hh
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

/** @file uwt/win/module.hh
    @brief  header file for tranlations units

*/

#ifndef UWT_MODULE_HH
#define UWT_MODULE_HH

#include <uwt/win/memmgr.hh>
#include <string>
#include <vector>
#include <iosfwd>
#include <inttypes.h>

namespace UWT
{
  struct Module
  {
    enum exception_t { NotFound, MakeError, TransmulationError };

    Module( std::string const& _name );
    virtual ~Module();

    std::string                   getname() const { return m_name; };
    std::string                   getfsname() const { return m_fsname; };
    bool                          isloaded() const { return m_loaded; };
    void                          loaded( bool _loaded ) { m_loaded = _loaded; }

    Module&                       record( uint32_t _addr, std::string const& _name, hostcode_t _code );
    uint32_t                      imgbase() const { return m_image.m_first; }
    void                          hostcodemap( CallDB& _calldb );
    virtual void                  init( WinNT& _os ) = 0;
    virtual uint32_t              symboladdr( std::string const& _symbol, Memory const& _mem ) const;
    virtual uint32_t              symboladdr( uint32_t _ordinal, Memory const& _mem ) const;
    virtual void                  make( UWT::IterBox& );
    virtual void                  transmulate( uint32_t _addr, Memory& _mem );

    Range                         allocate( WinNT& os, uint32_t bottom, uint32_t top, bool topdown, uint32_t size );

  protected:

    struct Function
    {
      uint32_t                    m_addr;
      std::string                 m_name;
      hostcode_t                  m_code;

      Function() : m_addr( 0 ), m_code( 0 ) {}
      Function( uint32_t _addr, std::string const& _name, hostcode_t _code )
        : m_addr( _addr ), m_name( _name ), m_code( _code )
      {}
      Function( Function const& _fun )
        : m_addr( _fun.m_addr ), m_name( _fun.m_name ), m_code( _fun.m_code )
      {}
      ~Function() {}

      Function&
      operator=( Function const& _fun );
    };
    std::vector<Function>         m_functions;
    Range                         m_image;
    uint32_t                      m_import_addr;

  private:
    std::string                   m_name;
    std::string                   m_fsname;
    bool                          m_loaded;
    std::vector<std::string>      m_newfiles;
  };

  struct PEFModule : public Module
  {
    std::string                   m_imagepath;
    uint32_t                      m_export_addr;
    uint32_t                      m_export_size;

    PEFModule( std::string const& _name );
    ~PEFModule();

    void                          imagepath( std::string const& _imagepath );
    void                          mount( WinNT& _os, std::istream& _source, PEF::PEDigger& _pe );
    void                          link( WinNT& _os, std::istream& _source, PEF::PEDigger& _pe );

    struct Export
    {
      Export( PEFModule const& _module, Memory const& _ctx );

      bool                        next() { return (++m_idx) < m_name_count; }
      std::string                 getname();
      uint32_t                    ordinal();
      uint32_t                    rva();
      uint32_t                    rva( uint32_t _ordinal );

      Memory const&             m_mem;
      uint32_t                    m_idx;
      uint32_t                    m_imgbase;
      uint32_t m_exp_flags,  m_tds, m_major, m_minor, m_name_ptr;
      uint32_t m_ord_start,  m_addr_count,   m_addr_table;
      uint32_t m_name_count, m_name_table,   m_ord_table;
    };
  };

  struct MainModule : public PEFModule
  {
    MainModule( std::string const& _imagepath );
    ~MainModule();

    void                          init( WinNT& _os );

    Range                       ms_tack;
    Range                       m_heap;
    uint32_t                      m_entrypoint;
  };

  struct LibModule : public PEFModule
  {
    struct WorkingDir
    {
      std::string                 m_saveddir;
      WorkingDir( LibModule const& _mine );
      ~WorkingDir() noexcept(false);
    };

    LibModule( std::string const& _name, std::string const& _imagepath );
    ~LibModule();

    void                          init( WinNT& _os );
    uint32_t                      symboladdr( std::string const& _symbol, Memory const& _mem ) const;
    uint32_t                      symboladdr( uint32_t _ordinal, Memory const& _mem ) const;
    void                          make( UWT::IterBox& );
    void                          transmulate( uint32_t _addr, Memory& _mem );
  };
};

#endif // UWT_MODULE_HH
