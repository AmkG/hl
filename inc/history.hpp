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
	typedef std::vector<Object::ref> Item;
	class InnerRing : public boost::circular_buffer<Item> {
	public:
		static const size_t breadth = 32;  // TODO: get size from user
		InnerRing(void) : boost::circular_buffer<Item>(breadth) { }
		void repeat_set_capacity(void) {
			set_capacity(breadth);
		}
		void traverse_references(GenericTraverser*);
	};

	// push a list of the last functions called in the process stack
	void to_list(Process & proc);
	// called at each entry of a function
	void entry(void);

	friend class Process;
};

#endif // HISTORY_H
