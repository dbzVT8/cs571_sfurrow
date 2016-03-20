/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define NPEOPLE 20

#define EMPTY 0
#define BOY 1
#define GIRL 2

static struct lock* bath_lk;
static struct semaphore *bath_sem;
static struct cv* boy_cv;
static struct cv* girl_cv;
static struct semaphore* finished_sem;
static volatile int currently_serving = EMPTY;
static volatile int boys_waiting = 0;
static volatile int girls_waiting = 0;

static
void
init(void)
{
    if (bath_sem==NULL) {
        bath_sem = sem_create("bath_sem", 3);
        if (bath_sem == NULL) {
            panic("bathroom: Failed to create bath_sem\n");
        }
    }
        
    if (boy_cv==NULL) {
        boy_cv = cv_create("boy_cv");
        if (boy_cv == NULL) {
            panic("bathroom: Failed to create boy_cv\n");
        }
    }

    if (girl_cv==NULL) {
        girl_cv = cv_create("girl_cv");
        if (girl_cv == NULL) {
            panic("bathroom: Failed to create girl_cv\n");
        }
    }
    
    if (bath_lk==NULL) {
        bath_lk = lock_create("bath_lk");
        if (bath_lk == NULL) {
            panic("bathroom: Failed to create bath_lk\n");
        }
    }

    if (finished_sem==NULL) {
        finished_sem = sem_create("finished_sem", 0);
        if (finished_sem == NULL) {
            panic("bathroom: Failed to create finished_sem\n");
        }
    }
}

static
void
shower()
{
	// The thread enjoys a refreshing shower!
    clocksleep(1);
}

//Fairness policy attempts to avoid having unequal number of
//boys and girls waiting if possible but to go ahead and
//service whatever gender is already in the bathroom if the
//bathroom is not full.

static
void
boy(void *p, unsigned long which)
{
	(void)p;
	kprintf("boy #%ld starting\n", which);
    lock_acquire(bath_lk);
    // if the bathroom is full, is currently serving girls, or
    // there are more girls waiting, then wait.
    if(bath_sem->sem_count == 0 || currently_serving == GIRL || boys_waiting < girls_waiting)
    {
        boys_waiting++;
        cv_wait(boy_cv,bath_lk);
        boys_waiting--;
    }
    lock_release(bath_lk);
    P(bath_sem);      
    currently_serving = BOY;

    // use bathroom
    kprintf("boy #%ld entering bathroom...\n", which);
    shower();
    kprintf("boy #%ld leaving bathroom\n", which);

    V(bath_sem);
    if(bath_sem->sem_count == 3)
        currently_serving = EMPTY;
    lock_acquire(bath_lk);
    if(boys_waiting > girls_waiting)
        cv_broadcast(boy_cv,bath_lk);
    else if(currently_serving != BOY)
        cv_broadcast(girl_cv,bath_lk);
    lock_release(bath_lk);
    V(finished_sem);
}

static
void
girl(void *p, unsigned long which)
{
	(void)p;
	kprintf("girl #%ld starting\n", which);
    lock_acquire(bath_lk);
    // if the bathroom is full, is currently serving boys, or
    // there are more boys waiting, then wait.
    if(bath_sem->sem_count == 0 || currently_serving == BOY || girls_waiting < boys_waiting)
    {
        girls_waiting++;
        cv_wait(girl_cv,bath_lk);
        girls_waiting--;
    }
    lock_release(bath_lk);
    P(bath_sem);      
    currently_serving = GIRL;

    // use bathroom
    kprintf("girl #%ld entering bathroom\n", which);
    shower();
    kprintf("girl #%ld leaving bathroom\n", which);

    V(bath_sem);
    if(bath_sem->sem_count == 3)
        currently_serving = EMPTY;
    lock_acquire(bath_lk);
    if(girls_waiting > boys_waiting)
        cv_broadcast(girl_cv,bath_lk);
    else if(currently_serving != GIRL)
        cv_broadcast(boy_cv,bath_lk);
    lock_release(bath_lk);
    V(finished_sem);
}

// Change this function as necessary
int
bathroom(int nargs, char **args)
{

	int i, err=0;

	(void)nargs;
	(void)args;
    init();

	for (i = 0; i < NPEOPLE; i++) {
		switch(i % 2) {
			case 0:
			err = thread_fork("Boy Thread", NULL,
					  boy, NULL, i);
			break;
			case 1:
			err = thread_fork("Girl Thread", NULL,
					  girl, NULL, i);
			break;
		}
		if (err) {
			panic("bathroom: thread_fork failed: %s)\n",
				strerror(err));
		}
	}

    for(i=0;i<NPEOPLE;i++) {
        P(finished_sem);
    }

	return 0;
}
