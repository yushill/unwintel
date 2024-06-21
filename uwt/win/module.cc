/***************************************************************************
                             uwt/win/module.cc
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

/** @file uwt/win/module.cc
    @brief  implementation file for tranlations units

*/

#include <uwt/win/module.hh>
#include <uwt/win/pef.hh>
#include <uwt/win/winnt.hh>
#include <uwt/trans/functionlab.hh>
#include <uwt/x86/context.hh>
#include <uwt/x86/memory.hh>
#include <uwt/x86/calldb.hh>
#include <uwt/iterbox.hh>
#include <uwt/utils/logchannels.hh>
#include <uwt/utils/misc.hh>
#include <unisim/util/dbgate/client/client.cc>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cassert>
#include <cstdlib>

#include <unistd.h>
#include <sys/wait.h>

namespace UWT
{
  Module::Function&
  Module::Function::operator=( Function const& _fun ) {
    m_addr = _fun.m_addr;
    m_name = _fun.m_name;
    m_code = _fun.m_code;
    return *this;
  }

  Module::Module( std::string const& _name )
    : m_name( _name ), m_loaded( false )
  {
    std::string buffer( _name );
    for( std::string::iterator ch = buffer.begin(); ch != buffer.end(); ++ch ) {
      if( *ch >= 'A' and *ch <= 'Z' ) *ch += 'a' - 'A';
    }
    m_fsname = buffer.c_str();
  }

  Module::~Module() {}

  Module&
  Module::record( uint32_t _addr, std::string const& _name, hostcode_t _code ) {
    m_functions.push_back( Function( _addr, _name, _code ) );
    return *this;
  }

  uint32_t
  Module::symboladdr( std::string const& _symbol, Memory const& _mem ) const {
    for( uintptr_t idx = 0; idx < m_functions.size(); ++ idx )
      if( m_functions[idx].m_name == _symbol )
        return m_functions[idx].m_addr + imgbase();
    std::cerr << "Missing symbol '" << _symbol << "' in '" << m_name << "'." << std::endl;
    return 0;
  }

  uint32_t
  Module::symboladdr( uint32_t _ordinal, Memory const& _mem ) const {
    // Symbol is referenced by ordinal, the default behaviour here is
    // to ignore it. UserModule or LibModule should support it.
    std::cerr << "Missing symbol #" << _ordinal << " in '" << m_name << "'." << std::endl;
    return 0;
  }

  void
  Module::hostcodemap( CallDB& _calldb ) {
    for( uintptr_t idx = 0; idx < m_functions.size(); ++idx ) {
      Function& func = m_functions[idx];
      _calldb.add( func.m_addr + m_image.m_first, func.m_code, func.m_name );
    }
  }

  void
  Module::make( UWT::IterBox& ibox )
  {
    uintptr_t argc
      = 1                    // make
      + ibox.quiet_make      // [-s]
      + 2*m_newfiles.size(); // [-W <file>]*

    char const* argv[argc+1];
    {
      char const** argvptr = argv;
      *argvptr++ = "make";
      if (ibox.quiet_make)
        *argvptr++ = "-s";
      for (std::vector<std::string>::const_iterator node = m_newfiles.begin(); node != m_newfiles.end(); ++ node)
        {
          *argvptr++ = "-W";
          *argvptr++ = node->c_str();
        }
      assert( uintptr_t(argvptr - argv) == argc );
      *argvptr++ = 0;
    }

    //     for( char const** argvptr = argv; *argvptr; ++argvptr )
    //       std::cerr << *argvptr << " ";
    //     std::cerr << std::endl;

    struct
    {
      bool operator () ( char const* const* _argv )
      {
        pid_t pid = fork();
        int status = -1;

        if (pid >  0)
          {
            // caller
            waitpid( pid, &status, 0 );
          }
        else if (pid < 0)
          {
            perror( "Module::make" );
            return false;
          }
        else /*pid == 0*/
          {
            // callee
            execvp( _argv[0], (char* const*)( _argv ) );
            std::cerr << "what the fork";
            return false;
          }

        return WIFEXITED( status ) and (WEXITSTATUS( status ) == 0);
      }
    } makeit;

    if (not makeit( argv ))
      throw MakeError;
  }

