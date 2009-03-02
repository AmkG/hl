#include"all_defines.hpp"

#include"aio.hpp"
#include"heaps.hpp"
#include"types.hpp"
#include"objects.hpp"
#include"processes.hpp"
#include"workers.hpp"
#include"bytecodes.hpp"

/*Generic code for AIO*/
/*This contains code that is shared across AIO implementations*/

/*internal function for sending a message to a process*/
/*
preconditions:
	stack.top() = message to send
postconditions:
	stack.top() has been popped
*/
static inline void send_message_to(Process* P, ProcessStack& stack) {
	/*prepare message*/
	ValueHolderRef m;
	ValueHolder::copy_object(m, stack.top());
	stack.pop();

	bool is_waiting = 0;
	/*keep sending until we definitely go through any locks or whatnot*/
	while(!P->receive_message(m, is_waiting)) /*do nothing*/;
	if(is_waiting) {
		workers->workqueue_push(P);
	}
}

ProcessInvoker::ProcessInvoker(Process* nP) : P(nP) {
	/*TODO: notify workers of one more process-gc root*/
}
ProcessInvoker::~ProcessInvoker() {
	/*TODO: notify workers of loss of root*/
}

void ProcessInvoker::io_respond(
		Process& host,
		boost::shared_ptr<IOPort> port,
		boost::shared_ptr<std::vector<unsigned char> >& dat) {
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	HlIOPort* io = hp.create<HlIOPort>();
	io->p = port;
	stack.push(Object::to_ref<Generic*>(io));
	/*any data?*/
	if(!dat || dat->size() == 0) {
		stack.push(Object::nil());
	} else {
		BinObj* e = BinObj::create(hp, dat);
		stack.push(Object::to_ref<Generic*>(e));
	}
	bytecode_cons(host, stack);

	send_message_to(P, stack);

}

void ProcessInvoker::nil_respond(
		Process& host,
		boost::shared_ptr<IOPort> port) {
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	HlIOPort* io = hp.create<HlIOPort>();
	io->p = port;
	stack.push(Object::to_ref<Generic*>(io));
	stack.push(Object::nil());
	bytecode_cons(host, stack);

	send_message_to(P, stack);

}

void ProcessInvoker::accept_respond(
		Process& host,
		boost::shared_ptr<IOPort> socket,
		boost::shared_ptr<IOPort> new_socket){
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	HlIOPort* io = hp.create<HlIOPort>();
	io->p = socket;
	stack.push(Object::to_ref<Generic*>(io));
	io = hp.create<HlIOPort>();
	io->p = new_socket;
	stack.push(Object::to_ref<Generic*>(io));
	bytecode_cons(host, stack);

	send_message_to(P, stack);

}

void ProcessInvoker::connect_respond(
		Process& host,
		boost::shared_ptr<Event> event,
		boost::shared_ptr<IOPort> new_socket) {
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	HlEvent* ev = hp.create<HlEvent>();
	ev->p = event;
	stack.push(Object::to_ref<Generic*>(ev));
	HlIOPort* io = hp.create<HlIOPort>();
	io->p = new_socket;
	stack.push(Object::to_ref<Generic*>(io));
	bytecode_cons(host, stack);

	send_message_to(P, stack);

}

void ProcessInvoker::sleep_respond(
		Process& host,
		boost::shared_ptr<Event> event,
		size_t time) {
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	HlEvent* ev = hp.create<HlEvent>();
	ev->p = event;
	stack.push(Object::to_ref<Generic*>(ev));
	stack.push(Object::to_ref<int>(time));
	bytecode_cons(host, stack);

	send_message_to(P, stack);

}

