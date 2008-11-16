#include"all_defines.hpp"
#include"thread.hpp"

#include<queue>
#include<iostream>

#include<boost/noncopyable.hpp>

class LockedQueue : boost::noncopyable {
private:
	std::queue<int> Q;
	Mutex m;
	CondVar c;
public:
	void pop(int& x) {
		Lock l(m);
		while(Q.empty()) {
			c.wait(l);
		}
		x = Q.front();
		Q.pop();
	}
	void push(int x) {
		Lock l(m);
		bool sig = Q.empty();
		Q.push(x);
		if(sig) c.broadcast();
	}
};

class ExitCondition : boost::noncopyable {
private:
	size_t num_wait;
	Mutex m;
	CondVar c;

	ExitCondition(); // disallowed!
public:
	void exited(void) {
		Lock l(m);
		--num_wait;
		if(num_wait == 0) c.signal();
	}
	void wait(void) {
		Lock l(m);
		while(num_wait > 0) {
			c.wait(l);
		}
	}
	explicit ExitCondition(size_t nnum_wait)
		: num_wait(nnum_wait), m(), c() { }
};

class Producer {
private:
	ExitCondition* E;
	LockedQueue* Q;
	int s, e;
public:
	Producer(ExitCondition& nE, LockedQueue& nQ, int start = 1, int end = 1)
			: E(&nE), Q(&nQ), s(start), e(end) { }
	void operator()(void) {
		for(int i = s; i <= e; ++i) {
			Q->push(i);
		}
		std::cout << "producer from " << s << " to "
			<< e << " completed." << std::endl;
		std::cout.flush();
		E->exited();
	}
};

class Consumer {
private:
	ExitCondition* E;
	LockedQueue* Q;
	int n;
	char const* nm;
public:
	Consumer(ExitCondition& nE, LockedQueue& nQ,
		int to_consume = 1, char const* name = "a consumer")
			: E(&nE), Q(&nQ), n(to_consume), nm(name) { }
	void operator()(void) {
		int x;
		for(int i = 0; i < n; ++i) {
			Q->pop(x);
			std::cout << nm << " got: " << x << std::endl;
			std::cout.flush();
		}
		E->exited();
	}
};

void test1(void) {
	ExitCondition E(2);
	LockedQueue Q;

	std::cout << "----test1" << std::endl;
	std::cout.flush();

	Thread<Consumer> t2(Consumer(E, Q, 10, "consumer"));
	Thread<Producer> t1(Producer(E, Q, 1, 10));

	E.wait();
}

void test2(void) {
	ExitCondition E(4);
	LockedQueue Q;

	std::cout << "----test2" << std::endl;
	std::cout.flush();

	Thread<Consumer> t1(Consumer(E, Q, 10, "consumer 1"));
	Thread<Consumer> t2(Consumer(E, Q, 10, "consumer 2"));
	Thread<Producer> t3(Producer(E, Q, 11, 20));
	Thread<Producer> t4(Producer(E, Q, 1, 10));

	E.wait();
}



int main(void) {
	test1();
	test2();
	return 0;
}