  void
  Module::transmulate( uint32_t entry_addr, Memory& _mem )
  {
    FunctionLab transcoder( entry_addr );

    // Decompile function at address @c entry_addr
    transcoder.decode( _mem );
    transcoder.decompile();

    uint32_t rva = entry_addr - m_image.m_first;

    std::string fncname = strx::fmt( "fx%08x", rva );

    {
      std::string srcpath( getfsname() + '/' + fncname + ".cc" );
      std::ofstream sink( srcpath.c_str() );
      transcoder.translate( getname().c_str(), fncname.c_str(), sink );
      m_newfiles.push_back( srcpath.c_str() );
    }

    // Recording new function
    record( rva, fncname, 0 );

    {
      std::string funpath( getfsname() + "/functions.inc" );
      std::ofstream sink( funpath.c_str() );
      sink << "// -*- mode: c++ -*-\n";

      for( uintptr_t idx = 0; idx < m_functions.size(); ++ idx ) {
        Function& func = m_functions[idx];
        sink << strx::fmt( "FUNCTION( %#010x, %s );\n", func.m_addr, func.m_name.c_str() );
      }
      m_newfiles.push_back( funpath.c_str() );
    }
  }

  Range
  Module::allocate( WinNT& os, uint32_t bottom, uint32_t top, bool topdown, uint32_t size )
  {
    Range range( bottom, top );
    os.memmgr().allocate( range, topdown, size, this );
    return range;
  }

  PEFModule::PEFModule( std::string const& _name )
    : Module( _name )
  {}

  PEFModule::~PEFModule() {}

  void
  PEFModule::imagepath( std::string const& _imagepath ) {
    m_imagepath = _imagepath;
  }

  void
  PEFModule::mount( WinNT& _os, std::istream& _source, PEF::PEDigger& _pe )
  {
    uint32_t imgbase = _pe.field( "nt.ImageBase" ).getint<uint32_t>( _source );
    uint32_t imgsize = _pe.field( "nt.SizeOfImage" ).getint<uint32_t>( _source );

    assert( (imgbase % Memory::Page::s_size) == 0 and (imgsize % Memory::Page::s_size) == 0 );

    DebugStream& sink = _os.loader_log();

    if (sink.good())
      {
        sink << strx::fmt( "%-24s%#010x\n", "Image base:", imgbase );
        sink << strx::fmt( "%-24s%#010x\n", "Image size:", imgsize );
      }

    {
      uint32_t imgtop = imgbase;
      // First pass: Sanity checking + determining image's top
      for (PEF::SectionHdrDigger sectionhdr( _pe, _source ); sectionhdr.next();)
        {
          uint32_t src_size = sectionhdr.field( "SizeOfRawData" ).getint<uint32_t>( _source );
          uint32_t dst_addr = sectionhdr.field( "VirtualAddress" ).getint<uint32_t>( _source ) + imgbase;
          uint32_t dst_size = sectionhdr.field( "VirtualSize" ).getint<uint32_t>( _source );
          assert( dst_addr >= imgtop  );
          imgtop = dst_addr + std::max( src_size, dst_size );
        }
      assert( imgtop <= (imgbase + imgsize) );
      m_image = Range( imgbase, imgbase + imgsize - 1 );
      _os.addmodule( m_image, this );

      for (PEF::SectionHdrDigger sectionhdr( _pe, _source ); sectionhdr.next();)
        {
          char     name[] = {'\0','\0','\0','\0','\0','\0','\0','\0','\0'};
          sectionhdr.field( "Name" ).fits(sizeof (name)).getstring( name, _source );
          uint32_t src_addr = sectionhdr.field( "PointerToRawData" ).getint<uint32_t>( _source );
          uint32_t src_size = sectionhdr.field( "SizeOfRawData" ).getint<uint32_t>( _source );
          uint32_t dst_addr = sectionhdr.field( "VirtualAddress" ).getint<uint32_t>( _source ) + imgbase;
          uint32_t dst_size = sectionhdr.field( "VirtualSize" ).getint<uint32_t>( _source );
          uint32_t flags    = sectionhdr.field( "Characteristics" ).getint<uint32_t>( _source );

          if (sink.good())
            sink << strx::fmt( "%-24s(%d) [%#010x,%#010x] -> [%#010x,%#010x] (flags: %08x)\n",
                               name, sectionhdr.index()+1, src_addr, src_size, dst_addr, dst_size, flags );

          if( flags & PEF::SectionHdrDigger::Uninitialized )
            {
              _os.mem().zfill( dst_addr, std::max( src_size, dst_size ) );
            }
          else
            {
              if (dst_size > src_size)
                _os.mem().zfill( dst_addr, dst_size );

              _source.seekg( src_addr );
              _os.mem().write( dst_addr, _source, src_size );
            }
        }
    }
  }

