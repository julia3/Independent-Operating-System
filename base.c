//Jiahua Liu

/************************************************************************

        This code forms the base of the operating system you will
        build.  It has only the barest rudiments of what you will
        eventually construct; yet it contains the interfaces that
        allow test.c and z502.c to be successfully built together.

        Revision History:
        1.0 August 1990
        1.1 December 1990: Portability attempted.
        1.3 July     1992: More Portability enhancements.
                           Add call to sample_code.
        1.4 December 1992: Limit (temporarily) printout in
                           interrupt handler.  More portability.
        2.0 January  2000: A number of small changes.
        2.1 May      2001: Bug fixes and clear STAT_VECTOR
        2.2 July     2002: Make code appropriate for undergrads.
                           Default program start is in test0.
        3.0 August   2004: Modified to support memory mapped IO
        3.1 August   2004: hardware interrupt runs on separate thread
        3.11 August  2004: Support for OS level locking
	4.0  July    2013: Major portions rewritten to support multiple threads
************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include			 "z502.h"
#include			 "stdio.h"
#include			 "stdlib.h"
#include			 "student.h"
#include			 "malloc.h"

#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE

Queue *ready_queue;
Queue *timer_queue;
Queue *PCB_queue;
Queue *suspend_queue;
PCB currentPCB;
long global_pid;



INT32	create_process(char process_name[], void *starting_address, INT32 initial_priority);
void state_printer_sleep(Queue *queuetimer, Queue *queueready, Queue *queuesuspend);



// These loacations are global and define information about the page table
extern UINT16        *Z502_PAGE_TBL_ADDR;
extern INT16         Z502_PAGE_TBL_LENGTH;

extern void          *TO_VECTOR [];

char                 *call_names[] = { "mem_read ", "mem_write",
                            "read_mod ", "get_time ", "sleep    ",
                            "get_pid  ", "create   ", "term_proc",
                            "suspend  ", "resume   ", "ch_prior ",
                            "send     ", "receive  ", "disk_read",
                            "disk_wrt ", "def_sh_ar" };







/************************************************************************
    INTERRUPT_HANDLER
        When the Z502 gets a hardware interrupt, it transfers control to
        this routine in the OS.
************************************************************************/

void    interrupt_handler( void ) {
    INT32              device_id;
    INT32              status;
    INT32              Index = 0;
	INT32			   Time;
	INT32			   SleepTime;

	INT32			   LockResult;

	PNode pnode = timer_queue->front;  
	PNode pnodeleft;
	PNode p_remove;

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );
	
	//  Your code will go here.  It should include a switch statement for 
    //  each device ¨C you have only the timer right now.
    //  Check that the timer reported a success code.
    //  Call a routine you write that does the work described later.
	  
	//printf("interrupt occur..................................\n");
	MEM_READ(Z502ClockStatus, &Time);
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	while(timer_queue->front!=NULL) {  
		pnode = timer_queue->front;
		if (Time > pnode->PCB0->duetime){
			EnQueuePriority(ready_queue,pnode->PCB0);
			
			DeQueue(timer_queue, &timer_queue->front->PCB0);

					
			
		}
		else{
			break;
		}
	}  
	READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	if (timer_queue->front != NULL){// if timer_queue is not NULL
		pnodeleft = timer_queue->front;
		MEM_READ(Z502ClockStatus, &Time);
		SleepTime = (pnodeleft->PCB0->duetime) - Time;
		if (SleepTime > 0)
		{
			MEM_WRITE(Z502TimerStart, &SleepTime);
		}
		else
		{
			SleepTime = 10;
			MEM_WRITE(Z502TimerStart, &SleepTime);
		}
	}
	


	// Clear out this device - we're done with it
    MEM_WRITE(Z502InterruptClear, &Index );
	                                   
}


/************************************************************************
    FAULT_HANDLER
        The beginning of the OS502.  Used to receive hardware faults.
************************************************************************/

