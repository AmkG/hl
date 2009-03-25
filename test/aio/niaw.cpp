/*
niaw is a program like the built-in cat: it
just transfers data from standard input to
standard output.  The difference is that it
uses the hl aio system.
*/
#include"all_defines.hpp"

#include"aio.hpp"

#include<boost/shared_ptr.hpp>

#include<vector>
#include<queue>
#include<stdexcept>
#include<iostream>

/*use an abstract base*/
class Process {
public:
	boost::shared_ptr<std::vector<unsigned char> > dat;
	bool io_responded;
	bool nil_responded;
	Process() : dat(), io_responded(0), nil_responded(0) { }
};

/*define some ProcessInvoker functions*/
ProcessInvoker::ProcessInvoker(Process* np) : P(np) { }
ProcessInvoker::~ProcessInvoker() { }
void ProcessInvoker::io_respond(
		Process& host,
		boost::shared_ptr<Event>& ev,
		boost::shared_ptr<std::vector<unsigned char> >& dat) {
	P->dat.swap(dat);
	P->io_responded = 1;
	P->nil_responded = 0;
}
void ProcessInvoker::nil_respond(
		Process& host,
		boost::shared_ptr<Event>& ev) {
	P->nil_responded = 1;
	P->io_responded = 0;
	P->dat.reset();
}
void ProcessInvoker::accept_respond(
		Process& host,
		boost::shared_ptr<Event>& ev,
		boost::shared_ptr<IOPort>& new_socket) {
	throw std::runtime_error("unexpected accept response!");
}
void ProcessInvoker::connect_respond(
		Process& host,
		boost::shared_ptr<Event>&, boost::shared_ptr<IOPort>& new_socket) {
	throw std::runtime_error("unexpected connect response!");
}
void ProcessInvoker::sleep_respond(
		Process& host,
		boost::shared_ptr<Event>&, size_t) {
	throw std::runtime_error("unexpected sleep response!");
}
void ProcessInvoker::system_respond(
		Process& host,
		boost::shared_ptr<Event>&, int term_code) {
	throw std::runtime_error("unexpected system response!");
}

void ProcessInvoker::error_respond(
		Process& host,
		boost::shared_ptr<Event>&, std::string const& err) {
	std::cerr << "an error occured: " << err;
	exit(1);
}


/*some globals*/
boost::shared_ptr<IOPort> input;
boost::shared_ptr<IOPort> output;

Process input_process;
boost::shared_ptr<ProcessInvoker> input_pi(
	new ProcessInvoker(&input_process)
);
Process output_process;
boost::shared_ptr<ProcessInvoker> output_pi(
	new ProcessInvoker(&output_process)
);
Process dummy_process;

bool stop;

/*event queue for event handler*/
std::queue<boost::shared_ptr<Event> > event_queue;

/*data queue for writer*/
std::queue<boost::shared_ptr<std::vector<unsigned char> > > write_data_queue;

/*pseudo-processes*/
/*read process*/
enum ReadState {
	reader_nothing, reader_waiting, reader_eof
};
ReadState read_state;
void read_process(void) {
	switch(read_state) {
	case reader_nothing:
		try {
			boost::shared_ptr<std::vector<unsigned char> > dat;
			boost::shared_ptr<Event> ev = input->read(
				input_pi, 80, dat
			);
			if(ev) {
				event_queue.push(ev);
				read_state = reader_waiting;
				return;
			} else if(dat) {
				write_data_queue.push(dat);
				input_process.nil_responded = 0;
				input_process.io_responded = 0;
				read_state = reader_nothing;
				return;
			} else {
				read_state = reader_eof;
				return;
			}
		} catch(IOError io) {
			std::cerr << "An I/O error occured while reading";
			throw io;
		}
		return;
	case reader_waiting:
		if(input_process.nil_responded) {
			read_state = reader_eof;
			return;
		} else if(input_process.io_responded) {
			write_data_queue.push(input_process.dat);
			input_process.dat.reset();
			read_state = reader_nothing;
			return;
		} else return;
	case reader_eof:
		return;
	}
}
/*write process*/
enum WriteState {
	writer_readwaiting, writer_writewaiting
};
WriteState write_state;
void write_process(void) {
	switch(write_state) {
	case writer_readwaiting:
		if(write_data_queue.empty()) {
			if(read_state == reader_eof) {
				stop = 1;
			}
			return;
		} else try {
			boost::shared_ptr<Event> ev = output->write(
				output_pi,
				write_data_queue.front()
			);
			write_data_queue.pop();
			if(ev) {
				event_queue.push(ev);
				output_process.nil_responded = 0;
				output_process.io_responded = 0;
				output_process.dat.reset();
				write_state = writer_writewaiting;
				return;
			} else {
				write_state = writer_readwaiting;
				return;
			}
		} catch(IOError io) {
			std::cerr << "An I/O error occured while writing";
			throw io;
		}
	case writer_writewaiting:
		if(output_process.nil_responded) {
			write_state = writer_readwaiting;
			return;
		} else if(output_process.io_responded) {
			/*re-send*/
			boost::shared_ptr<Event> ev = output->write(
				output_pi,
				output_process.dat
			);
			if(ev) {
				event_queue.push(ev);
				output_process.nil_responded = 0;
				output_process.io_responded = 0;
				output_process.dat.reset();
				write_state = writer_writewaiting;
				return;
			} else {
				write_state = writer_readwaiting;
				return;
			}
		} else return;
	}
}
/*event handler process*/
void event_handler(void) {
	EventSet& es = the_event_set();
	/*check for events to add*/
	while(!event_queue.empty()) {
		es.add_event(event_queue.front());
		event_queue.pop();
	}
	if(!es.empty()) {
		/*processes waiting to do something?*/
		if(!read_state == reader_nothing) {
			es.event_poll(dummy_process);
		} else {
			es.event_wait(dummy_process);
		}
	}
}

/*main loop*/
int main(void) {
	aio_initialize();
	input = ioport_stdin();
	output = ioport_stdout();

	read_state = reader_nothing;

	/*main loop*/
	stop = 0;
	while(!stop) {
		read_process();
		write_process();
		event_handler();
	}

	aio_deinitialize();
}