  void
  PEFModule::link( WinNT& _os, std::istream& _source, PEF::PEDigger& _pe )
  {
    uint32_t imgbase = m_image.m_first, import_addr = 0// , import_size
      ;
    DebugStream& sink = _os.loader_log();

    for (PEF::DataDirDigger datadir( _pe, _source ); datadir.next();)
      {
        uint32_t addr = datadir.field( "RVA" ).getint<uint32_t>( _source ) + imgbase;
        uint32_t size = datadir.field( "Size" ).getint<uint32_t>( _source );
        switch (datadir.id())
          {
          case PEF::Import:
            if (sink.good())
              sink << strx::fmt( "%-24s[%#x,%#x]\n", "Import:", addr, size );
            import_addr = addr;
            //import_size = size;
            break;
          case PEF::Export:
            if (sink.good())
              sink << strx::fmt( "%-24s[%#x,%#x]\n", "Export:", addr, size );
            m_export_addr = addr;
            m_export_size = size;
            break;
          case PEF::Resource: case PEF::Exception: case PEF::Certificate:
          case PEF::BaseRelocation: case PEF::Debug: case PEF::Architecture: case PEF::GlobalPtr:
          case PEF::TLS: case PEF::LoadConfig: case PEF::BoundImport: case PEF::IAT:
          case PEF::DelayImport: case PEF::COMPlusRuntime:
            /* Ignored for now... */
            break;
          default: break;
          }
      }

    // Mapping DLL imports
    std::string   memstring;

    for( uint32_t idt_addr = import_addr; true; idt_addr += 0x14 ) {
      // Import struct
      uint32_t ilt_addr  = _os.mem().r<4>( idt_addr + 0x00 ); // ILT
      uint32_t tds      = _os.mem().r<4>( idt_addr + 0x04 ); // TimeDateStamp
      // uint32_t fwd   = _os.mem().r<4>( idt_addr + 0x08 ); // Forwarder
      uint32_t name_addr = _os.mem().r<4>( idt_addr + 0x0c ); // Name
      uint32_t iat_addr  = _os.mem().r<4>( idt_addr + 0x10 ); // IAT

      if( ilt_addr == 0 and name_addr == 0 and iat_addr == 0 )
        break; ///< End of Image Import Table

      ilt_addr += imgbase;
      iat_addr += imgbase;
      name_addr += imgbase;

      uint32_t& ilt_ptr = (tds == 0 ? iat_addr : ilt_addr);

      memstring.clear();
      _os.mem().read( name_addr, memstring );
      sink << strx::fmt( "%-24s%s\n", "Importing from:", memstring.c_str() );

      Module& module = _os.loadmodule( memstring.c_str() );

      for( uint32_t iltentry; (iltentry = _os.mem().r<4>( ilt_ptr )); iat_addr += 4, ilt_addr += 4 ) {
        if( iltentry & 0x80000000 ) {
          sink << strx::fmt( "%-24snumber: %#x, stub: %#010x\n", "  by ordinal:", (iltentry & 0x7fffffff), iat_addr );
          _os.mem().w<4>( iat_addr ) = module.symboladdr( iltentry & 0x7ffffff, _os.mem() );
        } else {
          iltentry += imgbase;
          uint32_t hint = _os.mem().r<2>( iltentry );
          memstring.clear();
          _os.mem().read( iltentry + 2, memstring );

          sink << strx::fmt( "%-24shint: %#010x, stub: %#010x, name: %s\n", "  by name:", hint, iat_addr, memstring.c_str() );

          // Registering function address
          _os.mem().w<4>( iat_addr ) = module.symboladdr( memstring.c_str(), _os.mem() );
        }
      }
    }
  }

