// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"
#include "interrupt.h"
#include "thread.h"
#include "alarm.h"
#include "machine.h"

#define MAX_FILE_NAME  10

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions
//	is in machine.h.
//----------------------------------------------------------------------
//----------------------------------------------------------------------

Alarm *a;
int sharedMemory;
bool share = FALSE;
int shcount = 0;
Semaphore semalist[20];
int eatlist[20];

void Add_Syscall()
{
    int result;
    result = SysAdd(/* int op1 */(int)kernel->machine->ReadRegister(4),(int)kernel->machine->ReadRegister(5));
    DEBUG(dbgSys, "Add returning from " << kernel->currentThread->getName() << kernel->currentThread->pid << "with " << result << "\n");
    //kernel->stats->Print();
    kernel->machine->WriteRegister(2, (int)result);
}

char* setStatus(int x)
{
    if (x == 0) return "JUST_CREATED";
    else if (x == 1) return "RUNNING";
    else if (x == 2 ) return "READY";
    else if (x == 3 ) return "BLOCKED";
    else return "KILLED";
}

void SysStatsCall()
{
    //kernel->scheduler->Print();
    cout<< "\n";
    cout << "NumProcs : " << kernel->mysysinfo->numprocs << "\n";
    for (int i = 0; i < kernel->mysysinfo->numprocs ; i++)
    {
        cout<< kernel->mysysinfo->proc[i]->name<<", PID : "<<*(kernel->mysysinfo->proc[i]->pid)<<", PRIORITY : "<<*(kernel->mysysinfo->proc[i]->priority)<<", STATUS : " << setStatus(*(kernel->mysysinfo->proc[i]->status)) << "\n";
        ASSERT(*(kernel->mysysinfo->proc[i]->status) <= 3)
    }
    cout << "Total Ticks : " << *(kernel->mysysinfo->totalticks) <<"\n";
    cout << "Idle Ticks : " << *(kernel->mysysinfo->idleticks) << "\n" ;
    cout << "System Ticks : " << *(kernel->mysysinfo->systemticks) << "\n";
    cout << "User Ticks : " << *(kernel->mysysinfo->userticks) << "\n";
    kernel->scheduler->Print();
    cout<<"SysStatsCall Complete";
    cout<<"Eatlist : "<<endl;
    for (int i = 0; i < 20; i++)
    {
        cout << eatlist[i] << " ";
    }
    cout << "\n";
}

void StartFork(void *kernelThreadAddress)
{


    DEBUG(dbgSys,"Fork returning with 0");
    kernel->currentThread->RestoreUserState();
    kernel->currentThread->space->RestoreState();
   kernel->machine->WriteRegister(2,0);
    //kernel->currentThread->space->InitRegisters();
    kernel->machine->Run();
}

void StartProcessKernel(void *kernelThreadAddress)
{
    kernel->currentThread->space->InitRegisters();
    kernel->currentThread->space->RestoreState();
    kernel->machine->Run();
}
void readMemory(int FromUserAddress, char *ToKernelAddress) {
	//Machine::ReadMem(int addr, int size, int *value)
	//DEBUG(dbgSys, "CopyStringToKernel : (FromUserAddress 0x%x) (ToKernelAddress 0x%x)\n", FromUserAddress, ToKernelAddress);
	DEBUG(dbgSys, "CopyStringToKernel");
	int c  /*, physAddr*/ ;
	do {
		//currentThread->space->Translate(FromUserAddress++,&physAddr, 1); //translation
		kernel->machine->ReadMem(FromUserAddress++, 1, &c);   //translation inside ReadMem
		DEBUG(dbgSys, "%c\n"<<(char) c);
		*ToKernelAddress++ = (char) c ;
		//DEBUG('x', "ToKernelAddress (%c)\n", *(ToKernelAddress-1));
	} while (*(ToKernelAddress-1) != NULL);
}

