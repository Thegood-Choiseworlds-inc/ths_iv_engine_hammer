#ifndef BACKOFF_H
#define BACKOFF_H
#pragma once

#include "vstdlib/random.h"

struct Backoff
{
    int64 minAmount;
    int64 maxAmount;
    int64 current;
    int fails;

    Backoff(int64 min, int64 max)
      : minAmount(min)
      , maxAmount(max)
      , current(min)
      , fails(0)
    {
    }

    void reset()
    {
        fails = 0;
        current = minAmount;
    }

    int64 nextDelay()
    {
        ++fails;
        int64 delay = (int64)((float)current * 2.0f * RandomFloat());
        current = Min(current + delay, maxAmount);
        return current;
    }
};
#endif // BACKOFF_H