// thread.cc 
//	Routines to manage threads.  These are the main operations:
//
//	Fork -- create a thread to run a procedure concurrently
//		with the caller (this is done in two steps -- first
//		allocate the Thread object, then call Fork on it)
//	Begin -- called when the forked procedure starts up, to turn
//		interrupts on and clean up after last thread
//	Finish -- called when the forked procedure finishes, to clean up
//	Yield -- relinquish control over the CPU to another ready thread
//	Sleep -- relinquish control over the CPU, but thread is now blocked.
//		In other words, it will not run again, until explicitly 
//		put back on the ready queue.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "thread.h"
#include "switch.h"
#include "synch.h"
#include "sysdep.h"
#include "kernel.h"
#include "debug.h"
#include <string>
#include <fstream>
#include <boost/lexical_cast.hpp>

using namespace std;
// this is put at the top of the execution stack, for detecting stack overflows
const int STACK_FENCEPOST = 0xdedbeef;
int pid_counter = 0;
//----------------------------------------------------------------------
// Thread::Thread
// 	Initialize a thread control block, so that we can then call
//	Thread::Fork.
//
//	"threadName" is an arbitrary string, useful for debugging.
//----------------------------------------------------------------------

Thread::Thread(char* threadName)
{
    pid = pid_counter;
    pid_counter++;
    name = threadName;// + boost::lexical_cast<std::string>(2);
    stackTop = NULL;
    stack = NULL;

    status = JUST_CREATED;
    arrival_time = kernel->stats->totalTicks;

    for (int i = 0; i < MachineStateSize; i++) {
	machineState[i] = NULL;		// not strictly necessary, since
					// new thread ignores contents 
					// of machine registers
    }
    space = NULL;
    if (strcmp(name,"main") ==0 || strcmp(name,"postal worker") ==0)
        priority = -1 ;
    else
        priority = rand() % 10;
    response_time = -1;
    //arrival_time = -1;
    burst_time = 0;
    count = 0;
    io_time = 0;
    turnaround_time = -1;
    first_response_flag = false;
    int index;
    index = kernel->mysysinfo->numprocs;
    kernel->mysysinfo->proc[index] = new ProcInfo();
    kernel->mysysinfo->proc[index]->name = this->name;
    kernel->mysysinfo->proc[index]->pid = &this->pid;
    kernel->mysysinfo->proc[index]->priority = &this->priority;
    kernel->mysysinfo->proc[index]->status = &this->status;
    kernel->mysysinfo->numprocs += 1;

    DEBUG(dbgThread, "Thread created : name - " << name << ", status - " << status << ", priority - " <<priority << endl);
}
Thread::Thread(char* threadName,int debugPriority)
{
    pid = pid_counter;
    pid_counter++;
    name = threadName;// + boost::lexical_cast<std::string>(2);
    stackTop = NULL;
    stack = NULL;

    status = JUST_CREATED;
    arrival_time = kernel->stats->totalTicks;

    for (int i = 0; i < MachineStateSize; i++) {
	machineState[i] = NULL;		// not strictly necessary, since
					// new thread ignores contents 
					// of machine registers
    }
    space = NULL;
    if (strcmp(name,"main") ==0 || strcmp(name,"postal worker") ==0)
        priority = -1 ;
    else
        priority = debugPriority;
    response_time = -1;
    //arrival_time = -1;
    burst_time = 0;
    count = 0;
    io_time = 0;
    turnaround_time = -1;
    first_response_flag = false;
    int index;
    index = kernel->mysysinfo->numprocs;
    kernel->mysysinfo->proc[index] = new ProcInfo();
    kernel->mysysinfo->proc[index]->name = this->name;
    kernel->mysysinfo->proc[index]->pid = &this->pid;
    kernel->mysysinfo->proc[index]->priority = &this->priority;
    kernel->mysysinfo->proc[index]->status = &this->status;
    kernel->mysysinfo->numprocs += 1;

    DEBUG(dbgThread, "Thread created : name - " << name << ", status - " << status << ", priority - " <<priority << endl);
}


