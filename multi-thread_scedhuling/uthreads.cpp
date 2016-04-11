#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>
#include "uthreads.h"
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7
#define MIN_THREAD_NUM 0
#define INIT_QUANTOMS 0
#define FAILURE -1
#define MIN_QUEUE_NUM 0
#define MICRO_IN_SEC 1000000
using namespace std;

address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
		"rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

// global var which count quantoms
int totalQuantoms = INIT_QUANTOMS;
// global var which "remembers" current thread id
int currentThreadNum = MIN_THREAD_NUM;
// global vars which know if a thread terminates or suspends itself
bool toTerminate = false;
bool toSuspend = false;
// globar var which saves the number of quantoms until switching threads
int runningTime;
// an array which indicates if the id is already in use by a thread
bool availableId[MAX_THREAD_NUM] = {false};
sigjmp_buf env[MAX_THREAD_NUM];
sigset_t set;
// an array which counts number of quantoms has passed for each thread
int quantoms[MAX_THREAD_NUM] = {INIT_QUANTOMS};


/** class which represent a thread **/
class Thread 
{
	private:
	//id of thread
	int id;
	
	char stackt[STACK_SIZE];
	address_t sp;
	address_t pc;
	
	public:
	// priority of thread
	Priority pr; // !!!!!!!!!!!!!!!!!!!!
	
	/**
	 * constructor of thread
	 * @param f - function which the thread will point to
	 * @param prio - priority of thread
	 */
	Thread(void (*f)(void), Priority prio)
	{
		// searching for the minimal id can be used
		bool done = false;
		for (int i = MIN_QUEUE_NUM; i < MAX_THREAD_NUM && (!done); i++)
		{
			if (!availableId[i])
			{
				id = i;
				availableId[i] = true;
				done = true;
			}
		}
		pr = prio;
		sp = (address_t)stackt + STACK_SIZE - sizeof(address_t);
		pc = (address_t)f;
		sigsetjmp(env[id], 1);
		(env[id]-> __jmpbuf)[JB_SP] = translate_address(sp);
		(env[id]->__jmpbuf)[JB_PC] = translate_address(pc);
		sigemptyset(&env[id]->__saved_mask);
	}
	
	/**
	 * getter for stack-pointer of thread
	 * @return address of stack
	 */
	address_t getSp()
	{
		return sp;
	}
	
	/**
	 * getter for program-counter of thread
	 * @return address of counter
	 */
	address_t getPc()
	{
		return pc;
	}
	
	/**
	 * getter for id
	 * @return id of thread
	 */
	int getId()
	{
		return id;
	}
};

// three vectors which holds ready threads according to their priority
std::vector<Thread*> readyReds;
std::vector<Thread*> readyOranges;
std::vector<Thread*> readyGreens;
// vector which points to the blocked threads
std::vector<Thread*> blocked;

/** Get the id of the calling thread */
int uthread_get_tid()
{
	return currentThreadNum;
}

/** a functions which deletes the threads **/
void freeAll()
{
	for (unsigned int i = MIN_QUEUE_NUM; i < readyReds.size(); i++)
	{
		delete readyReds[i];
	}
	for (unsigned int i = MIN_QUEUE_NUM; i < readyOranges.size(); i++)
	{
		delete readyOranges[i];
	}
	for (unsigned int i = MIN_QUEUE_NUM; i < readyGreens.size(); i++)
	{
		delete readyGreens[i];
	}
	for (unsigned int i = MIN_QUEUE_NUM; i < blocked.size(); i++)
	{
		delete blocked[i];
	}
}

/** a function which switch to the next thread considering the priorities
 * of the remaining threads
 * @param sig. set by default to 0
 */
