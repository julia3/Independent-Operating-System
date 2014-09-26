//Jiahua Liu

#include			 "global.h"
#include			 "stdlib.h"
#include		     "string.h"
#include		     "stdlib.h"



#define		MAX_PROCESS_NUMBER		5



typedef struct {
	char process_name[20];
	void *starting_address;
	INT32 initial_priority;
	INT32 pid;
	INT32 error;
	INT32 duetime;// when this process start to execute (sleep time + system time)
	void *pro_context;
} *PCB, PCBentiy;

 typedef struct node {  
    PCB PCB0;  
    struct node *next;  
} PCBnode, *PNode;  

typedef struct  
{  
    PNode front;  
    PNode rear;  
    int size;  
}Queue;  




Queue *InitQueue();
void DestroyQueue(Queue *pqueue);
void ClearQueue(Queue *pqueue);
int IsEmpty(Queue *pqueue);
int GetSize(Queue *pqueue);
PNode GetFront(Queue *pqueue,PCB *pitem);
PNode GetRear(Queue *pqueue,PCB *pitem);
PNode EnQueue(Queue *pqueue,PCB pitem);
PNode DeQueue(Queue *pqueue,PCB *pitem);
PNode RemoveAdvanced(Queue *pqueue,INT32 pid);
INT32 Remove(Queue *pqueue,INT32 pid);
PNode EnQueuePriority(Queue *pqueue,PCB pitem);




  

/*compose a new queue*/ 
Queue *InitQueue()  
{  
    Queue *pqueue = (Queue *)malloc(sizeof(Queue));  
    if(pqueue!=NULL)  
    {  
        pqueue->front = NULL;  
        pqueue->rear = NULL;  
        pqueue->size = 0;  
    }  
    return pqueue;  
}  

  

/*destroy a queue*/ 
void DestroyQueue(Queue *pqueue)  
{  
    if(IsEmpty(pqueue)!=1)  
        ClearQueue(pqueue);  
    free(pqueue);  
}  
  
/*clear a queue*/  
void ClearQueue(Queue *pqueue)  
{  
    while(IsEmpty(pqueue)!=1)  
    {  
        DeQueue(pqueue,NULL);  
    }  
  }  
  
/*check if the queue is empty. If the queue is empty, return 1, if it's not empty, return 0*/  
int IsEmpty(Queue *pqueue)  
{  
    if(pqueue->front==NULL&&pqueue->rear==NULL&&pqueue->size==0)  
        return 1;  
    else  
        return 0;  
}  
  
/*return queue size*/  
int GetSize(Queue *pqueue)  
{  
    return pqueue->size;  
}  
  
/*return the first item of queue*/
PNode GetFront(Queue *pqueue,PCB *pitem)  
{  
    if(IsEmpty(pqueue)!=1&&pitem!=NULL)  
    {  
		*pitem = pqueue->front->PCB0;  
    }  
    return pqueue->front;  
}  
  

/*return the last item of queue*/    
PNode GetRear(Queue *pqueue,PCB *pitem)  
{  
    if(IsEmpty(pqueue)!=1&&pitem!=NULL)  
    {  
        *pitem = pqueue->rear->PCB0;  
    }  
    return pqueue->rear;  
}  



 /*insert a new item to queue  */
PNode EnQueue(Queue *pqueue,PCB pitem)  
{  
    PNode pnode = (PNode)malloc(sizeof(PCBnode));  
    if(pnode != NULL)  
    {  
        pnode->PCB0 = pitem;  
        pnode->next = NULL;  
          
		if(pqueue->front==NULL)  
        {  
            pqueue->front = pnode;  
			
        }  
        else  
        {  
            pqueue->rear->next = pnode;  
        }  
        pqueue->rear = pnode;  
        pqueue->size++;  
	}
    return pnode;  
}  

/*
PNode EnQueue(Queue *pqueue,PCB pitem){
	PNode pnode;
	pnode = pqueue->front;

	while (pnode != NULL){
		pnode = pnode->next;
		pqueue->front = pnode;
	}
}
*/
 
 



/*dequeue the first item*/   
PNode DeQueue(Queue *pqueue,PCB *pitem)  
    {  
        PNode pnode = pqueue->front;  
        if(IsEmpty(pqueue)!=1&&pnode!=NULL)  
        {  
            if(pitem!=NULL)  
                *pitem = pnode->PCB0;  
            pqueue->size--;  
            pqueue->front = pnode->next;  
            free(pnode);  
            if(pqueue->size==0)  
                pqueue->rear = NULL;  
        }  
        return pqueue->front;  
    }  
      