void ExceptionHandler(ExceptionType which)
{
    int type = kernel->machine->ReadRegister(2);
    DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");
    if (which==SyscallException)
    {
        switch(type)
        {
            case SC_Fork:
                                DEBUG(dbgSys, "This is a Fork system call by - "<< kernel->currentThread->getName() << kernel->currentThread->pid << "with priority" << kernel->currentThread->priority << "\n");

                Thread * newthread1;
                newthread1=new Thread("child");
                //int index;
                //index = kernel->mysysinfo->numprocs;
                //kernel->mysysinfo->proc[index] = new ProcInfo();
                //kernel->mysysinfo->proc[index]->name = newthread1->getName();
                //kernel->mysysinfo->proc[index]->status = &newthread1->status;
                //kernel->mysysinfo->numprocs += 1;
                newthread1->space= new AddrSpace(*kernel->currentThread->space);
                DEBUG(dbgSys,"Fork returning with "<<newthread1->space->id);
                {
                kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                /* svet next programm counter for brach execution */
                kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
               // kernel->currentThread->space->SaveState();
                }
                kernel->machine->WriteRegister(2,newthread1->space->id);
                newthread1->Fork(StartFork,(void *)0);
                kernel->currentThread->Yield();

//                kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
//                kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                /* svet next programm counter for brach execution */
//                kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);


                return;
                ASSERTNOTREACHED();

                break;


            case SC_Exec:
                DEBUG(dbgSys, "This is an Exec system call by - "<< kernel->currentThread->getName() << "\n");
                char execfile[MAX_FILE_NAME];
                int memaddress,prior;
                memaddress=kernel->machine->ReadRegister(4);
                prior=kernel->machine->ReadRegister(5);
                readMemory(memaddress,execfile);
                //func. defined at top of the File
                Thread * newthread;
                newthread=new Thread("child",prior);
                newthread->space= new AddrSpace();
                newthread->space->Load(execfile);
                //kernel->currentThread->space->Load(execfile);
                //kernel->currentThread->space->InitRegisters();
                //kernel->currentThread->space->RestoreState();
                //kernel->machine->Run();
                DEBUG(dbgSys, "File loaded\n");
                //int index1;
                //index1 = kernel->mysysinfo->numprocs;
                //kernel->mysysinfo->proc[index1] = new ProcInfo();
                //kernel->mysysinfo->proc[index1]->name = newthread->getName();
                //kernel->mysysinfo->proc[index1]->status = &newthread->status;
                //kernel->mysysinfo->numprocs += 1;
                newthread->Fork(StartProcessKernel,(void*)0);
                kernel->currentThread->Yield();
                //kernel->scheduler->Run(kernel->currentThread,false);    // ReadyToRun
                //kernel->machine->WriteRegister(PrevPCReg,kernel->machine->ReadRegister(PCReg));
                //kernel->machine->WriteRegister(PCReg,kernel->machine->ReadRegister(PCReg)+4);
                //kernel->currentThread->SaveUserState();
                //kernel->currentThread->space->SaveState();
                //IntStatus oldLevel;
                //oldLevel = kernel->interrupt->SetLevel(IntOff);
                //newthread->space->InitRegisters();
                //kernel->scheduler->ReadyToRun(newthread);
                //kernel->scheduler->Run(newthread,false);
                //newthread->space->RestoreState();
                //assumes that interrupts
                    // are disabled!
                //   (void) kernel->interrupt->SetLevel(oldLevel);
                //kernel->currentThread->SaveUserState();     // save the user's
                //CPU registers
                    //kernel->currentThread->space->SaveState();
                    //kernel->scheduler->Run(newthread,false);
                    //kernel->currentThread->space->Execute();
                    DEBUG(dbgSys, "Did i reach here ??");
                    /* set previous programm counter (debugging only)*/
                    //kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                    //kernel->machine->WriteRegister(PCReg,0);
                    /* set next programm counter for brach execution */
                    //newthread->space->RestoreState();
                    //kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    /* svet next programm counter for brach execution */
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                    return;
                    ASSERTNOTREACHED();

                break;
            case SC_Exec2:
                DEBUG(dbgSys, "This is an Exec2 system call by - "<< kernel->currentThread->getName() << "\n");
                char execfile3[MAX_FILE_NAME];
                int memaddress3;
                memaddress3=kernel->machine->ReadRegister(4);
                readMemory(memaddress3,execfile3);
                //func. defined at top of the File
                //Thread * newthread;
                //newthread=new Thread("child");
                //newthread->space= new AddrSpace();
                //newthread->space->Load(execfile);
                kernel->currentThread->space->Load(execfile3);
                kernel->currentThread->space->InitRegisters();
                kernel->currentThread->space->RestoreState();
                kernel->machine->Run();
                DEBUG(dbgSys, "File loaded\n");
                //newthread->Fork(StartProcessKernel,(void*)0);
                //kernel->currentThread->Yield();
                //kernel->scheduler->Run(kernel->currentThread,false);    // ReadyToRun
                //kernel->machine->WriteRegister(PrevPCReg,kernel->machine->ReadRegister(PCReg));
                //kernel->machine->WriteRegister(PCReg,kernel->machine->ReadRegister(PCReg)+4);
                //kernel->currentThread->SaveUserState();
                //kernel->currentThread->space->SaveState();
                //IntStatus oldLevel;
                //oldLevel = kernel->interrupt->SetLevel(IntOff);
                //newthread->space->InitRegisters();
                //kernel->scheduler->ReadyToRun(newthread);
                //kernel->scheduler->Run(newthread,false);
                //newthread->space->RestoreState();
                //assumes that interrupts
                    // are disabled!
                //   (void) kernel->interrupt->SetLevel(oldLevel);
                //kernel->currentThread->SaveUserState();     // save the user's
                //CPU registers
                    //kernel->currentThread->space->SaveState();
                    //kernel->scheduler->Run(newthread,false);
                    //kernel->currentThread->space->Execute();
                    DEBUG(dbgSys, "Did i reach here ??");
                    /* set previous programm counter (debugging only)*/
                    //kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                    //kernel->machine->WriteRegister(PCReg,0);
                    /* set next programm counter for brach execution */
                    //newthread->space->RestoreState();
                    //kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    /* svet next programm counter for brach execution */
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                    return;
                    ASSERTNOTREACHED();

                    break;

            case SC_SCreate:
                    int svalue;
                    svalue = (int)kernel->machine->ReadRegister(4);
                    int scount;
                    for (scount = 0; scount < 20; scount++)
                    {
                        if(semalist[scount].valid == 0)
                            break;
                    }
                    if (scount > 19)
                    {
                        DEBUG(dbgSys,"No free semaphores left");
                            SysHalt();
                    }
                    Semaphore *stemp;
                    stemp = new Semaphore("semaphore",svalue);
                    semalist[scount] = *stemp;
                    eatlist[scount] = 0;
                    kernel->machine->WriteRegister(2,(int)scount);

                        
                     kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    /* svet next programm counter for brach execution */
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                    return;
                    ASSERTNOTREACHED();

                    break;
            
            case SC_SWait:
                    int sid;
                    sid = (int)kernel->machine->ReadRegister(4);
                    DEBUG(dbgSys, "Semaphore Wait called");
                    
                    semalist[sid].P();

                     kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    /* svet next programm counter for brach execution */
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                    return;
                    ASSERTNOTREACHED();

                    break;

            case SC_SSignal:
                    sid = (int)kernel->machine->ReadRegister(4);
                    
                    semalist[sid].V();
                    DEBUG(dbgSys, "Semaphore signalled");

                     kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    /* svet next programm counter for brach execution */
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                    return;
                    ASSERTNOTREACHED();

                    break;

            case SC_SDestroy:
                    sid = (int)kernel->machine->ReadRegister(4);
                    
                    semalist[sid].valid = 0;

                    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    /* svet next programm counter for brach execution */
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                    return;
                    ASSERTNOTREACHED();

                    break;


            case SC_SIncrement:
                    sid = (int)kernel->machine->ReadRegister(4);
                    int i,sum;
                    sum = 0;
                    for (i=0;i<20;i++)
                        sum+=eatlist[i];
                    if(sum <= 1000)
                        eatlist[sid]++;


                    kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                    /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                    kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                    /* svet next programm counter for brach execution */
                    kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                    return;
                    ASSERTNOTREACHED();

            case SC_shared_memory_open :
                   DEBUG(dbgSys, "System Call to open shared memory"); 
                   int sizeMem; 
                   sizeMem = kernel->machine->ReadRegister(4);
                   if (sizeMem > PageSize){
                      DEBUG(dbgSys, "Requested memory for sharing is greater than physical page size. Physical page size is " << PageSize << endl);
                   }   
                   else{                                           
	              if (share == FALSE){
                      int m;
                      m = kernel->currentThread->space->numPages;
	                 sharedMemory = kernel->currentThread->space->idsh;
	                 DEBUG(dbgSys, "No shared memory still exists.Creating the shared memory. Returning the id of the shared memory" <<endl<< sharedMemory);
 	                   kernel->machine->WriteRegister(2,sharedMemory*(m-1));
 	                   shcount++;
 	                  // kernel->currentThread->space->share = TRUE;
 	                   share  = TRUE;
 	              }
 	              else{
 	                 DEBUG(dbgSys, "shared memory exists.adding it to the current process with PID "<<kernel->currentThread->pid <<endl);
 	                 int j = kernel->currentThread->space->numPages;
 	                 kernel->currentThread->space->pageTable[j-1].physicalPage = sharedMemory;
 	                 kernel->currentThread->space->pageTable[j-1].valid = TRUE;
	                 kernel->currentThread->space->pageTable[j-1].use = FALSE;
	                 kernel->currentThread->space->pageTable[j-1].dirty = FALSE;
	                 kernel->currentThread->space->pageTable[j-1].readOnly = FALSE;
	                 kernel->currentThread->space->share = TRUE;
	                 DEBUG(dbgSys, "value of shcount " << shcount<<endl);
	                 kernel->machine->WriteRegister(2,sharedMemory*(j));
	                  DEBUG(dbgSys, "Memory ID is "<< sharedMemory<< " and memory length is " <<sizeMem <<endl) ;
 	              }
 	            }
 	            // set previous programm counter (debugging only)
                kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                   //set programm counter to next instruction (all Instructions are 4 byte wide)
                kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                   //set next programm counter for brach execution 
                kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);             
	          return;
                  ASSERTNOTREACHED();
                    break;

                break;
            case SC_shared_memory_close :
              DEBUG(dbgSys,"System Call for closing the existing shared memory");    
                if(share == FALSE){
                DEBUG(dbgSys, "No memory still shared.");
                }
               else{
                  ASSERT(kernel->currentThread->space->share == TRUE);
                  DEBUG(dbgSys, "The Thread named " << kernel->currentThread->getName() <<" PID "<<kernel->currentThread->pid<< " has shared memory.Deleting memory");
                    int j = kernel->currentThread->space->numPages;
                    kernel->currentThread->space->pageTable[j-1].valid = TRUE;
                    kernel->currentThread->space->pageTable[j-1].use = FALSE;
                    kernel->currentThread->space->pageTable[j-1].dirty = TRUE;
                    kernel->currentThread->space->pageTable[j-1].readOnly = FALSE;
                    kernel->currentThread->space->share = FALSE; 
                   DEBUG(dbgSys, "all shared memory closed")
                  share  = FALSE;
                }
                  // set previous programm counter (debugging only)
                kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                 //set programm counter to next instruction (all Instructions are 4 byte wide)
                kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                 //set next programm counter for brach execution 
                kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                
                return;
                ASSERTNOTREACHED();
                break;
 
            case SC_shared_memory_write :
               DEBUG(dbgSys, "Writing to shared memory.");
               int val, valSize,j, mem;
                val  = kernel->machine->ReadRegister(6);
                valSize = kernel->machine->ReadRegister(5);
                mem = kernel->machine->ReadRegister(4);
                j = kernel->currentThread->space->numPages;
                kernel->machine->WriteMem((j-1)*PageSize+mem, valSize , val);
                kernel->machine->WriteRegister(2,mem+valSize);
                DEBUG(dbgSys, "Value "<< val<<" of size "<<valSize<<" Writtten at memory " <<mem <<endl);
               // set previous programm counter (debugging only)
                kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                 //set programm counter to next instruction (all Instructions are 4 byte wide)
                kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                 //set next programm counter for brach execution 
                kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                return;
                ASSERTNOTREACHED();
               break; 
               
            case SC_shared_memory_read :
                  DEBUG(dbgSys, "Reading from shared memory.");
               int valAdd;
                valSize = kernel->machine->ReadRegister(5);
                mem = kernel->machine->ReadRegister(4);
                j = kernel->currentThread->space->numPages;
                kernel->machine->ReadMem((j-1)*PageSize+mem, valSize , &valAdd);
                kernel->machine->WriteRegister(2,valAdd);
                DEBUG(dbgSys, "Value "<< valAdd<<" of size "<<valSize<<" read from the memory " <<mem <<endl);
               // set previous programm counter (debugging only)
                kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                 //set programm counter to next instruction (all Instructions are 4 byte wide)
                kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                 //set next programm counter for brach execution 
                kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
            
                return;
                ASSERTNOTREACHED();
                break;                
             
            case SC_Halt:
                DEBUG(dbgSys, "Shutdown, initiated by user program.\n");
                SysHalt();
                 /* set previous programm counter (debugging only)*/
                kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                /* set next programm counter for brach execution */
                kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                return;
                ASSERTNOTREACHED();
               break;

            case SC_Add:
	            DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");
                kernel->scheduler->Print();
                Add_Syscall();
                  /* set previous programm counter (debugging only)*/
                kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                /* set next programm counter for brach execution */
                kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                return;
                ASSERTNOTREACHED();
               break;
            case SC_SysStats:
	            DEBUG(dbgSys, "SysStatsCall "); 
                SysStatsCall();
                  /* set previous programm counter (debugging only)*/
                kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                /* set next programm counter for brach execution */
                kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);

                kernel->machine->WriteRegister(2,(int)kernel->mysysinfo);
                return;
                ASSERTNOTREACHED();
               break;
            case SC_Sleep:
               DEBUG(dbgSys,"Sleep System Call by thread : "<< kernel->currentThread->name << "and pid : " << kernel->currentThread->pid );
               int memaddress1;
               memaddress1=kernel->machine->ReadRegister(4);
               //a = new Alarm(false);
               DEBUG(dbgSys, "Going to WaitUntil "<< memaddress1 << endl); 
               kernel->alarm->WaitUntil(memaddress1);
               //free(buffer);
//               kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(NextPCReg));
                   /* set previous programm counter (debugging only)*/
                kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                /* set next programm counter for brach execution */
                kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);

              return;

            case SC_Exit:
