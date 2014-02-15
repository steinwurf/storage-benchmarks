#ifndef PXTIMER_H
#define PXTIMER_H

#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

//Get around Windows hackery
#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

/** PXTimer class. Platform-independent timer. */
class PXTimer
{
private:
	clock_t zeroClock;
#ifdef _WIN32    
    LARGE_INTEGER mStartTime;
    LARGE_INTEGER mFrequency;    
#else
	struct timeval start;
#endif

public:
	PXTimer();
	~PXTimer();

	/** Resets timer */
	void reset();

	/** Changes the timer backwards by 'delta' microseconds */
	void change(long delta);
	
	/** Returns microseconds since initialisation or last reset */
	double get();	

	/** Returns microseconds since initialisation or last reset, only CPU time measured */	
	double getCPU();
};

#endif