/*remove the PCB with pid from queue, return the removed PCB*/
PNode RemoveAdvanced(Queue *pqueue,INT32 pid){
	int i;
	PNode pnodepre = NULL;
	PNode pnode = pqueue->front; 
	i = pqueue->size;  

	while (pnode!=NULL && (pnode->PCB0->pid)!= pid){//find the next element
		pnodepre = pnode;
		pnode = pnode->next;
	}
	if (pnode == NULL){
		return NULL;
	}
	else
	{
		// if pnode is at the front of queue, clear queue, set size to 1
		if (pnode == pqueue->front){
			if (pqueue->size == 1){
				pqueue->front = NULL;
				pqueue->rear = NULL;
			}
			//if there is more than one element in the queue, move the element after pnode to front
			else{
				pqueue->front = pnode->next;
			}
			pqueue->size--;
			//free(pnode);
			return pnode;
		}
		else{
			// if pnode is the last element in the queue, set pnodepre as the last element of the queue
			if (pnode == pqueue->rear){
			pqueue->rear = pnodepre;
			pnodepre->next = pnode->next;// delete the element from queue
			pqueue->size--;
			//free(pnode);
			return pnode;
			}
		}
	}
}


/*remove the PCB with process_id pid from queue*/
INT32 Remove(Queue *pqueue,INT32 pid){
	int i;
	PNode pnodepre = NULL;
	PNode pnode = pqueue->front;  
	i = pqueue->size;  

	while (pnode!=NULL && (pnode->PCB0->pid)!= pid){// while pnode is not NULL and pnode pid doesn't match
		pnodepre = pnode;
		pnode = pnode->next;// find the next element
	}
	if (pnode == NULL){// reach the end of queue and pid still doesn't match
		return -1;
	}
	else
	{
		if (pnode == pqueue->front){// if front pid match
			if (pqueue->size == 1){// if size of queue = 1, clear queue
				pqueue->front = NULL;
				pqueue->rear = NULL;
			}
			else{// if size of queue > 1, move second element to front
				pqueue->front = pnode->next;
			}
			pqueue->size--;
			free(pnode);
			return 0;
		}
		else{
			// if the last element match, delete rear, and set pnodepre to rear
			if (pnode == pqueue->rear){
			pqueue->rear = pnodepre;
			pnodepre->next = pnode->next;
			pqueue->size--;
			free(pnode);
			return 0;
			}
		}
	}
}



/*
PCB pidtoPCB(Queue *pqueue,INT32 pid){
	int i;

	PNode pnode = pqueue->front; 
	i = pqueue->size;  
	
	if (pid < 0 ){
		return NULL;
	}
	if (pid <= MAX_PROCESS_NUMBER){
		return NULL;
	}
	else {
		while(i--) {  
			if (pnode->PCB0->pid == pid){
			return pnode->PCB0;
			}
		pnode = pnode->next;  
		}  
	}
}
*/




PNode EnQueuePriority(Queue *pqueue,PCB pitem){
	PNode a = (PNode)malloc(sizeof(PCBnode));
	PNode pnodepre = (PNode)malloc(sizeof(PCBnode));
	PNode pnode;  
	a->PCB0 = pitem;
	a->next = NULL;
	
	// if the queue is empty
	if (IsEmpty(pqueue) == 1){
		pqueue->front = a;
		pqueue->rear = a;
		pqueue->size++;
	}
	else {
		// if new PCB's priority is higher than front of queue
		if (pitem->initial_priority < pqueue->front->PCB0->initial_priority){
			a->next = pqueue->front;
			pqueue->front = a;
			pqueue->size++;
		}
		else{
			pnode = pqueue->front;
			// new PCB's priority is lower than pnode
			while(pnode!=NULL && pnode->PCB0->initial_priority <= a->PCB0->initial_priority){
				pnodepre = pnode;
				pnode = pnode->next;// go to next node
			}
			// if new PCB has the lowest priority
			if (pnode==NULL)
			{
				pnodepre->next=a;
				pqueue->rear = a;
				pqueue->size++;
			}
			// new PCB should be insert into queue
			else{
				pnodepre->next = a;
				a->next = pnode;
				pqueue->size++;
			}
		}
	}

	
}


PNode EnQueueTimer(Queue *pqueue,PCB pitem){
	INT32 Time;
	
	PNode a = (PNode)malloc(sizeof(PCBnode));
	PNode pnodepre = (PNode)malloc(sizeof(PCBnode));
	PNode pnode;  
	a->PCB0 = pitem;
	a->next = NULL;
	
	// if the queue is empty
	if (IsEmpty(pqueue) == 1){
		pqueue->front = a;
		pqueue->rear = a;
		pqueue->size++;
		MEM_READ(Z502ClockStatus, &Time);
		MEM_WRITE(Z502TimerStart, &pitem->duetime - Time);
	}
	else {
		// if new PCB is earlier than front of queue
		if (pitem->duetime < pqueue->front->PCB0->duetime){
			a->next = pqueue->front;
			pqueue->front = a;
			pqueue->size++;
			MEM_READ(Z502ClockStatus, &Time);
			MEM_WRITE(Z502TimerStart, &pitem->duetime - Time);
		}
		else{
			pnode = pqueue->front;
			// new PCB is later than pnode
			while(pnode!=NULL && pnode->PCB0->duetime <= a->PCB0->duetime){
				pnodepre = pnode;
				pnode = pnode->next;// go to next node
			}
			// if new PCB is later than all the PCB in this queue
			if (pnode==NULL)
			{
				pnodepre->next=a;
				pqueue->rear = a;
				pqueue->size++;
			}
			// new PCB should be insert into queue
			else{
				pnodepre->next = a;
				a->next = pnode;
				pqueue->size++;
			}
		}
	}
}

