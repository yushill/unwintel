/***************************************************************************
                               uwt/win/pef.hh
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

/** @file uwt/win/pef.hh
    @brief  PEF manips

*/

#ifndef UWT_PEF_HH
#define UWT_PEF_HH

#include <uwt/utils/bytefield.hh>
#include <iosfwd>
#include <inttypes.h>

namespace UWT {
  namespace PEF {
    enum exception_t { FormatError, NotFound, NotImplemented };

    struct RootDigger
    {
      RootDigger( std::istream& _source );

      ByteField                  field( char const* _key ) const { return ByteField().seek( this->description, _key, 0 ); }

    private:
      static char const* const    description;
    };

    struct PEDigger
    {
      PEDigger( RootDigger& _root, std::istream& _source );
      ByteField                   field( char const* _key ) const
      { return ByteField().seek( this->description, _key, this->m_offset ); }
      ByteField                   field() const
      { return ByteField( this->m_offset, this->field( "datadirs" ).offset - this->m_offset ); }

    private:
      uint32_t                    m_offset;

      static char const* const    description;
    };

    enum datadirid_t {
      Export = 0, Import, Resource,   Exception,   Certificate, BaseRelocation, Debug,         Architecture,
      GlobalPtr,  TLS,    LoadConfig, BoundImport, IAT,         DelayImport,    COMPlusRuntime
    };

    struct DataDirDigger
    {
      DataDirDigger( PEDigger& _pe, std::istream& _source );
      ByteField                   field( char const* _key ) const
      { return ByteField().seek( this->description, _key, this->m_offset ); }
      ByteField                   field() const
      { return ByteField().all( this->description, this->m_offset ); }
      operator                    bool() { return this->m_idx < this->m_count; }
      DataDirDigger&              next();
      intptr_t                index() { return this->m_idx; };
      datadirid_t                 id() { return datadirid_t( this->m_idx ); }

    private:
      uint32_t                    m_offset;
      int32_t                     m_count;
      int32_t                     m_idx;

      static char const* const    description;
    };

    struct SectionHdrDigger
    {
      SectionHdrDigger( PEDigger& _pe, std::istream& _source );
      ByteField                   field( char const* _key ) const
      { return ByteField().seek( this->description, _key, this->m_offset ); }
      ByteField                   field() const
      { return ByteField().all( this->description, this->m_offset ); }
      operator                    bool() { return this->m_idx < this->m_count; }
      SectionHdrDigger&           next();
      intptr_t                    index() { return this->m_idx; }
      int16_t                     count() { return this->m_count; }

      enum Characteristics {
        CodeObject    = 0x00000020,
        Initialized   = 0x00000040,
        Uninitialized = 0x00000080,
        NotCacheable  = 0x04000000,
        NotPageable   = 0x08000000,
        Shared        = 0x10000000,
        Executable    = 0x20000000,
        Readable      = 0x40000000,
        Writeable     = 0x80000000,
      };

     private:
      uint32_t                    m_offset;
      int16_t                     m_count;
      int16_t                     m_idx;

      static char const* const    description;
    };

    struct SectionDigger
    {
      SectionDigger( SectionHdrDigger& _sectionhdr, std::istream& _source );

      ByteField const&            field() const { return m_field; }

    private:
      ByteField                   m_field;
    };
  };
};

#endif // UWT_PEF_HH
