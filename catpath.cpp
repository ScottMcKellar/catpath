/*
    catpath -- program to add one or more directories to a colon-separated list of directory
    paths, such as are used in UNIX and UNIX-like operating systems for PATH, LD_LIBRARY_PATH,
    and the like.

    There are three reasons to use catpath instead of simple shell scripting to build directory
    path lists:

    1. catpath avoids duplications; i.e. it won't include any given directory path more than once
    (this feature may be suppressed with the -d option).

    2. If a directory path starts with the root directory, catpath will verify the existence of the
    directory before including it in the list (this feature may be suppressed with the -f option).

    3, catpath eliminates extra colons that sometimes sneak into manually built path lists.

    It is possible to do these things with shell scripts, but cumbersome.  catpath makes it easy.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Copyright 2011 Scott McKellar mck9@swbell.net
*/

#include <libgen.h>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

namespace std {}
using namespace std;

// To represent what the command line is asking for:
struct PathArgs
{
	vector< string > arg_vec;      // individual paths from command line
	char sep;                      // character used to separate paths
	bool allow_dups;               // If true, allow duplicates
	bool force;                    // If true, don't check for existence
	bool help;                     // If true, display help text only
	bool expand;                   // If true, expand tilde to home directory
};

static void build_path( const PathArgs & path_args, string & path );
static void get_opts( int argc, char ** argv, PathArgs & path_args );
static void parse_path( const char * path, vector< string > & vec, char sep );
static bool is_dir( const string & dirname );
static void show_help( const char * name );

static const char DEFAULT_SEP = ':';   // Separator used to separate directory paths

int main(int argc, char **argv)
{
    int rc = 0;

	try
	{
		// Parse the command line options.

		PathArgs path_args;
		get_opts( argc, argv, path_args );
		if( path_args.help )
		{
			show_help( basename( argv[ 0 ] ) );
			return 0;
		}

		// Parse the non-option command line arguments.  Each one is a list of one or more
		// directory paths, separated by the designated separator character.  There may
		// also be extraneous separator characters, which we shall ignore.  Dissect each
		// path list and load the individual paths into an array of strings.

		char ** argp = argv + optind;
		while( *argp )
		{
			parse_path( *argp, path_args.arg_vec, path_args.sep );
			++argp;
		}

		// Reassemble the paths into a path list, and write it to standard output.

		string path;
		build_path( path_args, path );
		cout << path << '\n';
	}
	catch( runtime_error & excp )
	{
		try
		{
			cerr << basename( argv[ 0 ] ) << ": " << excp.what() << '\n';
		}
		catch( ... ) { ; }
		rc = 1;
	}
	catch( exception & excp )
	{
		try
		{
			cerr << basename( argv[ 0 ] ) << ": Exception encountered: " << excp.what() << '\n';
		}
		catch( ... ) { ; }
		rc = 1;
	}

    return rc;
}

/* ---------------------------------------------------------------------------------
   Concatenate a collection of directory paths, separating them by a separator
   character, and (optionally) eliminating duplicates as you go.  Optionally: if
   a fully qualified path specifies a directory that doesn't exist, don't include
   it in the output list.
   ------------------------------------------------------------------------------ */
static void build_path( const PathArgs & path_args, string & path )
{
	path.clear();
	
	set< string > dir_set;

	vector< string >::const_iterator iter = path_args.arg_vec.begin();
	vector< string >::const_iterator end  = path_args.arg_vec.end();

	string curr_path;
	const char * home = NULL;
	
	while( iter != end )
	{
		curr_path = *iter;
		if( path_args.expand && '~' == curr_path.at( 0 ) && '/' == curr_path.at( 1 ) )
		{
			// Replace the tilde with the user's home directory

			if( NULL == home )
				home = getenv( "HOME" );

			if( home )
			{
				curr_path.erase( 0, 1 );
				curr_path.insert( 0, home );
			}
		}

		if( ! path_args.force && '/' == curr_path.at( 0 ) )
		{
			// If the -f option is not in effect, verify that the specified
			// directory exists and is accessible.  We do this check only
			// for fully qualified directory paths.

			if( ! is_dir( curr_path ) )
			{
				++iter;
				continue;   // Skip this entry and go on to the next one
			}
		}

		if( ! path_args.allow_dups )
		{
			if( dir_set.find( curr_path ) != dir_set.end() )
			{
				++iter;
				continue;  // We alredy included this one; skip it
			}

			dir_set.insert( curr_path );
		}

		if( ! path.empty() )
			path += path_args.sep;

		path += curr_path;

		++iter;
	}
}

