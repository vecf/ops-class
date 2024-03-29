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
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

 /*	vecf:
The approach is simple yet effective:

Each car needs access to a quadrant via a semaphore before accessing it.
Since only one car is allowed in a quadrant at once, this semaphore's value
is one. This avoids collisions.

To keep things simple, we only let the cars move in conventional ways.
Per thread, this limits gostraight, turnleft and turnright to acquire the 
quadrant semaphores in the same predefined order every time.

To avoid deadlocks, the intersection capacity is controlled via a semaphore.
Given the limited ways in which the cars can move, the intersection can only
deadlock when there are four cars in it at once. The intersection semaphore
is therefore initialized to 3.
*/




#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define NUMQUADS 4

// Signal entry and exit quadrants
#define INQ 4
#define OUTQ 5

// Shared resource capacities
#define INTERSECTION_SIMUL (NUMQUADS - 1)
#define QUAD_SIMUL 1

// Quadrant relative to entry direction
#define FORW		(direction)
#define TWO_FORW	( (direction + 3)%4 ) 
#define TWO_FORW_LEFT	( (direction + 2)%4 )

struct semaphore *intersection;
struct semaphore *quads[NUMQUADS];

void gonext(uint32_t index, uint32_t next, uint32_t prev); 

void
stoplight_init() {
	// Called by the driver during initialization.

	intersection = sem_create("isem", INTERSECTION_SIMUL);
	for (int i = 0; i<NUMQUADS; ++i) {
		quads[i] = sem_create("qsem", QUAD_SIMUL);
	}
	return;
}

void stoplight_cleanup() {
	// Called by the driver during teardown.

	sem_destroy(intersection);
	for (int i = 0; i<NUMQUADS; ++i) {
		sem_destroy(quads[i]);
	}
	return;
}


void 
gonext(uint32_t index, uint32_t nextquad, uint32_t prevquad)
{
	/* wait for access to next quad, 
	 * move to next quad,
	 * release prev quad.
	 * prevquad = INQ on entry
	 * nextquad = OUTQ on exit */

	if (nextquad != OUTQ) {
		P(quads[nextquad]);
		inQuadrant(nextquad, index);
	} else {
		leaveIntersection(index);
	}

	if (prevquad != INQ)
		V(quads[prevquad]);
}

void
turnright(uint32_t direction, uint32_t index)
{
	P(intersection);
	gonext(index, FORW, INQ);
	gonext(index, OUTQ, FORW);
	V(intersection);
	return;
}

void
gostraight(uint32_t direction, uint32_t index)
{
	P(intersection);
	gonext(index, FORW, INQ);
	gonext(index, TWO_FORW, FORW); 
	gonext(index, OUTQ, TWO_FORW);
	V(intersection);
	return;
}

void
turnleft(uint32_t direction, uint32_t index)
{
	P(intersection);
	gonext(index, FORW, INQ);
	gonext(index, TWO_FORW, FORW); 
	gonext(index, TWO_FORW_LEFT, TWO_FORW);
	gonext(index, OUTQ, TWO_FORW_LEFT);
	V(intersection);
	return;
}
