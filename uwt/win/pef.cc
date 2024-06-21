/***************************************************************************
                               uwt/win/pef.cc
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

/** @file uwt/win/pef.cc
    @brief  PEF manips

*/

#include <uwt/win/pef.hh>
#include <fstream>
#include <ostream>
#include <cstring>

namespace UWT
{
namespace PEF
{
  char const* const RootDigger::description = "signature:2;dummy:0x16;relocation:2;dummy:0x22;pe:2;";

  RootDigger::RootDigger( std::istream& _source )
  {
    char signature[3] = {'\0','\0','\0'};
    this->field( "signature" ).fits(sizeof signature).getstring( signature, _source );
    if (strcmp( signature, "MZ" ) != 0) throw FormatError; // Mark Zbikowski

    if (this->field( "relocation" ).getint<uint16_t>( _source ) < 0x0040)
      throw FormatError; // Pure DOS executable
  }

  char const* const PEF::PEDigger::description =
    "signature:4;"
    "coff.Machine:2;coff.NumberOfSections:2;coff.TimeDateStamp:4;"
    "coff.PointerToSymbolTable:4;coff.NumberOfSymbols:4;"
    "coff.SizeOfOptionalHeader:2;coff.Characteristics:2;"
    "optheader:0;"
    "coff.Magic:2;coff.MajorLinkerVersion:1;coff.MinorLinkerVersion:1;"
    "coff.SizeOfCode:4;coff.SizeOfInitializedData:4;"
    "coff.SizeOfUninitializedData:4;coff.AddressOfEntryPoint:4;"
    "coff.BaseOfCode:4;coff.BaseOfData:4;"
    "nt.ImageBase:4;"
    "nt.SectionAlignment:4;nt.FileAlignment:4;"
    "nt.MajorOperatingSystemVersion:2;nt.MinorOperatingSystemVersion:2;"
    "nt.MajorImageVersion:2;nt.MinorImageVersion:2;"
    "nt.MajorSubsystemVersion:2;nt.MinorSubsystemVersion:2;"
    "nt.Reserved:4;nt.SizeOfImage:4;nt.SizeOfHeaders:4;nt.CheckSum:4;"
    "nt.Subsystem:2;nt.DLLCharacteristics:2;"
    "nt.SizeOfStackReserve:4;nt.SizeOfStackCommit:4;"
    "nt.SizeOfHeapReserve:4;nt.SizeOfHeapCommit:4;"
    "nt.LoaderFlags:4;nt.NumberOfRvaAndSizes:4;"
    "datadirs:0;";


  PEDigger::PEDigger( RootDigger& _root, std::istream& _source )
    : m_offset( 0 )
  {
    this->m_offset = _root.field( "pe" ).getint<uint16_t>( _source );

    char signature[4]; ///< PE magic
    this->field( "signature" ).fits(sizeof signature).getstring( signature, _source );
    for( intptr_t idx = 0; idx < 4; idx++ )
      if( "PE\0\0"[idx] != signature[idx] ) throw FormatError;

    if (this->field( "coff.SizeOfOptionalHeader" ).getint<uint16_t>( _source ) == 0)
      throw FormatError;

    if (this->field( "coff.Magic" ).getint<uint16_t>( _source ) != 0x010b)
      throw NotImplemented; ///< Possibly a PE32+ image file
  }

  char const* const DataDirDigger::description = "RVA:4;Size:4;next:0;";

  DataDirDigger::DataDirDigger( PEDigger& _pe, std::istream& _source )
    : m_offset( 0 ), m_count( 0 ), m_idx( -1 )
  {
    this->m_offset = _pe.field( "datadirs" ).offset - this->field( "next" ).offset;
    this->m_count  = _pe.field( "nt.NumberOfRvaAndSizes" ).getint<int32_t>( _source );
  }

  DataDirDigger&
  DataDirDigger::next() {
    this->m_idx++;
    this->m_offset = this->field( "next" ).offset;
    return *this;
  }

  char const* const SectionHdrDigger::description =
    "Name:8;VirtualSize:4;VirtualAddress:4;SizeOfRawData:4;PointerToRawData:4;"
    "PointerToRelocations:4;PointerToLinenumbers:4;"
    "NumberOfRelocations:2;NumberOfLinenumbers:2;Characteristics:4;next:0;";

  SectionHdrDigger::SectionHdrDigger( PEDigger& _pe, std::istream& _source )
    : m_offset( 0 ), m_count( 0 ), m_idx( -1 )
  {
    this->m_offset =
      _pe.field( "coff.SizeOfOptionalHeader" ).getint<int16_t>( _source ) +
      _pe.field( "optheader" ).offset -
      this->field( "next" ).offset;
    this->m_count = _pe.field( "coff.NumberOfSections" ).getint<int16_t>( _source );
    if( not *this ) throw FormatError;
  }

  SectionHdrDigger&
  SectionHdrDigger::next() {
    this->m_idx ++;
    this->m_offset = this->field( "next" ).offset;
    return *this;
  }

  SectionDigger::SectionDigger( SectionHdrDigger& _sectionhdr, std::istream& _source ) {
    m_field.size   = _sectionhdr.field( "SizeOfRawData" ).getint<uint32_t>( _source );
    m_field.offset = _sectionhdr.field( "PointerToRawData" ).getint<uint32_t>( _source );
  }

}; /* end of namespace PEF */

}; /* end of namespace UWT */