  PEFModule::Export::Export( PEFModule const& _module, Memory const& _mem )
    : m_mem( _mem ), m_idx( uint32_t(-1) ), m_imgbase( _module.imgbase() )
  {
    m_exp_flags   = _mem.r<4>( _module.m_export_addr + 0x00 ); // Export Flags
    m_tds         = _mem.r<4>( _module.m_export_addr + 0x04 ); // Time/Date Stamp
    m_major       = _mem.r<2>( _module.m_export_addr + 0x08 ); // Major Version
    m_minor       = _mem.r<2>( _module.m_export_addr + 0x0a ); // Minor Version
    m_name_ptr    = _mem.r<4>( _module.m_export_addr + 0x0c ); // Name RVA
    m_ord_start   = _mem.r<4>( _module.m_export_addr + 0x10 ); // Ordinal Base
    m_addr_count  = _mem.r<4>( _module.m_export_addr + 0x14 ); // Address Table Entries
    m_name_count  = _mem.r<4>( _module.m_export_addr + 0x18 ); // Number of Name Pointers
    m_addr_table  = _mem.r<4>( _module.m_export_addr + 0x1c ); // Export Address Table RVA
    m_name_table  = _mem.r<4>( _module.m_export_addr + 0x20 ); // Name Pointer RVA
    m_ord_table   = _mem.r<4>( _module.m_export_addr + 0x24 ); // Ordinal Table RVA
    m_name_ptr   += m_imgbase;
    m_addr_table += m_imgbase;
    m_name_table += m_imgbase;
    m_ord_table  += m_imgbase;
  }

  std::string
  PEFModule::Export::getname()
  {
    uint32_t straddr = m_mem.r<4>( m_name_table + m_idx * 4 ) + m_imgbase;
    std::string result;
    m_mem.read( straddr, result );
    return result;
  }

  uint32_t
  PEFModule::Export::ordinal() {
    return m_mem.r<2>( m_ord_table + m_idx * 2 );
  }

  uint32_t
  PEFModule::Export::rva() {
    return rva( ordinal() );
  }

  uint32_t
  PEFModule::Export::rva( uint32_t _ordinal ) {
    uint32_t index = _ordinal - m_ord_start;
    if( _ordinal >= m_addr_count ) {
      std::cerr << "Error: bad ordinal reference ("
           << "count:   " << m_addr_count << ", "
           << "index:   " << index << ", "
           << "ordinal: " << _ordinal << ")." << std::endl;
      return 0;
    }
    return m_mem.r<4>( m_addr_table + _ordinal * 4 ) + m_imgbase;
  }

  MainModule::MainModule( std::string const& _imagepath )
    : PEFModule( "MAIN_EXE" )
  {
    m_imagepath = strx::abspath( _imagepath.c_str() );
  }

  MainModule::~MainModule() {}