/* ---------------------------------------------------------------------------------
   Parse the command-line options.
   ------------------------------------------------------------------------------ */
static void get_opts( int argc, char ** argv, PathArgs & path_args )
{
	// Apply defaults:

	path_args.sep = DEFAULT_SEP;
	path_args.allow_dups = false;
	path_args.force = false;
	path_args.help = false;
	path_args.expand = false;

	// Define valid option characters

	const char optstring[] = ":dfhs:x";

	// Suppress error messages from getopt()

	opterr = 0;

	// Examine each command line option in turn

	bool sep_found = false;
	int opt;
	while( ( opt = getopt( argc, argv, optstring ) ) != -1 )
	{
		switch( opt )
		{
			case 'd' :
				path_args.allow_dups = true;
				break;
			case 'f' :
				path_args.force = true;
				break;
			case 'h' :
				path_args.help = true;
				break;
			case 's' :
			{
				if( '\0' == *optarg )
					throw runtime_error( string(
						"Specified separator is an empty string" ) );
				else if( '\0' != *( optarg + 1 ) )
					throw runtime_error( string(
						"Specified separator consists of multiple characters" ) );

				char sep = *optarg;
				if( sep_found && sep != path_args.sep )
					throw runtime_error( string(
						"Conflicting specifications for separator character" ) );
				path_args.sep = sep;
				sep_found = true;
				break;
			}
			case 'x' :
				path_args.expand = true;
				break;
			case ':' :
			{
				string msg( "Required argument missing on -" );
				msg += static_cast< char >( optopt );
				msg += " option";
				throw runtime_error( msg );
			}
			case '?' :
			{
				string msg( "Invalid option -" );
				msg += static_cast< char >( optopt );
				msg += " on command line";
				throw runtime_error( msg );
			}
			default :
			{
				string msg( "Internal error: unexpected value \'");
				msg += static_cast< char >( optopt );
				msg += "\' of optopt";
				throw runtime_error( msg );
			}
		}
	}
}

/* ---------------------------------------------------------------------------------
   Parse a string as a separated list of directory paths.  Append each directory
   path to an existing vector of strings.
   ------------------------------------------------------------------------------ */
static void parse_path( const char * path, vector< string > & vec, char sep )
{
	const char * start = path;
	const char * stop = NULL;

	if( NULL == path || '\0' == *path )
		return;

	bool finished = false;
	while( ! finished )
	{
		// Skip leading separators
		while( sep == *start )
			++start;
		if( ! *start )
			break;

		// Look for the next separator, or end-of-string
		stop = start + 1;
		while( *stop && *stop != sep )
			++stop;

		// Add to the vector
		vec.push_back( string( start, stop ) );

		start = stop;
	}
}

/* ---------------------------------------------------------------------------------
   Return true if the input string identifies an existing directory.  Return false
   if it doesn't exist, or if it isn't a directory, or if search permission is
   denied for one of the parent directories.
   ------------------------------------------------------------------------------ */
static bool is_dir( const string & dirname )
{
	struct stat buf;

	if( 0 == stat( dirname.c_str(), &buf ) && S_ISDIR( buf.st_mode ) )
		return true;
	else
		return false;   // Doesn't exist, or isn't a directory, or isn't accessible
}

static void show_help( const char * name )
{
	cout << "Usaage: " << name << " [OPTION...] PATH...\n\n";

	cout << "Concatenate directory paths into a list.  Each PATH is a list\n";
	cout << "of one or more directory paths, separated by a designated\n";
	cout << "separator character (see -s option).\n\n";

	cout << "  -d  allow duplicate paths\n";
	cout << "  -f  include a path even if the directory doesn't exist\n";
	cout << "  -h  display this help text\n";
	cout << "  -s  specify a character used to separate paths\n";
	cout << "      (defaults to \'" << DEFAULT_SEP << "\')\n";
	cout << "  -x  replace tildes ('~') with the user's home directory\n\n";

	cout << "Report " << name << " bugs to mck9@swbell.net\n";
}
