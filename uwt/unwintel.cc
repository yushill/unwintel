/***************************************************************************
                              uwt/unwintel.cc
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

#include <uwt/launchbox.hh>
#include <uwt/trans/cfg_testbench.hh> /* for cfg testing purpose */
#include <uwt/utils/misc.hh>
#include <uwt/utils/logchannels.hh>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern char **environ;

struct UnWinTel : public UWT::LaunchBox
{
  struct Command : public Switch
  {
    virtual void exec( UnWinTel& uwt ) const = 0;
  };

  void commands( Command::Args& args )
  {
    {
      static struct : Command
      {
        char const* name() const { return "setup"; }
        void describe( std::ostream& sink ) const { sink << "Setup a new empty project (in cwd)."; }
        void exec( UnWinTel& uwt ) const { uwt.setup(); }
      } _;

      if (args.match(_)) return;
    }

    {
      static struct : Command
      {
        char const* name() const { return "make"; }
        void describe( std::ostream& sink ) const { sink << "Build the current project."; }
        void exec( UnWinTel& uwt ) const { uwt.make(); }
      } _;

      if (args.match(_)) return;
    }

    {
      static struct : Command
      {
        char const* name() const { return "run"; }
        void describe( std::ostream& sink ) const { sink << "Launch the transmulation."; }
        void exec( UnWinTel& uwt ) const { uwt.run(); }
      } _;

      if (args.match(_)) return;
    }

    {
      static struct : Command
      {
        char const* name() const { return "help"; }
        void describe( std::ostream& sink ) const { sink << "Show help and exit immediately."; }
        void exec( UnWinTel& uwt ) const { throw Abort( 0 ); }
      } _;

      if (args.match(_)) return;
    }

    {
      static struct : Command
      {
        char const* name() const { return "shrc"; }
        void describe( std::ostream& sink ) const { sink << "dumps environ setup for sh-compatible shells"; }
        void exec( UnWinTel& uwt ) const { uwt.dump_config( std::cout, sh_style ); }
      } _;

      if (args.match(_)) return;
    }

    {
      static struct : Command
      {
        char const* name() const { return "cshrc"; }
        void describe( std::ostream& sink ) const { sink << "dumps environ setup for csh-compatible shells"; }
        void exec( UnWinTel& uwt ) const { uwt.dump_config( std::cout, csh_style ); }
      } _;

      if (args.match(_)) return;
    }

    {
      static struct : Command
      {
        char const* name() const { return "test"; }
        void describe( std::ostream& sink ) const { sink << "internal test"; }
        void exec( UnWinTel& uwt ) const
        {
          UWT::CFG_TestBench::test( 0x1000000 );
        }
      } _;
      if (args.match(_)) return;
    }
  }

  /** setup
   *
   * Creates a new empty project (Generate make and config files).
   */
  void setup()
  {
    struct MakeDir
    {
      void operator() ( char const* dirname )
      {
        if ((mkdir( dirname, 0777) == 0) or (errno == EEXIST)) return;
        std::cerr << "LaunchBox: can't create directory " << dirname << std::endl;
        throw Abort( 1 );
      }
    } make_directory;
    make_directory( "logs" );
    make_directory( "shared" );
    make_directory( "main_exe" );
    {
      std::ofstream sink( "Config" );
      dump_config( sink, uwt_style );
    }
    {
      std::ofstream sink( "Makefile" );
      std::ifstream source( framework + "/templates/Makefile.src" );
      for (std::string line; getline( source, line );)
        sink << line << '\n';

      std::vector<char const*> files = {"functions.inc","modules.inc","breakpoint.hh","main.cc"};
      for (char const* filename: files) {
        sink  << "main_exe/" << filename << ":\n\tcp $(UWT_FRAMEWORK)/templates/" << filename << ".src $@\n\n";
      }
      sink.close();
      for (char const* filename: files) {
        make( (std::string( "main_exe/" ) + filename).c_str() );
      }
    }
  }

  void make( char const* rule = 0 )
  {
    std::string cmd( "make" );

    if (quiet_make)
      cmd += " -s";
    if (rule)
      cmd = cmd + ' ' + rule;

    int status = system( cmd.c_str() );
    if (not WIFEXITED( status ))
      {
        std::cerr << "Error in make invocation (was: " << cmd << ")\n";
        throw Abort( 1 );
      }

    status = WEXITSTATUS( status );
    if (status)
      throw Abort( status );
  }