void switchThreads(int sig = 0)
{
	totalQuantoms ++;
	if (sigprocmask(SIG_BLOCK, &set, NULL) == FAILURE)
	{
		freeAll();
		exit(0);
	}
		
	// the current running thread
	static Thread* running = readyOranges[MIN_QUEUE_NUM];
	// if its the first time of switching
	static bool first = true;
	
	// if its first so the thread should be deleted from the orange vector
	if (first)
	{
		readyOranges.erase(readyOranges.begin());
		first = false;
	}
	// remember current situation of old thread
	int ret_val = sigsetjmp(env[uthread_get_tid()],1);
	 
	if (ret_val == 1) 
	{
		return;
	}
	// another pointer to the current running thread
	Thread* temp = running;
	// an indicator which tell if a thread should be switched
	bool toChange = true;
	// looking for the thread with the highest priority to switch to
	if (!readyReds.empty())
	{
		running = readyReds[MIN_QUEUE_NUM];
		readyReds.erase(readyReds.begin());
		currentThreadNum = running -> getId();
	}
	
	else if (!readyOranges.empty())
	{
		running = readyOranges[MIN_QUEUE_NUM];
		readyOranges.erase(readyOranges.begin());
		currentThreadNum = running -> getId();
	}
	else if (!readyGreens.empty())
	{
		running = readyGreens[MIN_QUEUE_NUM];
		readyGreens.erase(readyGreens.begin());
		currentThreadNum = running -> getId();
	}
	else
	{
		// if all vectors are empty, no switch should be done
		toChange = false;
	}
	// inserting the old thread to the appropriate vector
	if (toChange && !toTerminate && !toSuspend)
	{
		switch (temp -> pr)
		{
			case RED:
				readyReds.push_back(temp);
				break;
			case ORANGE:
				readyOranges.push_back(temp);
				break;
			case GREEN:
				readyGreens.push_back(temp);
				break;
			default:
				break;
		}
	}
	else if (toSuspend)
	{
		blocked.push_back(temp);
		toSuspend = false;
	}
	// if terminated, should be deleted
	else if (toTerminate)
	{
		delete temp;
		toTerminate = false;
	}
	// incrementing the quantoms for the thread
	quantoms[running -> getId()] ++;
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
	{
		freeAll();
		exit(0);
	}
	struct itimerval tv;
	tv.it_value.tv_sec = 0;  /* first time interval, seconds part */
	tv.it_value.tv_usec = runningTime; /* first time interval, microseconds part */
	tv.it_interval.tv_sec = 0;  /* following time intervals, seconds part */
	tv.it_interval.tv_usec = runningTime; /* following time intervals, microseconds part */
	setitimer(ITIMER_VIRTUAL, &tv, NULL);
	siglongjmp(env[uthread_get_tid()],1);
}



/** Create a new thread whose entry point is f
 * param - *f - a pointer to a function f
 * 		   pr - priority : RED, ORANGE or GREEN **/
int uthread_spawn(void (*f)(void), Priority pr)
{
	// check for not passing legal threads num
	if (blocked.size() + readyReds.size() + readyOranges.size() + readyGreens.size() > \
	MAX_THREAD_NUM - 1)
	{
		std :: cerr << "Too many threads\n";
		return FAILURE;
	}
	if (sigprocmask(SIG_BLOCK, &set, NULL) == FAILURE)
	{
		freeAll();
		exit(0);
	}
	Thread* thr = new Thread(f , pr);
	// pushing the thread to it's matching queue depending on the given priority
	switch (thr -> pr)
	{
		case RED:
			readyReds.push_back(thr);
			break;
		case ORANGE:
			readyOranges.push_back(thr);
			break;
		case GREEN:
			readyGreens.push_back(thr);
			break;
		default:
			break;
	}
	if (sigprocmask(SIG_BLOCK, &set, NULL) == FAILURE)
	{
		freeAll();
		exit(0);
	}
	return thr -> getId();
}

/** Initialize the thread library
 * @param : quantum_usecs - quantoms in usecs  **/
