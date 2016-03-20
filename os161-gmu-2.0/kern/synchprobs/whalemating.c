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

/*
 * Driver code for whale mating problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define NMATING 10

// Male related synchs
static struct lock *male_lk;
static struct lock *female_lk;
static struct lock *match_lk;

static struct lock *mate_lk;
static struct cv *mate_cv;

static struct semaphore *mate_sem;
static struct semaphore *finished_sem;

static
void
init(void)
{
    if (male_lk==NULL) {
        male_lk = lock_create("male_lk");
        if (male_lk == NULL) {
            panic("whalemating: Could not create male lock\n");
        }
    }

    if (female_lk==NULL) {
        female_lk = lock_create("female_lk");
        if (female_lk == NULL) {
            panic("whalemating: Could not create female lock\n");
        }
    }

    if (match_lk==NULL) {
        match_lk = lock_create("match_lk");
        if (match_lk == NULL) {
            panic("whalemating: Could not create match lock\n");
        }
    }

    if (mate_lk==NULL) {
        mate_lk = lock_create("mate_lk");
        if (mate_lk == NULL) {
            panic("whalemating: Could not create mate lock\n");
        }
    }

    if (mate_cv==NULL) {
        mate_cv = cv_create("mate_cv");
        if (mate_cv == NULL) {
            panic("whalemating: Could not create mate cv\n");
        }
    }

    if (mate_sem==NULL) {
        mate_sem = sem_create("mate_sem", 3);
        if (mate_sem == NULL) {
            panic("whalemating: Could not create mate sem\n");
        }
    }

    if (finished_sem==NULL) {
        finished_sem = sem_create("finished_sem", 0);
        if (finished_sem == NULL) {
            panic("whalemating: Could not create finished sem\n");
        }
    }
}

static
void
male(void *p, unsigned long which)
{
	(void)p;
	kprintf("Male whale #%ld started\n", which);
    lock_acquire(male_lk);
    P(mate_sem);
    lock_acquire(mate_lk);

    if(mate_sem->sem_count != 0) {
        cv_wait(mate_cv,mate_lk);
    } else {
        cv_broadcast(mate_cv,mate_lk);
    }

    lock_release(mate_lk);
    V(mate_sem);
    lock_acquire(mate_lk);

    if(mate_sem->sem_count != 3) {
        cv_wait(mate_cv,mate_lk);
    } else {
        cv_broadcast(mate_cv,mate_lk);
    }

    lock_release(mate_lk);
    kprintf("Male whale #%ld finished mating\n", which);
    lock_release(male_lk);
    V(finished_sem);
}

static
void
female(void *p, unsigned long which)
{
	(void)p;
	kprintf("Female whale #%ld started\n", which);
    lock_acquire(female_lk);
    P(mate_sem);
    lock_acquire(mate_lk);

    if(mate_sem->sem_count != 0) {
        cv_wait(mate_cv,mate_lk);
    } else {
        cv_broadcast(mate_cv,mate_lk);
    }

    lock_release(mate_lk);
    V(mate_sem);
    lock_acquire(mate_lk);

    if(mate_sem->sem_count != 3) {
        cv_wait(mate_cv,mate_lk);
    } else {
        cv_broadcast(mate_cv,mate_lk);
    }

    lock_release(mate_lk);
    kprintf("Female whale #%ld finished mating\n", which);
    lock_release(female_lk);
    V(finished_sem);
}

static
void
matchmaker(void *p, unsigned long which)
{
	(void)p;
	kprintf("Matchmaker whale #%ld started\n", which);
    lock_acquire(match_lk);
    P(mate_sem);
    lock_acquire(mate_lk);

    if(mate_sem->sem_count != 0) {
        cv_wait(mate_cv,mate_lk);
    } else {
        cv_broadcast(mate_cv,mate_lk);
    }

    lock_release(mate_lk);
    V(mate_sem);
    lock_acquire(mate_lk);

    if(mate_sem->sem_count != 3) {
        cv_wait(mate_cv,mate_lk);
    } else {
        cv_broadcast(mate_cv,mate_lk);
    }

    lock_release(mate_lk);
    lock_release(match_lk);
    kprintf("Matchmaker whale #%ld finished making a match!\n", which);
    V(finished_sem);
}

int
whalemating(int nargs, char **args)
{
	int i, j, err=0;

	(void)nargs;
	(void)args;
    init();

	for (i = 0; i < 3; i++) {
		for (j = 0; j < NMATING; j++) {
			switch(i) {
			    case 0:
				err = thread_fork("Male Whale Thread",
						  NULL, male, NULL, j);
				break;
			    case 1:
				err = thread_fork("Female Whale Thread",
						  NULL, female, NULL, j);
				break;
			    case 2:
				err = thread_fork("Matchmaker Whale Thread",
						  NULL, matchmaker, NULL, j);
				break;
			}
			if (err) {
				panic("whalemating: thread_fork failed: %s)\n",
				      strerror(err));
			}
		}
	}

    for(i=0;i<3*NMATING;i++) {
        P(finished_sem);
    }

    kprintf("Whalemating problem solved!\n");
	return 0;
}
