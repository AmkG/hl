#include"all_defines.hpp"

#include"aio.hpp"
#include"heaps.hpp"
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


