#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#if OPT_A2
#include <mips/trapframe.h>
#include <vfs.h>
#include <kern/fcntl.h>
#endif
#include "opt-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
#if OPT_A2
  if (curproc->parent != NULL) {
    lock_acquire(curproc->parent->child_lock);
      for (unsigned int i = 0; i < array_num(curproc->parent->children_info); i++) {
        struct proc_info *p_info = array_get(curproc->parent->children_info, i);
        if (curproc->pid == p_info->pid) {
          p_info->exit_code = exitcode;
          break;
        }
      }
    lock_release(curproc->parent->child_lock);
    cv_signal(curproc->is_dying, curproc->child_lock);
  }
#else
  (void)exitcode;
#endif

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
#if OPT_A2
  *retval = curproc->pid;
#else
  *retval = 1;
#endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
#if OPT_A2
  lock_acquire(curproc->child_lock);
    bool exists = false;
    for (unsigned int i = 0; i < array_num(curproc->children_info); i++) {
      struct proc_info * pinfo = array_get(curproc->children_info, i);
      if (pid == pinfo->pid) {
        exists = true;
        struct proc_info *child = array_get(curproc->children_info, i);
        while (child->exit_code == -1) {
          cv_wait(child->proc_addr->is_dying, curproc->child_lock);
        }
        exitstatus = _MKWAIT_EXIT(child->exit_code);
      }
    }
    if (!exists) {
      lock_release(curproc->child_lock);
      *retval = -1;
      return (ESRCH);
    }
  lock_release(curproc->child_lock);
#else
  exitstatus = 0;
#endif
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

int
sys_fork(struct trapframe *tf, pid_t *retval)
{
  // create child proc
  struct proc *child = proc_create_runprogram(curproc->p_name); // perhaps we want a diff name for child
  KASSERT(child != NULL);
  KASSERT(child->pid > 0);

  // set parent pointer
  child->parent = curproc;

  // add to children_info array
  struct proc_info *child_info = kmalloc(sizeof(struct proc_info));
  child_info->proc_addr = child;
  child_info->exit_code = -1;
  child_info->pid = child->pid;
  array_add(curproc->children_info, child_info, NULL);

  // copy over address space
  int err = as_copy(curproc_getas(), &(child->p_addrspace));
  if (err != 0) panic("copy address space errored");
  // probably don't need this function since we are directly copying address space into child
  // address space
  /* proc_setas(child->p_addrspace, child); */

  // create temp trapframe
  curproc->tf = kmalloc(sizeof(struct trapframe));
  KASSERT(curproc->tf != NULL);
  memcpy(curproc->tf, tf, sizeof(struct trapframe));

  // fork thread with temp trapframe
  thread_fork(child->p_name, child, (void *)&enter_forked_process, curproc->tf, 10);

  *retval =  child->pid;

  return(0);
}

int sys_execv(const char * program_name, char ** args)
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
  (void)args;

  // copy over program name into kernel space
  size_t program_name_size = (strlen(program_name) + 1) * sizeof(char);
  char * program_name_kernel = kmalloc(program_name_size);
  KASSERT(program_name_kernel != NULL);
  int err = copyin((const_userptr_t) program_name, (void *) program_name_kernel, program_name_size);
  KASSERT(err == 0);


  // count number of args and copy into kernel
  int num_args = 0;
  for (int i = 0; args[i] != NULL; i++) {
    num_args++;
  }

  // copy program args into kernel
  size_t args_array_size = (num_args + 1) * sizeof(char *);
  char ** args_kernel = kmalloc(args_array_size);
  KASSERT(args_kernel != NULL);

  for (int i = 0; i <= num_args; i++) {
    if (i == num_args) {
      args_kernel[i] = NULL;
    }
    else {
      size_t arg_size = (strlen(args[i]) + 1) * sizeof(char);
      args_kernel[i] = kmalloc(arg_size);
      KASSERT(args_kernel[i] != NULL);
      int err = copyin((const_userptr_t) args[i], (void *) args_kernel[i], arg_size);
      KASSERT(err == 0);
    }
  }


	/* Open the file. */
	result = vfs_open(program_name_kernel, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	/* KASSERT(curproc_getas() == NULL); */

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	struct addrspace * as_old = curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

  //HARD PART: COPY ARGS TO USER STACK
  vaddr_t temp_stack_ptr = stackptr;
  vaddr_t *stack_args = kmalloc((num_args + 1) * sizeof(vaddr_t));

  for (int i = num_args; i >= 0; i--) {
    if (i == num_args) {
      stack_args[i] = (vaddr_t) NULL;
      continue;
    }
    size_t arg_length = ROUNDUP(strlen(args_kernel[i]) + 1, 4);
    size_t arg_size = arg_length * sizeof(char);
    temp_stack_ptr -= arg_size;
    int err = copyout((void *) args_kernel[i], (userptr_t) temp_stack_ptr, arg_length);
    KASSERT(err == 0);
    stack_args[i] = temp_stack_ptr;
  }

  for (int i = num_args; i >= 0; i--) {
    size_t str_pointer_size = sizeof(vaddr_t);
    temp_stack_ptr -= str_pointer_size;
    int err = copyout((void *) &stack_args[i], (userptr_t) temp_stack_ptr, str_pointer_size);
    KASSERT(err == 0);
  }
  // HARD PART: COPY ARGS TO USER STACK

  as_destroy(as_old);
  kfree(program_name_kernel);
  // might want to free args_kernel

	/* Warp to user mode. */
	enter_new_process(num_args /*argc*/, (userptr_t) temp_stack_ptr /*userspace addr of argv*/,
			  ROUNDUP(temp_stack_ptr, 8), entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
  return EINVAL;

}

