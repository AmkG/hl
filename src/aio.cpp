#include"all_defines.hpp"

#include"aio.hpp"
#include"heaps.hpp"
#include"types.hpp"
#include"objects.hpp"
#include"processes.hpp"
#include"workers.hpp"
#include"bytecodes.hpp"
#include"symbols.hpp"

/*Generic code for AIO*/
/*This contains code that is shared across AIO implementations*/

/*internal function for sending a message to a process*/
/*
preconditions:
	stack.top() = message to send
postconditions:
	stack.top() has been popped
*/
static inline void send_message_to(Process& host, Process* P, ProcessStack& stack) {
	/*prepare message*/
	ValueHolderRef m;
	ValueHolder::copy_object(m, stack.top());
	stack.pop();

	bool is_waiting = 0;
	/*keep sending until we definitely go through any locks or whatnot*/
	while(!P->mailbox().receive_message(m, is_waiting)) /*do nothing*/;
	if(is_waiting) {
		/*Use multipush on the hosting process to
		push the processes on the workqueue when
		the hosting process' timeslice expires.
		*/
		host.add_to_multipush(P);
	}
}

ProcessInvoker::ProcessInvoker(Process* nP) : P(nP) {
}
ProcessInvoker::~ProcessInvoker() {
}

static inline void recreate_event_push(
		Heap& hp,
		ProcessStack& stack,
		boost::shared_ptr<Event>& event,
		Process* P) {
	{
		HlPid *pid = hp.create<HlPid>();
		pid->process = P;
		stack.push(Object::to_ref<Generic*>(pid));
	}
	HlEvent* ev = hp.create<HlEvent>();
	ev->p = event;
	ev->hl_pid = stack.top(); stack.pop();
	stack.push(Object::to_ref<Generic*>(ev));
}

void ProcessInvoker::io_respond(
		Process& host,
		boost::shared_ptr<Event>& event,
		boost::shared_ptr<std::vector<unsigned char> >& dat) {
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	recreate_event_push(hp, stack, event, P);
	/*any data?*/
	if(!dat || dat->size() == 0) {
		stack.push(Object::nil());
	} else {
		BinObj* e = BinObj::create(hp, dat);
		stack.push(Object::to_ref<Generic*>(e));
	}
	bytecode_cons(host, stack);

	send_message_to(host, P, stack);

}

void ProcessInvoker::nil_respond(
		Process& host,
		boost::shared_ptr<Event>& event) {
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	recreate_event_push(hp, stack, event, P);
	stack.push(Object::nil());
	bytecode_cons(host, stack);

	send_message_to(host, P, stack);

}

void ProcessInvoker::accept_respond(
		Process& host,
		boost::shared_ptr<Event>& event,
		boost::shared_ptr<IOPort>& new_socket) {
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	recreate_event_push(hp, stack, event, P);
	HlIOPort* io = hp.create<HlIOPort>();
	io->p = new_socket;
	stack.push(Object::to_ref<Generic*>(io));
	bytecode_cons(host, stack);

	send_message_to(host, P, stack);

}

void ProcessInvoker::connect_respond(
		Process& host,
		boost::shared_ptr<Event>& event,
		boost::shared_ptr<IOPort>& new_socket) {
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	recreate_event_push(hp, stack, event, P);
	HlIOPort* io = hp.create<HlIOPort>();
	io->p = new_socket;
	stack.push(Object::to_ref<Generic*>(io));
	bytecode_cons(host, stack);

	send_message_to(host, P, stack);

}

void ProcessInvoker::sleep_respond(
		Process& host,
		boost::shared_ptr<Event>& event,
		size_t time) {
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	recreate_event_push(hp, stack, event, P);
	stack.push(Object::to_ref<int>(time));
	bytecode_cons(host, stack);

	send_message_to(host, P, stack);

}

void ProcessInvoker::error_respond(
		Process& host,
		boost::shared_ptr<Event>& event,
		std::string const& msg) {
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	recreate_event_push(hp, stack, event, P);
	/*slow lookup is OK, we don't expect error handling
	to be fast.
	*/
	stack.push(Object::to_ref(symbols->lookup("<hl>i/o")));
	/*assume ASCII string for now*/
	for(size_t i = 0; i < msg.size(); ++i) {
		stack.push(Object::to_ref(UnicodeChar(msg[i])));
	}
	HlString::stack_create(hp, stack, msg.size());

	bytecode_tag(host, stack);
	bytecode_cons(host, stack);

	send_message_to(host, P, stack);
}