//----------------------------------------------------------------------
// Thread::~Thread
// 	De-allocate a thread.
//
// 	NOTE: the current thread *cannot* delete itself directly,
//	since it is still running on the stack that we need to delete.
//
//      NOTE: if this is the main thread, we can't delete the stack
//      because we didn't allocate it -- we got it automatically
//      as part of starting up Nachos.
//----------------------------------------------------------------------

Thread::~Thread()
{
    DEBUG(dbgThread, "Deleting thread: " << name);
    //ASSERTNOTREACHED();
    ASSERT(this != kernel->currentThread);
    if (stack != NULL)
	DeallocBoundedArray((char *) stack, StackSize * sizeof(int));
}

//----------------------------------------------------------------------
// Thread::Fork
// 	Invoke (*func)(arg), allowing caller and callee to execute 
//	concurrently.
//
//	NOTE: although our definition allows only a single argument
//	to be passed to the procedure, it is possible to pass multiple
//	arguments by making them fields of a structure, and passing a pointer
//	to the structure as "arg".
//
// 	Implemented as the following steps:
//		1. Allocate a stack
//		2. Initialize the stack so that a call to SWITCH will
//		cause it to run the procedure
//		3. Put the thread on the ready queue
// 	
//	"func" is the procedure to run concurrently.
//	"arg" is a single argument to be passed to the procedure.
//----------------------------------------------------------------------
void Thread::setStatus(ThreadStatus st)
{
    //DEBUG(dbgThread,"Change status from : "<<status<<" to : "<<st<<endl);
    //DEBUG(dbgThread,"TotalTics = "<<kernel->stats->totalTicks);
    if ( st == 1 && !first_response_flag) 
    {
        response_time = kernel->stats->totalTicks - arrival_time;
    }
    if (st == 1) first_response_flag = true;
    if (st== 1) //Any to Running
    {   
        burst_time -= kernel->stats->totalTicks;
    }
    if (status ==  1 )
    {
        burst_time += kernel->stats->totalTicks;
        
    }
    if (st == 2)
    {
        count -= 1;
        waiting_time -= kernel->stats->totalTicks;
    }
    if( status == 2)
    {
        count += 1;
        waiting_time += kernel->stats->totalTicks;
    }

    if (st==3)
    {
        io_time -= kernel->stats->totalTicks;
    }
    if (status == 3)
    {

        io_time += kernel->stats->totalTicks;
    }
    status = st;

    //if (name == "main") 
    //    return ;
    //else {
    //    kernel->mysysinfo->numprocs += 1;
    //
}
void 
Thread::Fork(VoidFunctionPtr func, void *arg)
{
    Interrupt *interrupt = kernel->interrupt;
    Scheduler *scheduler = kernel->scheduler;
    IntStatus oldLevel;
    
    DEBUG(dbgThread, "Forking thread: " << name << " f(a): " << (int) func << " " << arg);
    
    StackAllocate(func, arg);

    oldLevel = interrupt->SetLevel(IntOff);
    this->SaveUserState();
    scheduler->ReadyToRun(this);	// ReadyToRun assumes that interrupts 
					// are disabled!
    (void) interrupt->SetLevel(oldLevel);
}    

//----------------------------------------------------------------------
// Thread::CheckOverflow
// 	Check a thread's stack to see if it has overrun the space
//	that has been allocated for it.  If we had a smarter compiler,
//	we wouldn't need to worry about this, but we don't.
//
// 	NOTE: Nachos will not catch all stack overflow conditions.
//	In other words, your program may still crash because of an overflow.
//
// 	If you get bizarre results (such as seg faults where there is no code)
// 	then you *may* need to increase the stack size.  You can avoid stack
// 	overflows by not putting large data structures on the stack.
// 	Don't do this: void foo() { int bigArray[10000]; ... }
//----------------------------------------------------------------------

