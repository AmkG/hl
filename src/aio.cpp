#include"all_defines.hpp"

#include"aio.hpp"
#include"heaps.hpp"
#include"types.hpp"
#include"objects.hpp"
#include"processes.hpp"
#include"workers.hpp"

/*Generic code for AIO*/
/*This contains code that is shared across AIO implementations*/

static inline void send_message_to(Process* p, ValueHolderRef& v) {
	bool is_waiting = 0;
	/*keep sending until we definitely go through any locks or whatnot*/
	while(!p->receive_message(v, is_waiting)) /*do nothing*/;
	if(is_waiting) {
		workers->workqueue_push(p);
	}
}

ProcessInvoker::ProcessInvoker(Process* nP) : P(nP) {
	/*TODO: notify workers of one more process-gc root*/
}
ProcessInvoker::~ProcessInvoker() {
	/*TODO: notify workers of loss of root*/
}

void ProcessInvoker::io_respond(
		boost::shared_ptr<IOPort> port,
		std::vector<unsigned char> const& dat) {
	/*Create a heap to build the response from*/
	Process hp; ProcessStack& stack = hp.stack;
	/*build objects*/
	/*TODO: first define hl-side type for IO port and events*/

}

