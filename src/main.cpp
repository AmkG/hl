/*
 * Example hl driver -- execute bytecode
 */
#include "all_defines.hpp"

#include <iostream>
#include <fstream>
#include <cstdlib>

#include "reader.hpp"
#include "executors.hpp"
#include "assembler.hpp"
#include "symbols.hpp"
#include "types.hpp"
#include "workers.hpp"
#include "assembler.hpp"
#include "mutexes.hpp"

using namespace std;

void throw_OverBrokenHeart(Generic*) {
  throw_HlError("overbrokenheart");
}

/*
 *Command line option management -- consider moving it to its own file
 */

/* an abstract option */
class Option {
public:
	// parse a single option, consuming input
	// i is the position of the option name in argv
	// on error, print a report on stderr and return false
	virtual bool parse_option(char **argv, int argc, int & i) = 0;
	// return the name of the option
	virtual const char* name() = 0;
	// print option usage on stdout
	virtual void usage() = 0;
};

/* Command line options parser */
class OptionParser {
private:
	char **argv;
	int argc;
	std::map<std::string, Option*> opts;
	void usage();

public:
	OptionParser() : argv(argv), argc(argc) {}

	// register a new option
	void add_option(Option *o) {
		opts[o->name()] = o;
	}
	// parse all the options, report an error on stderr if it fails
	// return true on success
	bool parse(char **argv, int argc);

	// tell if the string is an option string
	static bool is_option(char *str) {
		return strlen(str)>2 && str[0]=='-' && str[1]=='-';
	}
};

bool OptionParser::parse(char **argv, int argc) {
	bool fail = false;
	for (int i = 1; i < argc && !fail; ++i) {
		std::string name = argv[i];
		std::map<std::string, Option*>::iterator it = opts.find(name);
		if (it != opts.end()) {
			if (!it->second->parse_option(argv, argc, i)) {
				fail = true;
			}
		} else {
			std::cerr << "Unknown option " << name << std::endl;
			fail = true;
		}
	}
	if (fail) {
		usage();
	}

	return !fail;
}

void OptionParser::usage() {
	cout << "Usage:" << endl;
	cout << "hl [options ...]" << endl;
	cout << "Available options are:" << endl;
	for (std::map<std::string, Option*>::iterator it = opts.begin();
	     it != opts.end(); ++it) {
		it->second->usage();
	}
}

/* --bc f1 [f2 ...] */
class BytecodeOption : public Option {
private:
	std::vector<std::string> files;
public:
	virtual bool parse_option(char **argv, int argc, int & i);

	virtual const char* name() {
		return "--bc";
	}

	std::vector<std::string> const& get_files() {
		return files;
	}

	virtual void usage();
};

bool BytecodeOption::parse_option(char **argv, int argc, int & i) {
	while (i+1 < argc && !OptionParser::is_option(argv[i+1])) {
		files.push_back(argv[i+1]);
		++i;
	}
	if (files.size() == 0) {
		std::cerr << "Error: no file specified after " 
			  << name() << std::endl;
		return false;
	} else {
		return true;
	}
}

void BytecodeOption::usage() {
	cout << name() << " filename1 [filename2 ...]:\n";
	cout << "\texecute bytecode from the specified files\n";
}

/* --help */
class HelpOption : public Option {
public:
	virtual bool parse_option(char **argv, int argc, int & i) {
		// always fail, this way OptionParser will print the usage
		return false;
	}

	virtual const char* name() {
		return "--help";
	}

	virtual void usage();
};

void HelpOption::usage() {
	cout << "--help\n\tthis help\n";
}

/*--------------------------------------------------------------------------
Multifile bootstrap
--------------------------------------------------------------------------*/

void load_into_process(Process& proc, std::string const& file) {
	{
		ifstream in(file.c_str());
		if (!in) {
			cerr << "Can't open file: " << file << endl;
			exit(2);
		}
		read_sequence(proc, in);
	}
	assembler.go(proc);
	Closure *k = Closure::NewClosure(proc, 0);
	k->codereset(proc.stack.top());
	proc.stack.top() = Object::to_ref(k);
	proc.stack.restack(1);
}

/*mutex is not strictly necessary except to shut helgrind up*/
AppMutex boot_next_mtx;
std::vector<std::string>::const_iterator boot_next;
std::vector<std::string>::const_iterator boot_end;

bool GoNextBoot::run(Process& proc, size_t& reductions) {
	std::vector<std::string>::const_iterator it;
	{AppLock l(boot_next_mtx);
		it = boot_next;
		if(it == boot_end) {
			goto construct_halting;
		}
		++boot_next;
	}
	load_into_process(proc, *it);
	return true;
construct_halting:
	proc.stack.push(Assembler::inline_assemble(proc, "(<bc>halt)"));
	Closure* k = Closure::NewClosure(proc, 0);
	k->codereset(proc.stack.top());
	proc.stack.top() = Object::to_ref(k);
	proc.stack.push(proc.stack.top(2));
	proc.stack.restack(2);
	return true;
}

/*--------------------------------------------------------------------------
Main
--------------------------------------------------------------------------*/

int main(int argc, char **argv) {
	OptionParser opt;
	HelpOption help;
	BytecodeOption bytecodes;
	opt.add_option(&help);
	opt.add_option(&bytecodes);

	if (!opt.parse(argv, argc)) {
		return 1;
	}

	#ifndef single_threaded
		single_threaded = 1;
	#endif

	initialize_globals();

	Process *p;
	Process *Q;
	size_t timeslice;
	timeslice = 128;

	p = new Process();
	while(execute(*p, timeslice, Q, 1) == process_running); // init phase
	delete p;

	#ifndef single_threaded
		single_threaded = 0;
	#endif

	std::vector<std::string> const& files = bytecodes.get_files();	
	if(files.empty()) {
		cerr << "Nothing to do; please try `hlvma --help' for how to execute hlvma" << endl;
		exit(1);
	}

	std::vector<std::string>::const_iterator it = files.begin();
	{AppLock l(boot_next_mtx);
		boot_next = it; ++boot_next;
		boot_end = files.end();
	}

	try {
		p = new Process();
		load_into_process(*p, *it);
		// process will be deleted by workers
		AllWorkers &w = AllWorkers::getInstance();
		ValueHolderRef rv;
		w.initiate(3, p, rv);
		cout << rv.value() << endl; // print return value
	} catch(HlError& h) {
		cerr << "Error: " << h.err_str() << endl;
	}

	return 0;
}