void    fault_handler( void )
    {
    INT32       device_id;
    INT32       status;
    INT32       Index = 0;

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );

    printf( "Fault_handler: Found vector type %d with value %d\n",
                        device_id, status );
	switch (device_id){
		case PRIVILEGED_INSTRUCTION:
			CALL(Z502Halt());
			break;
	}

    // Clear out this device - we're done with it
    MEM_WRITE(Z502InterruptClear, &Index );
}                                       /* End of fault_handler */


/* run the first process on ready_queue*/
void dispatch (){
	PNode pnode;

	while (ready_queue->front == NULL)
	{
		CALL();
		CALL();
		CALL();
		CALL();
		//printf("waiting.........\n");
	}
	
	pnode = (PNode)malloc(sizeof(PNode));
	pnode = ready_queue->front;
	currentPCB = pnode->PCB0;
		
	Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(ready_queue->front->PCB0->pro_context));
}


/************************************************************************
    SVC
        The beginning of the OS502.  Used to receive software interrupts.
        All system calls come to this point in the code and are to be
        handled by the student written code here.
        The variable do_print is designed to print out the data for the
        incoming calls, but does so only for the first ten calls.  This
        allows the user to see what's happening, but doesn't overwhelm
        with the amount of data.
************************************************************************/
void    svc (SYSTEM_CALL_DATA *SystemCallData ) {
    short               call_type;
    static INT16        do_print = 10;
    INT32               Time;
	long				SleepTime;
	short				i;
	
	
	INT32 pid;
	char name[20];
	void *starting_address;
	INT32 priority;
	char temp[20];
	INT32 temppid;
	PNode pnode;
	PNode pnodepriority;
	PCB PCB1;
	PCB PCBpriority;
	PCB PCBsleep;
	PNode pnodesleepready;
	INT32 LockResult;
	
	PCB1 = (PCB) malloc(sizeof (PCBentiy));
	pnode = PCB_queue->front;  
	i = PCB_queue->size;  
	
	call_type = (short)SystemCallData->SystemCallNumber;
	if ( do_print > 0 ) {
        printf( "SVC handler: %s\n", call_names[call_type]);
        for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++ ){
        	 printf( "Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
             (unsigned long )SystemCallData->Argument[i],
             (unsigned long )SystemCallData->Argument[i]);
        }
    do_print--;
	}
    switch (call_type) {
        // Get time service call
        case SYSNUM_GET_TIME_OF_DAY:   // This value is found in syscalls.h
            MEM_READ( Z502ClockStatus, &Time );
            *(long *)SystemCallData->Argument[0] = Time;
            break;
		// sleep a certain amount of time 
		case SYSNUM_SLEEP:
			SleepTime=(long )SystemCallData->Argument[0];
			MEM_READ(Z502ClockStatus, &Time);
			
			PCBsleep = (PCB)malloc(sizeof(PCBentiy));
			strcpy (PCBsleep->process_name, currentPCB->process_name);
			PCBsleep->pid = currentPCB->pid;
			PCBsleep->starting_address = currentPCB->starting_address;
			PCBsleep->initial_priority = currentPCB->initial_priority;
			PCBsleep->error = currentPCB->error;
			PCBsleep->pro_context = currentPCB->pro_context;
			PCBsleep->duetime = Time + SleepTime;
			
			EnQueueTimer(timer_queue, PCBsleep);
			DeQueue(ready_queue, &ready_queue->front->PCB0);
			READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_LOCK+10, SUSPEND_UNTIL_LOCKED, &LockResult);
			state_printer_sleep(timer_queue, ready_queue, suspend_queue);
			READ_MODIFY(MEMORY_INTERLOCK_BASE, DO_UNLOCK+10, SUSPEND_UNTIL_LOCKED, &LockResult);
			MEM_WRITE(Z502TimerStart, &SleepTime);
			dispatch(); 
			
			break;
		//create process and check if they're legal
		case SYSNUM_CREATE_PROCESS:
			strcpy(name, (char *)SystemCallData->Argument[0]);  
			starting_address=(void *)SystemCallData->Argument[1];
			priority=(INT32)SystemCallData->Argument[2];
			pid = create_process(name, starting_address, priority);
			//printf("~~~~~~~~~~~~~~~~~~~~ %d~~~~~~~~~~~~~~~~~~\n",pid);
			if (pid == -1){
				*(INT32 *)SystemCallData->Argument[4]=ERR_BAD_PARAM;
			}
			else {
				*SystemCallData->Argument[3]=pid;// pid of the process just created(end of queue)
				
				*(INT32 *)SystemCallData->Argument[4]=ERR_SUCCESS;
			}
			break;
		// get process ID
		case SYSNUM_GET_PROCESS_ID:
			strcpy(temp, (char *)SystemCallData->Argument[0]);
			if (strcmp (temp, "")==0){
				*(INT32 *)SystemCallData->Argument[1] = currentPCB->pid;
				*(INT32 *)SystemCallData->Argument[2] = ERR_SUCCESS;
				break;
			}
			temppid = find_name((char *)SystemCallData->Argument[0]);
			if (temppid == -1){
				*(INT32 *)SystemCallData->Argument[2] = ERR_BAD_PARAM;
			}
			else {
				*(INT32 *)SystemCallData->Argument[1] = temppid;	
				*(INT32 *)SystemCallData->Argument[2] = ERR_SUCCESS;
			}
			break;
		// terminate system call
        case SYSNUM_TERMINATE_PROCESS:
			temppid = (INT32)SystemCallData->Argument[0];
			if (temppid == -2) {
				CALL(Z502Halt());
			}
			if (temppid == -1){// terminate the process itself
				*(INT32 *)SystemCallData->Argument[1]=ERR_SUCCESS;
				Remove(PCB_queue, currentPCB->pid);
				Remove(ready_queue, currentPCB->pid);
				if (ready_queue->front == NULL && timer_queue->front == NULL){
					CALL(Z502Halt());
				}
				else{
					dispatch();
				}
			}
			else {
				//remove a PCB
				if (Remove(PCB_queue,temppid) == -1){
					*(INT32 *)SystemCallData->Argument[1]=ERR_BAD_PARAM;
				}
				else{
					Remove(PCB_queue,temppid);
					Remove(ready_queue,temppid);
					*(INT32 *)SystemCallData->Argument[1]=ERR_SUCCESS;
				}
			}
			break;
		
		// suspend a certain process, return error if it's illegal
		case SYSNUM_SUSPEND_PROCESS:
			// ourselves
			if ((INT32)SystemCallData->Argument[0] == -1){
				*(INT32 *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			// in timer_queue
			if ((find_pid(timer_queue, (INT32)SystemCallData->Argument[0])) != -1){
				*(INT32 *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			// already in suspend_queue
			if ((find_pid(suspend_queue, (INT32)SystemCallData->Argument[0])) != -1){
				*(INT32 *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			// legal suspend, in ready_queue
			if ((find_pid(ready_queue, (INT32)SystemCallData->Argument[0]) != -1)){
				pnode=RemoveAdvanced(ready_queue,(INT32)SystemCallData->Argument[0]);
				EnQueue(suspend_queue, pnode->PCB0);
				free(pnode);
				*(INT32 *)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			else{
				*(INT32 *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			break;
		
		// resume the process after it has been suspended
		case SYSNUM_RESUME_PROCESS:
			// in suspend_queue
			if ((find_pid(suspend_queue, (INT32)SystemCallData->Argument[0])) != -1){
				pnode=RemoveAdvanced(suspend_queue,(INT32)SystemCallData->Argument[0]);
				EnQueue(ready_queue, pnode->PCB0);
				free(pnode);
				*(INT32 *)SystemCallData->Argument[1] = ERR_SUCCESS;
			}
			// illegal resume
			else{
				*(INT32 *)SystemCallData->Argument[1] = ERR_BAD_PARAM;
			}
			break;
		
		case SYSNUM_CHANGE_PRIORITY:
			if ((find_pid(PCB_queue, (INT32)SystemCallData->Argument[0]) == -1)){
				*(INT32 *)SystemCallData->Argument[2] = ERR_BAD_PARAM;
			}
			else if ((INT32)SystemCallData->Argument[1] < 0 || (INT32)SystemCallData->Argument[1] > 500){
				*(INT32 *)SystemCallData->Argument[2] = ERR_BAD_PARAM;
			}
			else{
				RemoveAdvanced(PCB_queue, (INT32)SystemCallData->Argument[0]);
				pnodepriority = RemoveAdvanced(ready_queue, (INT32)SystemCallData->Argument[0]);
				pnodepriority->PCB0->initial_priority = (INT32)SystemCallData->Argument[1];
				EnQueuePriority(PCB_queue, pnodepriority->PCB0);
				EnQueuePriority(ready_queue, pnodepriority->PCB0);
				free(pnodepriority);
				*(INT32 *)SystemCallData->Argument[2] = ERR_SUCCESS;
			}
			break;
			//default
			default:  
            printf( "ERROR!  call_type not recognized!\n" ); 
            printf( "Call_type is - %i\n", call_type);
    }                                           // End of switch
}                                               // End of svc 



void state_printer_sleep(Queue *queuetimer, Queue *queueready, Queue *queuesuspend){// sleep
	int j;


	//CALL(SP_setup( SP_TIME_MODE, 99999 ));
	CALL(SP_setup_action( SP_ACTION_MODE, "SLEEP" ));
	CALL(SP_setup( SP_TARGET_MODE, currentPCB->pid ));// current id?
	CALL(SP_setup( SP_RUNNING_MODE, currentPCB->pid ));

	for (j = 0; j < queuetimer->size ; j++)
		CALL(SP_setup( SP_WAITING_MODE, j ));
	for (j = 0; j < queueready->size ; j++)
		CALL(SP_setup( SP_READY_MODE, j ));
	for (j = 0; j <queuesuspend->size ; j++)
		CALL(SP_setup( SP_SUSPENDED_MODE, j ));

	CALL(SP_print_header());
	CALL(SP_print_line());

}




/*find whether there is a PCB with name process_name in PCB_queue. 
if yes, return pid of this PCB; if not, return -1*/
int find_name(char *process_name){
	int i;
	
	PNode pnode = PCB_queue->front;  
	i = PCB_queue->size;  

    while(i--) {  
		if (strcmp(pnode->PCB0->process_name, process_name) == 0){
			return pnode->PCB0->pid;
		}
		pnode = pnode->next;  
    }  
	return -1;
}

/*find whether there is a PCB with process_id pid in a specific queue. 
if yes, return pid of this PCB; if not, return -1*/
int find_pid(Queue *pqueue, INT32 pid){
	int i;

	PNode pnode = pqueue->front;  
	i = pqueue->size;  

	if (pnode){
		
		while(i--) {  
		if (pnode->PCB0->pid == pid){
			return pid;
		}
		pnode = pnode->next;  
		}  
	}
	return -1;
}



void	OSCreateProcess(void *starting_address){

	void *next_context=NULL;
	PCB PCB1=(PCB)calloc(1,sizeof(PCBentiy));

	PCB1->starting_address = starting_address;
	PCB1->pid = global_pid;
	strcpy(PCB1->process_name, "MAINPROCESS");
	PCB1->initial_priority = 0;

	Z502MakeContext( &next_context, starting_address, USER_MODE );
	PCB1->pro_context=(void *)next_context;
	currentPCB = PCB1;
	EnQueue(PCB_queue, PCB1);
	EnQueue(ready_queue, PCB1);
	
	Z502SwitchContext( SWITCH_CONTEXT_SAVE_MODE, &PCB1->pro_context );
}



INT32	create_process(char process_name[], void *starting_address, INT32 initial_priority){
	PCB PCB2;
	PNode pnode;
	int i;
	void *next_context=NULL;

	
	// check if Cntr of # of processes created has exceed the MAX_PROCESS_NUMBER
	if(PCB_queue->size >= MAX_PROCESS_NUMBER){
		return -1;
	}
	//check if use of illegal priorities
	if (initial_priority < 0){
		return -1;
	}

	PCB2 = (PCB) malloc(sizeof (PCBentiy));
	strcpy(PCB2->process_name, process_name);
	
	
	PCB2->starting_address = starting_address;
	PCB2->initial_priority = initial_priority;
	PCB2->duetime = 0;
	pnode = PCB_queue->front;  
	i = PCB_queue->size;  
	


	Z502MakeContext( &next_context, starting_address, USER_MODE );//next_context is a struct(Z502context)
	PCB2->pro_context = next_context;
	// check if use of a process name of an already existing process
	while(i--)  
    {  	
		if (strcmp(pnode->PCB0->process_name,process_name) == 0){
			free(PCB2);
			return -1;}
		pnode = pnode->next;  	
   }  
			
	global_pid++;// create pid
	PCB2->pid = global_pid;
	//success
	EnQueue(PCB_queue,PCB2);
	
	/************************************************************************/
	//EnQueue(ready_queue, PCB2);
	EnQueuePriority(ready_queue,PCB2);
	/************************************************************************/

	// currentPCB = PCB2;
	
	return global_pid;
	
}



/************************************************************************
    osInit
        This is the first routine called after the simulation begins.  This
        is equivalent to boot code.  All the initial OS components can be
        defined and initialized here.
************************************************************************/

void    osInit( int argc, char *argv[]  ) {
    void                *next_context;
    INT32               i;

	char testname[20];
    /* Demonstrates how calling arguments are passed thru to here       */

    printf( "Program called with %d arguments:", argc );
    for ( i = 0; i < argc; i++ )
        printf( " %s", argv[i] );
    printf( "\n" );
    printf( "Calling with argument 'sample' executes the sample program.\n" );

    /*          Setup so handlers will come to code in base.c           */

    TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR]   = (void *)interrupt_handler;
    TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR] = (void *)fault_handler;
    TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR]  = (void *)svc;

    /*  Determine if the switch was set, and if so go to demo routine.  */

    if (( argc > 1 ) && ( strcmp( argv[1], "sample" ) == 0 ) ) {
        Z502MakeContext( &next_context, (void *)sample_code, KERNEL_MODE );
        Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &next_context );
    }                   /* This routine should never return!!           */

    /*  This should be done by a "os_make_process" routine, so that
        test0 runs on a process recognized by the operating system.    */
	
	 ready_queue = InitQueue(); 
	 timer_queue = InitQueue(); 
	 PCB_queue = InitQueue();
	 suspend_queue = InitQueue();
	 global_pid = 0;
   
	printf("INPUT THE TEST YOU WANT TO RUN: (e.g. test1a)\n");
	scanf("%s", testname);
	if (strcmp(testname, "test1a") == 0){
		OSCreateProcess(*test1a);    
	}
	else if (strcmp(testname, "test1b") == 0){
		OSCreateProcess(*test1b);    
	}
	else if (strcmp(testname, "test1c") == 0){
		OSCreateProcess(*test1c);    
	}
	else if (strcmp(testname, "test1d") == 0){
		OSCreateProcess(*test1e);    
	}
	else if (strcmp(testname, "test1e") == 0){
		OSCreateProcess(*test1a);    
	}
	else if (strcmp(testname, "test1f") == 0){
		OSCreateProcess(*test1f);    
	}
	else if (strcmp(testname, "test1g") == 0){
		OSCreateProcess(*test1g);    
	}
	else if (strcmp(testname, "test1h") == 0){
		OSCreateProcess(*test1h);    
	}
	else if (strcmp(testname, "test1i") == 0){
		OSCreateProcess(*test1i);    
	}
	else
	{
		printf("YOU'VE ENTERED WRONG TEST NAME.\n");
	}

	OSCreateProcess(*test1k);    
}                                               // End of osInit