void
Thread::CheckOverflow()
{
    if (stack != NULL) {
#ifdef HPUX			// Stacks grow upward on the Snakes
	ASSERT(stack[StackSize - 1] == STACK_FENCEPOST);
#else
	ASSERT(*stack == STACK_FENCEPOST);
#endif
   }
}

//----------------------------------------------------------------------
// Thread::Begin
// 	Called by ThreadRoot when a thread is about to begin
//	executing the forked procedure.
//
// 	It's main responsibilities are:
//	1. deallocate the previously running thread if it finished 
//		(see Thread::Finish())
//	2. enable interrupts (so we can get time-sliced)
//----------------------------------------------------------------------

void
Thread::Begin ()
{
    ASSERT(this == kernel->currentThread);
    DEBUG(dbgThread, "Beginning thread: " << name);
    
    kernel->scheduler->CheckToBeDestroyed();
    kernel->interrupt->Enable();
}

//----------------------------------------------------------------------
// Thread::Finish
// 	Called by ThreadRoot when a thread is done executing the 
//	forked procedure.
//
// 	NOTE: we can't immediately de-allocate the thread data structure 
//	or the execution stack, because we're still running in the thread 
//	and we're still on the stack!  Instead, we tell the scheduler
//	to call the destructor, once it is running in the context of a different thread.
//
// 	NOTE: we disable interrupts, because Sleep() assumes interrupts
//	are disabled.
//----------------------------------------------------------------------

//
void
Thread::Finish ()
{
    (void) kernel->interrupt->SetLevel(IntOff);		
    ASSERT(this == kernel->currentThread);
    
    DEBUG(dbgThread, "Finishing thread: " << name);
    
    Sleep(TRUE);				// invokes SWITCH
    // not reached
}

//----------------------------------------------------------------------
// Thread::Yield
// 	Relinquish the CPU if any other thread is ready to run.
//	If so, put the thread on the end of the ready list, so that
//	it will eventually be re-scheduled.
//
//	NOTE: returns immediately if no other thread on the ready queue.
//	Otherwise returns when the thread eventually works its way
//	to the front of the ready list and gets re-scheduled.
//
//	NOTE: we disable interrupts, so that looking at the thread
//	on the front of the ready list, and switching to it, can be done
//	atomically.  On return, we re-set the interrupt level to its
//	original state, in case we are called with interrupts disabled. 
//
// 	Similar to Thread::Sleep(), but a little different.
//----------------------------------------------------------------------

void
Thread::Yield ()
{
    Thread *nextThread;
    IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
    
    ASSERT(this == kernel->currentThread);
    
    DEBUG(dbgThread, "Yielding thread: " << name);
    if (priority == 9)
    {
        DEBUG(dbgThread, "Priority 9 process ... DONT YIELD");
    }
    else
    {
        nextThread = kernel->scheduler->FindNextToRun();
        if (nextThread != NULL) {
            kernel->scheduler->ReadyToRun(this);
            kernel->scheduler->Run(nextThread, FALSE);
        }
    }
    (void) kernel->interrupt->SetLevel(oldLevel);
}
//----------------------------------------------------------------------
// Thread::Sleep
// 	Relinquish the CPU, because the current thread has either
//	finished or is blocked waiting on a synchronization 
//	variable (Semaphore, Lock, or Condition).  In the latter case,
//	eventually some thread will wake this thread up, and put it
//	back on the ready queue, so that it can be re-scheduled.
//
//	NOTE: if there are no threads on the ready queue, that means
//	we have no thread to run.  "Interrupt::Idle" is called
//	to signify that we should idle the CPU until the next I/O interrupt
//	occurs (the only thing that could cause a thread to become
//	ready to run).
//
//	NOTE: we assume interrupts are already disabled, because it
//	is called from the synchronization routines which must
//	disable interrupts for atomicity.   We need interrupts off 
//	so that there can't be a time slice between pulling the first thread
//	off the ready list, and switching to it.
//----------------------------------------------------------------------
void
Thread::Sleep (bool finishing)
{
    Thread *nextThread;
    
    ASSERT(this == kernel->currentThread);
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    
    DEBUG(dbgThread, "Sleeping thread: " << name);
    
    this->setStatus(BLOCKED);
    if (finishing)
    {  
        //io_time += kernel->stats->totalTicks;
        turnaround_time = kernel->stats->totalTicks - arrival_time;
        //waiting_time = turnaround_time - io_time - burst_time;
        this->PrintStats(); 
    }
    else
    {
        io_time += kernel->stats->totalTicks;
    }
    //status = BLOCKED;
   while ((nextThread = kernel->scheduler->FindNextToRun()) == NULL)
	kernel->interrupt->Idle();	// no one to run, wait for an interrupt
    
    // returns when it's time for us to run
    kernel->scheduler->Run(nextThread, finishing); 
}

