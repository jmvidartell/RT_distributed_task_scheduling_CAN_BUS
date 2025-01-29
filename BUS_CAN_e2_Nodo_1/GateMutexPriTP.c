/*
 * GateMutexPriTP.c
 *
 *  Created on: 9 ene. 2021
 *      Author: jmvid
 */

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Swi.h>
#define ti_sysbios_knl_Task__internalaccess
#include <ti/sysbios/knl/Task.h>
#include "GateMutexPriTP.h"

#define FIRST_ENTER   0
#define NESTED_ENTER  1 /* Not used due to not implementing nested calls. */

/*
 *  ======== GateMutexPriTP_Instance_init ========
 */
Void GateMutexPriTP_Instance_init(GateMutexPriTP_Object *obj, const xdc_Int techo_prio, GateMutexPriTP_Object** serverList, const xdc_Int n_servers)
{
    obj->mutexCnt = 1;
    obj->TP = techo_prio;
    obj->owner = NULL;
    obj->ownerOrigPri = 0;
    obj->queue = Queue_create(NULL, NULL);
    obj->serverList = serverList;
    obj->n_servers = n_servers;
}

/*
 *  ======== GateMutexPriTP_Instance_finalize ========
 */
Void GateMutexPri_Instance_finalize(GateMutexPriTP_Object *obj)
{
    Queue_destruct(Queue_struct(obj->queue));
}


/*
 *  ======== GateMutexPri_insertPri ========
 *  Inserts the element in order by priority, with higher priority
 *  elements at the head of the queue.
 */
Void GateMutexPriTP_insertPri(Queue_Object* queue, Queue_Elem* newElem, Int newPri)
{
    Queue_Elem* qelem;

    /* Iterate over the queue. */
    for (qelem = Queue_head(queue); qelem != (Queue_Elem *)queue;
         qelem = Queue_next(qelem)) {
        /* Tasks of equal priority will be FIFO, so '>', not '>='. */
        if (newPri > Task_getPri(((Task_PendElem *)qelem)->taskHandle)) {
            /* Place the new element in front of the current qelem. */
            Queue_insert(qelem, newElem);
            return;
        }
    }

    /*
     * Put the task at the back of the queue if:
     *   1. The queue was empty.
     *   2. The task had the lowest priority in the queue.
     */
    Queue_enqueue(queue, newElem);
}

/* Returns if the server is currently owned by a task */
/* To call this function disable task scheduler first (task_disable)*/
int is_serving(GateMutexPriTP_Object *obj){
    if(obj->mutexCnt == 0){
        return 1; //Mtx taken
    }else{
        return 0; //Mtx free
    }
}

void unblock_waiting_task(GateMutexPriTP_Object *obj){
    UInt hwiKey;
    Task_Handle newOwner;
    Task_PendElem *elem;
    /*
     * Get the next owner from the front of the queue (the task with the
     * highest priority of those waiting on the queue).
     */
    elem = (Task_PendElem *)Queue_dequeue(obj->queue);
    newOwner = elem->taskHandle;

    /* Setup the gate. */
    obj->mutexCnt = 0;
    obj->owner = newOwner;
    obj->ownerOrigPri = Task_getPri(newOwner);
    //Task_setPri(obj->owner, obj->TP);   /* Insert priority celling value to the task priority. */ /* this is inmediate priority celling */

    /* Task_unblockI must be called with interrupts disabled. */
    hwiKey = Hwi_disable();
    Task_unblockI(newOwner, hwiKey);
    Hwi_restore(hwiKey);
}

/*
 *  ======== GateMutexPriTP_Gate ========
 */
IArg GateMutexPriTP_enter(GateMutexPriTP_Object *obj)
{
    Task_Handle tsk;
    UInt tskKey;
    Int tskPri;
    Task_PendElem elem;
    UInt blocked_by_server = 0;

    tsk = Task_self();

    tskPri = Task_getPri(tsk);

    /*
     * Gate may only be called from task context, so Task_disable is sufficient
     * protection.
     *
     * Avoid preemption caused by another tasks -> disable task scheduler to contain "critical sections".
     */
    tskKey = Task_disable();

    /* Verify if there is any other server with priority celling >= tskPri serving to another task */
    ///*
    int i;
    for(i=0; i < obj->n_servers; i++){
        if((obj != obj->serverList[i]) && is_serving(obj->serverList[i]) && (obj->serverList[i]->TP >= tskPri)){
            blocked_by_server = 1;
            break;
        }
    }
    //*/

    /* If there is not any server with priority celling >= tskPri serving and the gate is free and , take it. */
    if (!blocked_by_server && obj->mutexCnt == 1) {
        obj->mutexCnt = 0;
        obj->owner = tsk;
        obj->ownerOrigPri = tskPri;
        //Task_setPri(obj->owner, obj->TP); /* Insert priority celling value to the task priority */ /* this is inmediate priority celling */
        Task_restore(tskKey);
        return FIRST_ENTER;
    }

    /* At this point, the gate is already held by someone or task has been blocked by another server. */

    /* Remove tsk from ready list. */
    Task_block(tsk);

    elem.taskHandle = tsk;
    elem.clockHandle = NULL;
    /* leave a pointer for Task_delete() */
    tsk->pendElem = &elem;

    /* Insert tsk in wait queue in order by priority (high pri at head) */
    GateMutexPriTP_insertPri(obj->queue, (Queue_Elem *)&elem, tskPri);

    /*
     * Donate priority if necessary. The owner is guaranteed to have the
     * highest priority of anyone waiting on the gate, so just compare this
     * task's priority against the owner's.
     */
    ///*
    if (!blocked_by_server && tskPri > Task_getPri(obj->owner)) {
        Task_setPri(obj->owner, tskPri);
    }
    //*/

    /* Task_restore will call the scheduler and this task will block. */
    Task_restore(tskKey);

    tsk->pendElem = NULL;

    /*
     * At this point, tsk has the gate. Initialization of the gate is handled
     * by the previous owner's call to leave.
     */
    return FIRST_ENTER;
}

/*
 *  ======== GateMutexPriTP_leave ========
 *  Only releases the gate if key == FIRST_ENTER,
 *  in this case always due that nested calls are not used.
 */
Void GateMutexPriTP_leave(GateMutexPriTP_Object *obj, IArg key)
{
    UInt tskKey;
    Task_Handle owner;

    owner = Task_self();

    /*
     * Gate may only be called from task context, so Task_disable is sufficient
     * protection.
     *
     * Avoid preemption caused by another tasks -> disable task scheduler to contain "critical sections".
     */
    tskKey = Task_disable();

    /*
     * Restore this task's priority. The if-test is worthwhile because of the
     * cost of a call to setPri.
     */
    if (obj->ownerOrigPri < Task_getPri(owner)) {
        Task_setPri(owner, obj->ownerOrigPri);
    }

    /* If the list of waiting tasks is not empty... */
    if (!Queue_empty(obj->queue)) {
        unblock_waiting_task(obj);
    }else{
        obj->mutexCnt = 1; /* Free the gate */
        ///*
        int i;
        for(i=0; i < obj->n_servers; i++){
            if((obj->serverList[i]->TP <= obj->TP) && !is_serving(obj->serverList[i]) && !Queue_empty(obj->serverList[i]->queue)){
                unblock_waiting_task(obj->serverList[i]);
                break;
            }
        }
        //*/

    }

    Task_restore(tskKey);
}
