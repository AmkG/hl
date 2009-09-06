#ifndef HISTORY_H
#define HISTORY_H

#include"objects.hpp"

#define BOOST_CB_DISABLE_DEBUG

#include <boost/circular_buffer.hpp>
#include <vector>

class Process;
class ProcessStack;
class GenericTraverser;

/*
 * Wrapper around Process, converts history information
 */
class History {
private:
	ProcessStack& stack;

	History(void); //disallowed!
	History(ProcessStack& nstack) : stack(nstack) { }

public:
	// push a list of the last functions called in the process stack
	void to_list(Process & proc);
	// called at each entry of a function
	void entry(void);

	friend class Process;
};

/*
 * Stored at each kontinuation
 */
class HistoryInnerRing {
private:
	typedef std::vector<Object::ref> Item
	boost::circular_buffer<Item> items;

	void add_item(ProcessStack&);

public:
	friend class History;
	HistoryInnerRing(void) : items(32) { } /*TODO: get length from user*/
};

#endif // HISTORY_H