//               int exitcode;
 //            exitcode = kernel->machine->ReadRegister(4);
               //kernel->currentThread->Yield();
 //              DEBUG(dbgSys, "Calling exit " << exitcode);
//                IntStatus oldLevel2;
//                oldLevel2 = kernel->interrupt->SetLevel(IntOff);
//              kernel->scheduler->toBeDestroyed = kernel->currentThread;
//              (void) kernel->interrupt->SetLevel(oldLevel2);
//               if (!kernel->scheduler->FindNextToRun()) {
//                   DEBUG(dbgSys, "No more threads to run..");
//                    SysHalt();
//               }

               DEBUG(dbgSys,"Current Thread in SC_Exit : "<< kernel->currentThread->name << "and pid : " << kernel->currentThread->pid );

                SysStatsCall();
//             oldLevel2 = kernel->interrupt->SetLevel(IntOff);
//                DEBUG(dbgSys, "Running next thread . ");
                //if (kernel->currentThread->space->id ==0)
                //{
                
                    //if (kernel->scheduler->readyList->IsEmpty()){
                        //SysHalt();
                        //return;
                    //}
                //}
                //else
                {  
             //  Thread *nextthread;
               // DEBUG(dbgSys, "Finding next thread to run . ");
             // nextthread = kernel->scheduler->FindNextToRun();
              //  nextthread->RestoreUserState();
                    kernel->currentThread->Finish();
               
                }
                kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
                /* set programm counter to next instruction (all Instructions are 4 byte wide)*/
                kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
                /* set next programm counter for brach execution */
                kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
                //kernel->currentThread->Finish();
                //kernel->machine->Run();
                 return;
            //    ASSERTNOTREACHED();
//                kernel->scheduler->toBeDestroyed=kernel->currentThread;
//               kernel->scheduler->Run(nextthread,false);
//               (void) kernel->interrupt->SetLevel(oldLevel2);
                break;
            default:
                cerr << "Unexpected system call " << type << "\n";
                break;
        }
            }
    else
    {
        cerr << "Unexpected user mode exception" << which << "\nabc " << (int)which << "\n";
    }
    ASSERTNOTREACHED();
}
