/*
 * Example hl driver -- execute bytecode
 */
#include "all_defines.hpp"

#include <iostream>
#include <fstream>
#include <cstdlib>

#include "reader.hpp"
#include "executors.hpp"
#include "symbols.hpp"
#include "types.hpp"
#include "workers.hpp"

using namespace std;

void throw_HlError(const char *str) {
  cout << "Error: " << str << endl << flush;
  exit(1);
}

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

	std::vector<std::string>& get_files() {
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

int main(int argc, char **argv) {
	OptionParser opt;
	HelpOption help;
	BytecodeOption bytecodes;
	opt.add_option(&help);
	opt.add_option(&bytecodes);

	if (!opt.parse(argv, argc)) {
		return 1;
	}

	initialize_globals();

	Process *p;
	Process *Q;
	size_t timeslice;
	timeslice = 128;

	p = new Process();
	while(execute(*p, timeslice, Q, 1) == process_running); // init phase
	delete p;

	std::vector<std::string> files = bytecodes.get_files();	
	for (std::vector<std::string>::iterator it = files.begin(); 
	     it !=files.end(); ++it) {
		ifstream in(it->c_str());
		if (!in) {
			cerr << "Can't open file: " << *it << endl;
			return 2;
		}

		p = new Process();
		read_sequence(*p, in);
		assembler.go(*p);
		Closure *k = Closure::NewKClosure(*p, 0);
		k->codereset(p->stack.top()); p->stack.pop();
		p->stack.push(Object::to_ref(k)); // entry point
		// process will be deleted by workers
		AllWorkers &w = AllWorkers::getInstance();
		w.initiate(3, p);
		cout << p->stack.top() << endl; // print result
	}

	return 0;
}
