#ifndef __GT_INCLUDE_H
#define __GT_INCLUDE_H

#include "gt_signal.h"
#include "gt_spinlock.h"
#include "gt_tailq.h"
#include "gt_bitops.h"

#include "gt_uthread.h"
#include "gt_pq.h"
#include "gt_kthread.h"

extern int kthreads_finished;
extern int gt_yield;
extern double uthread_lifespan[2][128];
extern double uthread_lifespan_usec[2][128];

extern double uthread_executiontime[3][128];
extern double uthread_executiontime_usec[3][128];

#endif