int uthread_init(int quantum_usecs)
{
	static int alreadyInit = false;
	// negative quantoms was given
	if (quantum_usecs <= INIT_QUANTOMS)
	{
		std :: cerr << "Quantom should be positive\n";
		return FAILURE;
	}
	// checks the case of multiple initilization
	else if (alreadyInit)
	{
		std :: cerr << "Already initialized\n";
		return FAILURE;
	}
	alreadyInit = true;
	totalQuantoms ++;
	quantoms[MIN_THREAD_NUM] ++ ;
	runningTime = quantum_usecs;
	Thread* thr = new Thread(nullptr, ORANGE);
	readyOranges.push_back(thr);
	signal(SIGVTALRM, switchThreads);
	sigemptyset(&set);
	struct itimerval tv;
	tv.it_value.tv_sec = quantum_usecs/MICRO_IN_SEC;  /* first time interval, seconds part */
	tv.it_value.tv_usec = quantum_usecs % MICRO_IN_SEC; /* first time interval, microseconds part */
	tv.it_interval = tv.it_value;
	setitimer(ITIMER_VIRTUAL, &tv, NULL);
	return EXIT_SUCCESS;
}

/** Terminate a thread
 * @param : tid - thread id **/
int uthread_terminate(int tid)
{
	// block the signal
	if (sigprocmask(SIG_BLOCK, &set, NULL) == FAILURE)
	{
		freeAll();
		exit(0);
	}
	// checks for cases which the given tid is illegal or not available
	if (tid >= MAX_THREAD_NUM || tid < MIN_THREAD_NUM || !availableId[tid])
	{
		std :: cerr << "thread library error : no such id\n";
		if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
		{
			freeAll();
			exit(0);
		}
		return FAILURE;
	}
	// case of trying to terminate the main thread
	if (tid == MIN_THREAD_NUM)
	{
		//std :: cerr << "system error : cannot terminate main thread\n";
		freeAll();
		exit(EXIT_SUCCESS);
	}
	// case of thread terminating itself
	if (tid == uthread_get_tid())
	{
		toTerminate = true;
		availableId[tid] = false;
		quantoms[tid] = INIT_QUANTOMS;
		switchThreads();
		return EXIT_SUCCESS;
	}
	// terminating a thread which is in one of the ready queues or the in the blocked queue
	for (unsigned int i = MIN_QUEUE_NUM; i < readyReds.size(); i++)
	{
		if (readyReds[i] -> getId() == tid)
		{
			// creating a temp pointer so we can delete the thread later from memory after erasing
			// it from the vector
			Thread* temp = readyReds[i];
			readyReds.erase(readyReds.begin() + i);
			delete temp;
			availableId[tid] = false;
			quantoms[tid] = INIT_QUANTOMS;
			if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
			{
				freeAll();
				exit(0);
			}
			return EXIT_SUCCESS;
		}
	}
	for (unsigned int i = MIN_QUEUE_NUM; i < readyOranges.size(); i++)
	{
		if (readyOranges[i] -> getId() == tid)
		{
			Thread* temp = readyOranges[i];
			readyOranges.erase(readyOranges.begin() + i);
			delete temp;
			availableId[tid] = false;
			quantoms[tid] = INIT_QUANTOMS;
			if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
			{
				freeAll();
				exit(0);
			}
			return EXIT_SUCCESS;
		}
	}
	for (unsigned int i = MIN_QUEUE_NUM; i < readyGreens.size(); i++)
	{
		if (readyGreens[i] -> getId() == tid)
		{
			Thread* temp = readyGreens[i];
			readyGreens.erase(readyGreens.begin() + i);
			delete temp;
			availableId[tid] = false;
			quantoms[tid] = INIT_QUANTOMS;
			if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
			{
				freeAll();
				exit(0);
			}
			return EXIT_SUCCESS;
		}
	}
	for (unsigned int i = MIN_QUEUE_NUM; i < blocked.size(); i++)
	{
		if (blocked[i] -> getId() == tid)
		{
			Thread* temp = blocked[i];
			blocked.erase(blocked.begin() + i);
			delete temp;
			availableId[tid] = false;
			quantoms[tid] = INIT_QUANTOMS;
			if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
			{
				freeAll();
				exit(0);
			}
			return EXIT_SUCCESS;
		}
	}
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
	{
		freeAll();
		exit(0);
	}
	std :: cerr << "thread library error : no such id\n";
	return FAILURE;
}

/** Suspend a thread
 * @param : tid - thread id **/
