#include"all_defines.hpp"

#include<iostream>
#include<sstream>
#include<string>
#include<vector>

#include<errno.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<stdlib.h>

/*
 * print a fail and exit
 */
void fail(const char* f) {
	std::cerr << "hl: " << f << std::endl;
	exit(1);
}

/*
 * determines if the given file exists
 */
bool find_file(std::string path) {
	char const* c_path = path.c_str();
	struct stat buf;
	if(stat(c_path, &buf) == 0) {
		return 1;
	} else {
		/*assume that all errors are "file not found"*/
		errno = 0;
		return 0;
	}
}

/*
 * given argv[0], attempts to extract the path of the
 * executable.
 */
std::string get_argv0_path(std::string const& argv0) {
	/*reverse iterators can be confusing to handle when
	converting back-and-forth with forward iterators,
	so we just take advantage of bidirectionality
	*/
	std::string::const_iterator slash = argv0.end();
	do {
		--slash;
		if(*slash == '/' || *slash == '\\') {
			/*limit it to just past the / or \ */
			++slash;
			/*copy*/
			return std::string(argv0.begin(), slash);
		}
	} while(slash != argv0.begin());
	return std::string("");
}

#ifndef EXE_EXT
	#define EXE_EXT
#endif

/*
 * searches for the `hlvma` executable
 */
std::string find_hlvma(std::string const& argv0) {
	/*first, find it in the directory we are in*/
	std::string argv0_path = get_argv0_path(argv0);
	if(argv0_path != "") {
		std::string path = argv0_path + ("hlvma" EXE_EXT);
		if(find_file(path)) {
			return path;
		}
	}
	/*look for it in HLVMA_LIBEXECDIR*/
	char const* c_hlvma_libexecdir = getenv("HLVMA_LIBEXECDIR");
	if(c_hlvma_libexecdir) {
		std::string hlvma_libexecdir = c_hlvma_libexecdir;
		std::string path = hlvma_libexecdir + ("/hlvma" EXE_EXT);
		if(find_file(path)) {
			return path;
		}
	}
	/*Finally, look for it in the LIBEXECDIR*/
	#ifdef LIBEXECDIR
	{
		std::string path = ((std::string) LIBEXECDIR) + ("/hlvma" EXE_EXT);
		if(find_file(path)) {
			return path;
		}
	}
	#endif
	fail("Unable to find `hlvma` executable; please check your installation");
}

/*
 * searches for the hlvma boot files, hlvma?.hlbc
 * returns the directory where they are found
 */
std::string find_bootdir(std::string const& argv0) {
	/*find it at ../hl2b/ from the directory we are in*/
	std::string argv0_path = get_argv0_path(argv0);
	if(argv0_path != "" && argv0_path != "/") {
		std::string path = argv0_path + "../hl2b/";
		std::string file = path + "hlvma0.hlbc";
		if(find_file(file)) {
			return path;
		}
	}
	/*find it at the given HLVMA_BOOTDIR*/
	char const* c_hlvma_bootdir = getenv("HLVMA_BOOTDIR");
	if(c_hlvma_bootdir) {
		std::string hlvma_bootdir = c_hlvma_bootdir;
		std::string file = hlvma_bootdir + "/hlvma0.hlbc";
		if(find_file(file)) {
			return hlvma_bootdir;
		}
	}
	/*find it at the given DATADIR*/
	#ifdef DATADIR
	{
		std::string path = (DATADIR "/hlvmaboot/");
		std::string file = path + "hlvma0.hlbc";
		if(find_file(file)) {
			return path;
		}
	}
	#endif
	fail("Unable to find `hlvma` bootstrap directory; please check your installation");
}