//----------------------------------------------------------------------
// ThreadBegin, ThreadFinish,  ThreadPrint
//	Dummy functions because C++ does not (easily) allow pointers to member
//	functions.  So we create a dummy C function
//	(which we can pass a pointer to), that then simply calls the 
//	member function.
//----------------------------------------------------------------------

static void ThreadFinish()    { kernel->currentThread->Finish(); }
static void ThreadBegin() { kernel->currentThread->Begin(); }
void ThreadPrint(Thread *t) { t->Print(); }

#ifdef PARISC

//----------------------------------------------------------------------
// PLabelToAddr
//	On HPUX, function pointers don't always directly point to code,
//	so we need to do the conversion.
//----------------------------------------------------------------------

static void *
PLabelToAddr(void *plabel)
{
    int funcPtr = (int) plabel;

    if (funcPtr & 0x02) {
        // L-Field is set.  This is a PLT pointer
        funcPtr -= 2;	// Get rid of the L bit
        return (*(void **)funcPtr);
    } else {
        // L-field not set.
        return plabel;
    }
}
#endif

//----------------------------------------------------------------------
// Thread::StackAllocate
//	Allocate and initialize an execution stack.  The stack is
//	initialized with an initial stack frame for ThreadRoot, which:
//		enables interrupts
//		calls (*func)(arg)
//		calls Thread::Finish
//
//	"func" is the procedure to be forked
//	"arg" is the parameter to be passed to the procedure
//----------------------------------------------------------------------

void
Thread::StackAllocate (VoidFunctionPtr func, void *arg)
{
    stack = (int *) AllocBoundedArray(StackSize * sizeof(int));

#ifdef PARISC
    // HP stack works from low addresses to high addresses
    // everyone else works the other way: from high addresses to low addresses
    stackTop = stack + 16;	// HP requires 64-byte frame marker
    stack[StackSize - 1] = STACK_FENCEPOST;
#endif

#ifdef SPARC
    stackTop = stack + StackSize - 96; 	// SPARC stack must contains at 
					// least 1 activation record 
					// to start with.
    *stack = STACK_FENCEPOST;
#endif 

#ifdef PowerPC // RS6000
    stackTop = stack + StackSize - 16; 	// RS6000 requires 64-byte frame marker
    *stack = STACK_FENCEPOST;
#endif 

#ifdef DECMIPS
    stackTop = stack + StackSize - 4;	// -4 to be on the safe side!
    *stack = STACK_FENCEPOST;
#endif

#ifdef ALPHA
    stackTop = stack + StackSize - 8;	// -8 to be on the safe side!
    *stack = STACK_FENCEPOST;
#endif


#ifdef x86
    // the x86 passes the return address on the stack.  In order for SWITCH() 
    // to go to ThreadRoot when we switch to this thread, the return addres 
    // used in SWITCH() must be the starting address of ThreadRoot.
    stackTop = stack + StackSize - 4;	// -4 to be on the safe side!
    *(--stackTop) = (int) ThreadRoot;
    *stack = STACK_FENCEPOST;
#endif
    
#ifdef PARISC
    machineState[PCState] = PLabelToAddr(ThreadRoot);
    machineState[StartupPCState] = PLabelToAddr(ThreadBegin);
    machineState[InitialPCState] = PLabelToAddr(func);
    machineState[InitialArgState] = arg;
    machineState[WhenDonePCState] = PLabelToAddr(ThreadFinish);
#else
    machineState[PCState] = (void*)ThreadRoot;
    machineState[StartupPCState] = (void*)ThreadBegin;
    machineState[InitialPCState] = (void*)func;
    machineState[InitialArgState] = (void*)arg;
    machineState[WhenDonePCState] = (void*)ThreadFinish;
#endif
}