int uthread_suspend(int tid)
{
	// block the signal
	if (sigprocmask(SIG_BLOCK, &set, NULL) == FAILURE)
	{
		freeAll();
		exit(0);
	}
	// checks for cases which the given tid is illegal or not available
	if (tid >= MAX_THREAD_NUM || tid < MIN_THREAD_NUM || !availableId[tid])
	{
		std :: cerr << "thread library error : no such id\n";
		sigprocmask(SIG_UNBLOCK, &set, NULL);
		return FAILURE;
	}
	// case of trying to suspending the main thread
	if (tid == MIN_THREAD_NUM)
	{
		//std :: cerr << "thread library error : cannot block main thread\n";
		sigprocmask(SIG_UNBLOCK, &set, NULL);
		return FAILURE;
	}
	// case of thread suspending itself
	if (tid == uthread_get_tid())
	{
		toSuspend = true;
		sigprocmask(SIG_UNBLOCK, &set, NULL);
		switchThreads();
	}
	// if the thread is in one of the priorities queues 
	for (unsigned int i = MIN_QUEUE_NUM; i < readyReds.size(); i++)
	{
		if (readyReds[i] -> getId() == tid)
		{
			// pushing the thread to blocked queue and removing it from its ready queue
			blocked.push_back(readyReds[i]);
			readyReds.erase(readyReds.begin() + i);
			if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
			{
				freeAll();
				exit(0);
			}
			return EXIT_SUCCESS;
		}
	}
	
	for (unsigned int i = MIN_QUEUE_NUM; i < readyOranges.size(); i++)
	{
		if (readyOranges[i] -> getId() == tid)
		{
			blocked.push_back(readyOranges[i]);
			readyOranges.erase(readyOranges.begin() + i);
			if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
			{
				freeAll();
				exit(0);
			}
			return EXIT_SUCCESS;
		}
	}
	
	for (unsigned int i = MIN_QUEUE_NUM; i < readyGreens.size(); i++)
	{
		if (readyGreens[i] -> getId() == tid)
		{
			blocked.push_back(readyGreens[i]);
			readyGreens.erase(readyGreens.begin() + i);
			if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
			{
				freeAll();
				exit(0);
			}
			return EXIT_SUCCESS;
		}
	}
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
	{
		freeAll();
		exit(0);
	}
	return EXIT_SUCCESS;
	
}

/** Resume a thread
 * @param : tid - thread id **/
int uthread_resume(int tid)
{
	// block the signal
	if (sigprocmask(SIG_BLOCK, &set, NULL) == FAILURE)
	{
		freeAll();
		exit(0);
	}
	// checks for cases which the given tid is illegal or not available
	if (tid >= MAX_THREAD_NUM || tid < MIN_THREAD_NUM || !availableId[tid])
	{
		std :: cerr << "thread library error : no such id\n";
		if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
		{
			freeAll();
			exit(0);
		}
		return FAILURE;
	}
	// looking for the requested tid in the blocked queue and pushing it to it's matching ready 
	// queue depending on its priority
	for (unsigned int i = MIN_QUEUE_NUM; i < blocked.size(); i++)
	{
		if (blocked[i] -> getId() == tid)
		{
			switch (blocked[i] -> pr)
			{
				case RED:
					readyReds.push_back(blocked[i]);
					break;
				case ORANGE:
					readyOranges.push_back(blocked[i]);
					break;
				case GREEN:
					readyGreens.push_back(blocked[i]);
					break;
				default:
					break;
			}
			blocked.erase(blocked.begin() + i);
			if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
			{
				freeAll();
				exit(0);
			}
			return EXIT_SUCCESS;
		}
	}
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) == FAILURE)
	{
		freeAll();
		exit(0);
	}
	return EXIT_SUCCESS;
}

/** Get the total number of library quantums **/
int uthread_get_total_quantums()
{
	return totalQuantoms;
}

/** Get the number of thread quantums 
 * @param : tid - thread id **/
int uthread_get_quantums(int tid)
{
	// illegal tid number was given or thread is not available
	if (tid < MIN_THREAD_NUM || tid >= MAX_THREAD_NUM || !availableId[tid])
	{
		std :: cerr << "thread library error : no such id\n";
		return FAILURE;
	}
	return quantoms[tid];
}
