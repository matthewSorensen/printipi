#include "scheduler.h"

#include <pthread.h> //for pthread_setschedparam
#include <time.h> //for clock_nanosleep
#include "logging.h"
#include "timeutil.h"


/*Scheduler::Scheduler(const std::function<void(const Event&)>& callback) : consumer(std::thread(&Scheduler::consumerLoop, this, callback)) {}*/

//Scheduler::Scheduler() : consumer(std::thread(&Scheduler::consumerLoop, this)) {}
Scheduler::Scheduler() : _lockPushes(mutex, std::defer_lock), _arePushesLocked(false) {
	clock_gettime(CLOCK_MONOTONIC, &(this->lastEventHandledTime)); //initialize to current time.
}


void Scheduler::queue(const Event& evt) {
	//LOGV("Scheduler::queue\n");
	std::unique_lock<std::mutex> lock(this->mutex);
	if (this->eventQueue.empty()) { //if no event is before this, then the relative time held by this event must be associated with the current time:
		clock_gettime(CLOCK_MONOTONIC, &(this->lastEventHandledTime));
	}
	this->eventQueue.push(evt);
	this->nonemptyCond.notify_one(); //notify the consumer thread that a new event is ready.
}


Event Scheduler::nextEvent() {
	Event evt;
	{
		//std::unique_lock<std::mutex> lock(this->mutex);
		if (!this->_arePushesLocked) { //check if the mutex is already locked *by us*
			_lockPushes.lock();
		}
		while (this->eventQueue.empty()) { //wait for an event to be pushed.
			this->nonemptyCond.wait(_lockPushes); //condition_variable.wait() can produce spurious wakeups; need the while loop.
		}
		evt = this->eventQueue.front();
		this->eventQueue.pop();
		if (this->eventQueue.size() < SCHED_CAPACITY) { //queue is underfilled; release the lock
			_lockPushes.unlock();
			this->_arePushesLocked = false;
		} else { //queue is filled; do not release the lock.
			this->_arePushesLocked = true;
		}
	} //unlock the mutex and then handle the event.
	struct timespec sleepUntil = evt.time();
	struct timespec curTime;
	clock_gettime(CLOCK_MONOTONIC, &curTime);
	//struct timespec sleepUntil = timespecAdd(this->lastEventHandledTime, evt.time());
	LOGV("Scheduler::nextEvent sleep from %lu.%lu until %lu.%lu\n", curTime.tv_sec, curTime.tv_nsec, sleepUntil.tv_sec, sleepUntil.tv_nsec);
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &sleepUntil, NULL); //sleep to event time.
	//clock_gettime(CLOCK_MONOTONIC, &(this->lastEventHandledTime)); //in case we fall behind, preserve the relative time between events.
	this->lastEventHandledTime = sleepUntil;
	return evt;
}

void Scheduler::initSchedThread() {
	struct sched_param sp; 
	sp.sched_priority=SCHED_PRIORITY; 
	if (int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp)) {
		LOGW("Warning: pthread_setschedparam (increase thread priority) at scheduler.cpp returned non-zero: %i\n", ret);
	}
}

struct timespec Scheduler::lastSchedTime() const {
	Event evt;
	{
		std::unique_lock<std::mutex> lock(this->mutex);
		if (this->eventQueue.size()) {
			evt = this->eventQueue.back();
		} else {
			lock.unlock();
			timespec ts;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			return ts;
		}
	}
	return evt.time();
}