  void help( std::ostream& sink )
  {
    sink << "Usage: unwintel [<parameters>*] <command>*\n";

    struct DumpSwitch : Command::Args
    {
      DumpSwitch( std::ostream& _sink ) : sink(_sink) {} std::ostream& sink;
      virtual bool match( Switch& s )
      {
        s.describe( sink << s.name() << ":\t" );
        sink << '\n';
        return false;
      }
    } ds( sink );

    sink << "\nparameters:\n";

    parameters( ds );

    sink << "\ncommands:\n";

    commands( ds );

    // prototype();
    // description();
    // options();
  }


  void run()
  {
    make();

    // Executing product
    char const* args[] = { "[LaunchBox: strap]", 0 };
    execvp( getenv( "APPNAME" ), (char* const*)( args ) );
    std::cerr << "LaunchBox: execvp failure (app)..." << std::endl;
    throw Abort( 1 );
  }

  enum config_style_t { uwt_style=0, sh_style, csh_style };

  void dump_config( std::ostream& sink, config_style_t config_style = uwt_style )
  {
    struct PA : Switch::Args
    {
      PA( std::ostream& _sink, config_style_t _cfgsty ) : sink(_sink), cfgsty(_cfgsty) {} std::ostream& sink; config_style_t cfgsty;
      virtual bool match( Switch& s )
      {
        char const* key = s.name();
        char const* val = LaunchBox::getenv( key );

        char assign = (cfgsty == csh_style) ? ' ' : '=';
        char const* declare = (cfgsty == csh_style) ? "setenv UWT_" : (cfgsty == sh_style) ? "export UWT_" : "";

        s.describe( sink << "# " );
        sink << '\n';
        if (val)
          sink << declare << key << assign << val;
        else
          sink << "# " << declare << key << assign;
        sink << '\n';

        return false;
      }
    } pa( sink, config_style );

    parameters( pa );

    if (config_style != uwt_style)
      internal_parameters( pa );
  }

  void execute( std::string const& arg )
  {
    struct _ : Switch::Args
    {
      virtual bool match( Switch& s )
      {
        if (arg.compare(s.name()) != 0)
          return false;
        found = &dynamic_cast<Command&>( s );

        return true;
      }
      _(std::string const& _arg) : arg(_arg), found(0) {}
      std::string const& arg; Command* found;
    } cmd( arg );

    commands( cmd );

    if (not cmd.found)
      {
        std::cerr << "Unknown command: " << arg << std::endl;
        throw Abort( 1 );
      }

    cmd.found->exec( *this );
  }
};


int
main( int argc, char** argv )
{
  UnWinTel uwt;

  try
    {
      // #1 Locate parent directory of this executable (FRAMEWORK)
      {
        std::string execpath = strchr( argv[0], '/' ) ?
          strx::abspath( argv[0] ) : strx::readlink( "/proc/self/exe" );
        uwt.configure( "FRAMEWORK=" +  execpath.substr( 0, execpath.find_last_of('/') ) );
      }

      // #2 Read arguments from environment variables
      for (char **envp = environ; *envp; ++envp)
        if (char const* arg = UWT::popfirst("UWT_", *envp))
          uwt.configure( arg, true );

      // #3 Read arguments from configuration file
      for (std::ifstream cfg( "Config" ); cfg;)
        {
          std::string line;
          getline( cfg, line );
          uintptr_t start = line.find_first_not_of("\f\n\r\t\v ");
          if (start==std::string::npos) continue;
          if (start > 0) line = line.substr( start );
          if (line[0] == '#')           continue;
          uwt.configure( line );
        }

      // #4 Read arguments and commands from command line
      std::vector<std::string> commands;
      for (int argi = 1; argi < argc; ++argi)
        {
          std::string arg( argv[argi] );
          if ((arg == "--help") or arg == "-h") arg = "help";

          if (arg.find( '=' ) != std::string::npos)
            uwt.configure( arg );
          else
            commands.push_back( arg );
        }

      // #5 Process special arguments
      uwt.setenv( "APPNAME", uwt.default_app_name() );
      uwt.setenv( "MODULE", "main_exe" );
      uwt.setenv( "STEP", "0" );

      // #6 execute commands
      for (std::string const& cmd: commands)
        {
          uwt.execute( cmd );
        }
    }

  catch (UWT::LaunchBox::Abort const& aex )
    {
      uwt.help( aex.code ? std::cerr : std::cout );
      return aex.code;
    }

  return 0;
}


