#ifndef THREADING_WIN_H_
#define THREADING_WIN_H_

#include <windows.h>

typedef HANDLE thread_identifier;

class Lock;

class Mutex {
	HANDLE mutex;

	friend class Lock;
public:
	Mutex();
	Mutex(const Mutex& other) = delete;
	Mutex(Mutex&& other);
	~Mutex();

	Mutex& operator=(const Mutex& other) = delete;

	Lock lock();
};

class Lock {
	HANDLE mutex;

	friend class Mutex;

	Lock(Mutex& mutex);
public:
	Lock() = delete;
	Lock(const Lock& other) = delete;
	Lock(Lock&& other);
	~Lock();

	Lock& operator=(const Lock& other) = delete;
};

std::vector<CONTEXT> stop_world(std::vector<thread_identifier>& threads);

#endif