#include "machine.h"

//----------------------------------------------------------------------
// Thread::SaveUserState
//	Save the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers -- 
//	one for its state while executing user code, one for its state 
//	while executing kernel code.  This routine saves the former.
//----------------------------------------------------------------------

void
Thread::SaveUserState()
{
    for (int i = 0; i < NumTotalRegs; i++)
	userRegisters[i] = kernel->machine->ReadRegister(i);
}

//----------------------------------------------------------------------
// Thread::RestoreUserState
//	Restore the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers -- 
//	one for its state while executing user code, one for its state 
//	while executing kernel code.  This routine restores the former.
//----------------------------------------------------------------------

void
Thread::RestoreUserState()
{
    for (int i = 0; i < NumTotalRegs; i++)
	kernel->machine->WriteRegister(i, userRegisters[i]);
}


//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

static void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	cout << "*** thread " << which << " looped " << num << " times\n";
        kernel->currentThread->Yield();
    }
}

//----------------------------------------------------------------------
// Thread::SelfTest
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
Thread::SelfTest()
{
    DEBUG(dbgThread, "Entering Thread::SelfTest");

    Thread *t = new Thread("forked thread");

    t->Fork((VoidFunctionPtr) SimpleThread, (void *) 1);
    kernel->currentThread->Yield();
    SimpleThread(0);
}
int
ComparePriority(Thread *x, Thread *y)
{
    if (x->priority < y->priority)
        return 1;
    else if (x->priority  == y->priority)
        return 0;
    else
        return -1;
}
void Thread::PrintStats()
{
    this->WriteStats();
    cout<<"Thread Name : "<<name;
    cout<<"\nPID : "<<this->pid;
    cout<<"\nPriority : "<<priority;
    cout<<"\nBurst Time : "<<burst_time;
//    cout<<"\nIO Time : "<<io_time;
    cout<<"\nCounter : "<<count;
    cout<<"\nArrival Time : "<<arrival_time;
    cout<<"\nResponse Time : "<<response_time;
    cout<<"\nWaiting Time : "<<waiting_time;
    cout<<"\nTurnaround Time : "<<turnaround_time<<endl;
    
}
void Thread::WriteStats()
{
    ofstream myfile;
    myfile.open("stats.txt", ios::app);
    
    myfile<<"Thread Name : "<<name<<endl;
    myfile<<"TotalTicks : "<<kernel->stats->totalTicks;
    myfile<<"\nPID : "<<this->pid;
    myfile<<"\nPriority : "<<priority;
    myfile<<"\nBurst Time : "<<burst_time;
//    myfile<<"\nIO Time : "<<io_time;
    myfile<<"\nArrival Time : "<<arrival_time;
    myfile<<"\nResponse Time : "<<response_time;
    myfile<<"\nWaiting Time : "<<waiting_time;
    myfile<<"\nTurnaround Time : "<<turnaround_time<<endl<<endl;
    myfile.close();
}
