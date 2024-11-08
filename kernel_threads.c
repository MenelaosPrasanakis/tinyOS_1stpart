
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  TCB* new_thread;
  new_thread = spawn_thread(CURPROC, start_thread);
  aquire_PTCB(new_thread, task, argl, args);
  PCB* curproc = CURPROC;
  curproc->thread_count++;

  wakeup(new_thread);

  return (Tid_t)new_thread->ptcb;
}
//here we initialize the ptcb and match it with a tcb 
  //refcount is for the threads that waits the ptcb 

void aquire_PTCB(TCB* tcb, Task call, int argl, void* args){
  PTCB* ptcb = (PTCB*)xmalloc(sizeof(PTCB));
  ptcb->tcb = tcb;
  tcb->ptcb = ptcb;
  ptcb->task = call;
  ptcb->argl = argl;
  ptcb->args = args;
  ptcb->exitval = 0;
  ptcb->detached = 0;
  ptcb->exited = 0;
  ptcb->exit_cv = COND_INIT;
  ptcb->refcount=0;

  rlnode_init(&ptcb->ptcb_list_node, ptcb);
  rlist_push_back(&tcb->owner_pcb->ptcb_list, &ptcb->ptcb_list_node);
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
  return (Tid_t) cur_thread()->ptcb;
}

/**
  @brief Join the given thread.
  */
  //
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
 
  PTCB* ptcb=(PTCB*) tid;
  PCB *curproc=CURPROC;
  if (rlist_find(&curproc->ptcb_list,ptcb,NULL)==NULL){
  return -1;
 }

//checking if the given thread tries to join itself
 if(ptcb==cur_thread()->ptcb){
  return -1;
 }
//checking if the ptcb is detached
 if (ptcb->detached==1){
  return -1;
 }
ptcb->refcount++;

while(ptcb->exited!=1 && ptcb->detached!=1){
  kernel_wait(&(ptcb->exit_cv),SCHED_USER);
}

 ptcb->refcount--;
  if (ptcb->detached==1){
  return -1;
 }

if(exitval!=NULL){
  *exitval=ptcb->exitval;
}

//if the given ptcb doesn't refer to any tcb we free that ptcb
if(ptcb->refcount==0){
  rlist_remove(&ptcb->ptcb_list_node);
  free(ptcb);
}

  return 0;
}


/**
  @brief Detach the given thread.
  */

int sys_ThreadDetach(Tid_t tid)
{

  
  PTCB* temp = (PTCB*)tid;
  //checking if the given thread exists
  if(rlist_find(&CURPROC->ptcb_list, (PTCB*)tid, NULL)==NULL) {
    return -1;
  }
  //checking if the given thread is exited
  if(temp->exited == 1){
    return -1;
  }

  {
    
  }
  temp->detached = 1;
  //wakes up all threads with the given cond variable
  kernel_broadcast(&(temp->exit_cv));
  return 0;
}

/**
  @brief Terminate the current thread.
  */

void sys_ThreadExit(int exitval)
{

  PTCB* ptcb = cur_thread()->ptcb;
 //change the status of ptcb to exited 
  ptcb->exitval = exitval;
  ptcb->exited = 1;

  PCB* curproc = CURPROC;
  kernel_broadcast(&(ptcb->exit_cv));

  curproc->thread_count--;

  if(curproc->thread_count==0) {
    //we clean the exit
    if(get_pid(curproc)!=1) {
    // Reparent any children of the exiting process to the
       //initial task
    PCB* initpcb = get_pcb(1);
    while(!is_rlist_empty(& curproc->children_list)) {
      rlnode* child = rlist_pop_front(& curproc->children_list);
      child->pcb->parent = initpcb;
      rlist_push_front(& initpcb->children_list, child);
    }

    // Add exited children to the initial task's exited list
      // and signal the initial task
    if(!is_rlist_empty(& curproc->exited_list)) {
      rlist_append(& initpcb->exited_list, &curproc->exited_list);
      kernel_broadcast(& initpcb->child_exit);
    }

    // Put me into my parent's exited list
    rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
    kernel_broadcast(& curproc->parent->child_exit);

  }

  assert(is_rlist_empty(& curproc->children_list));
  assert(is_rlist_empty(& curproc->exited_list));



  //  Do all the other cleanup we want here, close files etc.

   

  // Release the args data
  if(curproc->args) {
    free(curproc->args);
    curproc->args = NULL;
  }

  // Clean up FIDT
  for(int i=0;i<MAX_FILEID;i++) {
    if(curproc->FIDT[i] != NULL) {
      FCB_decref(curproc->FIDT[i]);
      curproc->FIDT[i] = NULL;
    }

  }
   rlnode *temp;
while(!is_rlist_empty(&(curproc->ptcb_list))){
    
    temp = rlist_pop_front(&(curproc->ptcb_list));
    free(temp->ptcb);
 }


  // Disconnect my main_thread
  curproc->main_thread = NULL;

  // Now, mark the process as exited.
  curproc->pstate = ZOMBIE;

}
  
  kernel_sleep(EXITED, SCHED_USER);


}