  void
  MainModule::init( WinNT& _os )
  {
    std::ifstream source( m_imagepath.c_str(), std::ios::binary );

    DebugStream& sink = _os.loader_log();
    sink << "Module: " << getname() << std::endl;

    PEF::RootDigger root( source );
    PEF::PEDigger   pe( root, source );

    this->mount( _os, source, pe );

    {
      // Entrypoint
      uint32_t imgbase = m_image.m_first;
      uint32_t entrypoint = pe.field( "coff.AddressOfEntryPoint" ).getint<uint32_t>( source ) + imgbase;
      m_entrypoint = entrypoint;
      sink << strx::fmt( "%-24s%#010x\n", "Instruction Pointer:", entrypoint );
      uint32_t reserve, commit;
      // Stack
      reserve = pe.field( "nt.SizeOfStackReserve" ).getint<uint32_t>( source );
      commit = pe.field( "nt.SizeOfStackCommit" ).getint<uint32_t>( source );
      sink << strx::fmt( "%-24s%#010x\n", "Stack reserve size:", reserve );
      sink << strx::fmt( "%-24s%#010x\n", "Stack commit size:", commit );
      _os.stack( Range( 0, imgbase - 1 ), 1, reserve, this );
      // Heap
      reserve = pe.field( "nt.SizeOfHeapReserve" ).getint<uint32_t>( source );
      commit = pe.field( "nt.SizeOfHeapCommit" ).getint<uint32_t>( source );
      sink << strx::fmt( "%-24s%#010x\n", "Heap reserve size:", reserve );
      sink << strx::fmt( "%-24s%#010x\n", "Heap commit size:", commit );
      _os.heap( Range( imgbase, Range::top / 2 ), 0, reserve, this );
    }

    this->link( _os, source, pe );

    _os.memmgr().dump( sink << "Dump of memory map:\n" );
    {
      // Finally, some miscellaneous information on executable
      uint16_t subsystem = pe.field( "nt.Subsystem" ).getint<uint16_t>( source );
      sink << strx::fmt( "%-24s%#x\n", "Windows NT subsystem:", subsystem );
    }
  }

  LibModule::LibModule( std::string const& _name, std::string const& _imagepath )
    : PEFModule( _name )
  {
    if (startswith("/", _imagepath.c_str()))
      m_imagepath = _imagepath;
    else
      m_imagepath = strx::abspath( "shared/" ) + getfsname() + '/' + _imagepath;;
  }

  LibModule::~LibModule() {}

  LibModule::WorkingDir::WorkingDir( LibModule const& lib )
  {
    m_saveddir = strx::abspath(".");
    std::string target( strx::abspath("shared/") + lib.getfsname() );
    if (chdir( target.c_str() ) != 0)
      throw std::runtime_error("chdir internal error");
  }

  LibModule::WorkingDir::~WorkingDir() noexcept(false)
  {
    if (chdir( m_saveddir.c_str() ) != 0)
      throw std::runtime_error("chdir internal error");
  }

  void
  LibModule::init( WinNT& _os )
  {
    WorkingDir change( *this );
    std::ifstream source(m_imagepath.c_str(), std::ios::binary);
    DebugStream& sink = _os.loader_log();
    sink << "Module: " << getname() << std::endl;

    PEF::RootDigger root( source );
    PEF::PEDigger   pe( root, source );

    this->mount( _os, source, pe );
    this->link( _os, source, pe );

    {
      std::string symbolname;
      sink << "Export table:\n";
      for (Export symbol( *this, _os.mem() ); symbol.next();)
        {
          symbolname = symbol.getname();
          sink << "  " << symbolname << ": " << symbol.ordinal() << std::endl;
        }
    }
  }

  uint32_t
  LibModule::symboladdr( std::string const& _symbol, Memory const& _mem ) const
  {
    std::string symbolname;

    for (Export symbol( *this, _mem ); symbol.next();)
      {
        symbolname = symbol.getname();
        if (symbolname == _symbol.c_str())
          return symbol.rva();
      }

    return 0;
  }

  uint32_t
  LibModule::symboladdr( uint32_t _ordinal, Memory const& _mem ) const
  {
    Export symbol( *this, _mem );
    return symbol.rva( _ordinal );
  }

  void
  LibModule::make( UWT::IterBox& ibox )
  {
    WorkingDir change( *this );
    this->Module::make( ibox );
  }

  void
  LibModule::transmulate( uint32_t _addr, Memory& _mem )
  {
    WorkingDir change( *this );
    this->Module::transmulate( _addr, _mem );
  }
};
