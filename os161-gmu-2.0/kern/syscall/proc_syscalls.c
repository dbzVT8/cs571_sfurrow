/*
 * Process-related system call implementations.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/syscall.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <kern/wait.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <copyinout.h>
#include <vfs.h>
#include <vnode.h>
#include <openfile.h>
#include <filetable.h>
#include <syscall.h>
#include <mips/trapframe.h>
#include <addrspace.h>
#include <thread.h>

int
sys_getpid(pid_t* ret)
{
    KASSERT(curproc != NULL);
    *ret = curproc->p_pid;
    return 0;
}

void
sys__exit(int exitcode)
{

    struct addrspace* as;
    struct proc* p = curproc;
    DEBUG(DB_EXEC, "sys_exit(): process %u exiting with code %d\n",p->p_pid,exitcode);

    KASSERT(p->p_addrspace != NULL);
    as_deactivate();
    as = proc_setas(NULL);
    as_destroy(as);
    proc_remthread(curthread);

    p->p_exitstatus = _MKWAIT_EXIT(exitcode);
    p->p_exitable = true;

    lock_acquire(p->p_waitpid_lk);
        cv_broadcast(p->p_waitpid_cv, p->p_waitpid_lk);
    lock_release(p->p_waitpid_lk);

    proc_destroy(p);
    thread_exit();
    panic("sys__exit(): unexpected return from thread_exit()\n");
}

int
sys_waitpid(pid_t pid,
            userptr_t status,
            int options,
            pid_t* retval)
{
    int exitstatus;
    int result;

    if (options != 0)
    {
        return(EINVAL);
    }

    struct proc* p = curproc;
    struct proc* child = proc_getProc(pid);
    KASSERT(child != NULL);
    DEBUG(DB_EXEC, "sys_waitpid(): process %u waiting on %u\n",p->p_pid,child->p_pid);
    if (p != kproc && child->p_parent != p)
    {
        DEBUG(DB_EXEC, "sys_waitpid(): ECHILD\n");
        return(ECHILD);
    }

    DEBUG(DB_EXEC, "sys_waitpid(): process %u exitable before wait is %d\n",child->p_pid, child->p_exitable);

    lock_acquire(child->p_waitpid_lk);
    while (!child->p_exitable)
    {
        cv_wait(child->p_waitpid_cv, child->p_waitpid_lk);
    }
    lock_release(child->p_waitpid_lk);

    exitstatus = child->p_exitstatus;
    result = copyout((void*)&exitstatus,status,sizeof(int));
    if(result)
    {
        return(result);
    }

    *retval = pid;
    return 0;
}

int
sys_fork(struct trapframe* tf, pid_t* retval)
{
    DEBUG(DB_EXEC,"sys_fork(): entering\n");
    KASSERT(curproc != NULL);
    KASSERT(sizeof(struct trapframe)==(37*4));

    char* child_name = kmalloc(sizeof(char)* NAME_MAX);
    strcpy(child_name, curproc->p_name);
    strcat(child_name, "_c");

    // create PCB for child
    struct proc* child_proc = NULL;
    proc_fork(&child_proc);
    if (child_proc == NULL)
    {
        return ENOMEM;
    }
    strcpy(child_proc->p_name, child_name);
    
    KASSERT(child_proc->p_pid > 0);

    // copy address space and registers from this process to child
    struct addrspace* child_as = kmalloc(sizeof(struct addrspace));
    if (child_as == NULL)
    {
        kfree(child_name);
        proc_destroy(child_proc);
        return ENOMEM;
    }

    struct trapframe* child_tf = kmalloc(sizeof(struct trapframe));
    if (child_tf == NULL)
    {
        kfree(child_name);
        as_destroy(child_as);
        proc_destroy(child_proc);
        return ENOMEM;
    }

    DEBUG(DB_EXEC,"sys_fork(): copying address space...\n");
    // copy the address space just created into the child process's
    // PCB structure
    int result = as_copy(curproc->p_addrspace, &child_as);
    if (result)
    {
        kfree(child_name);
        as_destroy(child_as);
        proc_destroy(child_proc);
        return result;
    }
    child_proc->p_addrspace = child_as;

    DEBUG(DB_EXEC,"sys_fork(): copying trapframe space...\n");
    // copy this process's trapframe to the child process
    memcpy(child_tf, tf, sizeof(struct trapframe));

    
    DEBUG(DB_EXEC, "sys_fork(): copying filetable...\n");
    filetable_copy(curproc->p_filetable,&child_proc->p_filetable);

    DEBUG(DB_EXEC,"sys_fork(): assigning child process's parent as this process...\n");
    // assign child processes parent as this process
    child_proc->p_parent = curproc;

    DEBUG(DB_EXEC,"sys_fork(): adding child to children procarray...\n");
    result = procarray_add(&curproc->p_children, child_proc, NULL);
    if (result)
    {
        DEBUG(DB_EXEC, "sys_fork(): failed to add child process to proc_table...\n");
    }

    DEBUG(DB_EXEC,"sys_fork(): allocating data...\n");
    void **data = kmalloc(2*sizeof(void*));
    data[0] = (void*)child_tf;
    data[1] = (void*)child_as;

    result = thread_fork(child_name, child_proc, &enter_forked_process, data, 0);
    if (result)
    {
        kfree(child_name);
        kfree(child_tf);
        as_destroy(child_as);
        proc_destroy(child_proc);
        return ENOMEM;
    }

    *retval = child_proc->p_pid;
    DEBUG(DB_EXEC, "sys_fork(): thread_fork returned: curproc=%u, child_proc=%u\n",curproc->p_pid,child_proc->p_pid);
    return 0;
}