void cpp_exec(std::vector<std::string> const& argv) {
	typedef std::vector<std::string> argv_t;
	/*construct a basic C array of char* */
	char** c_argv = new char*[argv.size() + 1];
	char** c_argvp = c_argv;
	for(argv_t::const_iterator ss = argv.begin(); ss != argv.end(); ++ss) {
		std::string const& s = *ss;
		char* c_argvn = new char[s.size() + 1];
		*c_argvp = c_argvn;
		for(
				std::string::const_iterator cs = s.begin();
				cs != s.end();
				++cs) {
			*c_argvn = *cs;
			++c_argvn;
		}
		*c_argvn = (char)0;
		++c_argvp;
	}
	*c_argvp = (char*)0;
	execv(c_argv[0], c_argv);
	fail("unable to execute main program");
}

/*
 * wraps the string and indents any wrapped substring:
 *   "foo bar nitz koo quux qux niaw woof bar foo foobar."
 * =>
 *   foo bar nitz koo quux qux niaw woof
 *       bar foo foobar.
 * we use this instead of manually handling word-wrap so
 * that we can just splice in gettext() without worrying
 * about having a sentence split across strings.
 */
std::string wrap(std::string const& s) {
	static const int line_width = 60;
	if(s.size() < line_width) return s;

	std::stringstream oss;
	std::string::const_iterator cs = s.begin();
	int remaining = s.size();
	int width = line_width;

	bool first = 1;
	do {
		if(remaining < width) {
			for(; cs != s.end(); ++cs) {
				oss << *cs;
			}
			break;
		} else {
			std::string::const_iterator ics = cs + width;
			int line = width;
			/*note: if a word is more than `width' characters
			long, we will have problems.  We don't expect any
			human language word to take up more than 57 or so
			bytes.
			*/
			while(*ics != ' ') {
				--ics; --line;
			}
			for(; cs != ics; ++cs) {
				oss << *cs;
			}
			remaining -= line;
		}
		if(first) {
			width -= 3;
			first = 0;
		}
		oss << std::endl << "   ";
	} while(1);
	return oss.str();
}


int main(int argc, char* argv[]) {
	/*check options*/
	if(argc > 1 && argv[1][0] == '-') {
		std::string argv1 = argv[1];
		if(argv1 == "--version") {
			std::cout << "hl virtual machine A " << PACKAGE_VERSION << std::endl;
			exit(0);
		}
		if(argv1 == "--help") {
			std::cout
				<< "Usage:" << std::endl
				<< "    hl [--version] [ignored-arguments*]"
				<< std::endl
				<< "    hl [--help] [ignored-arguments*]"
				<< std::endl
				<< "    hl [--] [script [script-arguments]]"
				<< std::endl << std::endl
				<< wrap("`hl', without any script argument, "
					"boots the virtual machine REPL.  "
					"You can exit the REPL by typing "
					"`(exit)' at the prompt."
				)
				<< std::endl
				<< wrap("`hl -- [script]' allows you to "
					"launch a script file that begins "
					"with a `-' character."
				)
				<< std::endl
				<< wrap("`hl --version' prints the hl virtual "
					"machine name and version to standard "
					"output, then exits."
				)
				<< std::endl
				<< wrap("`hl --help' prints this help, then exits.")
				<< std::endl
			;
			exit(0);
		}
		/*if the argument is "--", just shift arguments and continue*/
		if(argv1 == "--") {
			--argc;
			for(int i = 1; i < argc; ++i) {
				argv[i] = argv[i + 1];
			}
			goto process_args;
		}
		/*unknown option*/
		fail("Given option was not recognized as a valid option");
	}

process_args:
	std::vector<std::string> new_argv;
	std::string argv0 = argv[0];
	new_argv.push_back(find_hlvma(argv0));
	new_argv.push_back("--bootdir");
	new_argv.push_back(find_bootdir(argv0));
	if(argc > 1) {
		new_argv.push_back("--args");
		for(int i = 1; i < argc; ++i) {
			new_argv.push_back(argv[i]);
		}
	}
	cpp_exec(new_argv);
}

