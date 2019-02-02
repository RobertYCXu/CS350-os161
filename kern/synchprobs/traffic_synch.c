#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/*
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/*
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
/* static struct semaphore *intersectionSem; */
static struct lock *curInInter;
static struct cv *N;
static struct cv *S;
static struct cv *E;
static struct cv *W;

static int interState[4];
static int waitTimes[4];

/*
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 *
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

  /* intersectionSem = sem_create("intersectionSem",1); */
  curInInter = lock_create("curInInter");
  if (curInInter == NULL) panic("could not create curInInter lock");
  N = cv_create("N");
  if (N == NULL) panic("could not create N cv");
  S = cv_create("S");
  if (S == NULL) panic("could not create S cv");
  E = cv_create("E");
  if (E == NULL) panic("could not create E cv");
  W = cv_create("W");
  if (W == NULL) panic("could not create W cv");
  /* if (intersectionSem == NULL) { */
  /*   panic("could not create intersection semaphore"); */
  /* } */
  return;
}

/*
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  /* KASSERT(intersectionSem != NULL); */
  KASSERT(curInInter != NULL);
  lock_destroy(curInInter);
  KASSERT(N != NULL);
  cv_destroy(N);
  KASSERT(S != NULL);
  cv_destroy(S);
  KASSERT(E != NULL);
  cv_destroy(E);
  KASSERT(W != NULL);
  cv_destroy(W);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination)
{
  /* replace this default implementation with your own implementation */
  /* (void)origin;  /1* avoid compiler complaint about unused parameter *1/ */
  /* (void)destination; /1* avoid compiler complaint about unused parameter *1/ */
  /* KASSERT(intersectionSem != NULL); */
  /* P(intersectionSem); */
  (void)destination;
  KASSERT(curInInter != NULL);
  lock_acquire(curInInter);
    if (origin == north) {
        while(interState[1] != 0 || interState[2] != 0 || interState[3] != 0) {
          waitTimes[0]++;
          cv_wait(N, curInInter);
        }
        waitTimes[0] = 0;
        interState[0]++;

    }
    else if (origin == south) {
        while(interState[0] != 0 || interState[2] != 0 || interState[3] != 0) {
          waitTimes[1]++;
          cv_wait(S, curInInter);
        }
        waitTimes[1] = 0;
        interState[1]++;
    }
    else if (origin == east) {
        while(interState[0] != 0 || interState[1] != 0 || interState[3] != 0) {
          waitTimes[2]++;
          cv_wait(E, curInInter);
        }
        waitTimes[2] = 0;
        interState[2]++;
    }
    else if (origin == west) {
        while(interState[0] != 0 || interState[1] != 0 || interState[2] != 0) {
          waitTimes[3]++;
          cv_wait(W, curInInter);
        }
        waitTimes[3] = 0;
        interState[3]++;
    }
  lock_release(curInInter);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination)
{
  /* replace this default implementation with your own implementation */
  /* (void)origin;  /1* avoid compiler complaint about unused parameter *1/ */
  /* (void)destination; /1* avoid compiler complaint about unused parameter *1/ */
  /* KASSERT(intersectionSem != NULL); */
  /* V(intersectionSem); */
  KASSERT(curInInter != NULL);
  (void)destination;
  lock_acquire(curInInter);
    if (origin == north) {
      KASSERT(interState[0] > 0);
      interState[0]--;
      if (interState[0] == 0) {
        if (waitTimes[1] >= waitTimes[2] && waitTimes[1] >= waitTimes[3]) {
          cv_broadcast(S, curInInter);
        }
        else if (waitTimes[2] >= waitTimes[1] && waitTimes[2] >= waitTimes[3]) {
          cv_broadcast(E, curInInter);
        }
        else {
          cv_broadcast(W, curInInter);
        }
      }
    }
    else if (origin == south) {
      KASSERT(interState[1] > 0);
      interState[1]--;
      if (interState[1] == 0) {
        if (waitTimes[2] >= waitTimes[3] && waitTimes[2] >= waitTimes[0]) {
          cv_broadcast(E, curInInter);
        }
        else if (waitTimes[0] >= waitTimes[2] && waitTimes[0] >= waitTimes[3]) {
          cv_broadcast(N, curInInter);
        }
        else {
          cv_broadcast(W, curInInter);
        }
      }
    }
    else if (origin == east) {
      KASSERT(interState[2] > 0);
      interState[2]--;
      if (interState[2] == 0) {
        if (waitTimes[3] >= waitTimes[0] && waitTimes[3] >= waitTimes[1]) {
          cv_broadcast(W, curInInter);
        }
        else if (waitTimes[0] >= waitTimes[3] && waitTimes[0] >= waitTimes[1]) {
          cv_broadcast(N, curInInter);
        }
        else {
          cv_broadcast(S, curInInter);
        }
      }
    }
    else {
      KASSERT(interState[3] > 0);
      interState[3]--;
      if (interState[3] == 0) {
        if (waitTimes[0] >= waitTimes[1] && waitTimes[0] >= waitTimes[2]) {
          cv_broadcast(N, curInInter);
        }
        else if (waitTimes[2] >= waitTimes[0] && waitTimes[2] >= waitTimes[1]) {
          cv_broadcast(E, curInInter);
        }

        else {
          cv_broadcast(S, curInInter);
        }
      }
    }
    /* cv_broadcast(N, curInInter); */
    /* cv_broadcast(S, curInInter); */
    /* cv_broadcast(E, curInInter); */
    /* cv_broadcast(W, curInInter); */
  lock_release(curInInter);
}
