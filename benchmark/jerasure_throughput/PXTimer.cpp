#include "PXTimer.h"


//-------------------------------------------------------------------------
PXTimer::PXTimer()
{
	reset();
}

//-------------------------------------------------------------------------
PXTimer::~PXTimer()
{
}

// Changes the timer backwards by 'delta' microseconds
void PXTimer::change(long delta)
{
#ifdef _WIN32
	mStartTime.QuadPart = mStartTime.QuadPart - (delta * mFrequency.QuadPart / 1000000);
#else
// 	if (start.tv_usec >= delta)
// 		start.tv_usec -= delta;
// 	else
// 	{
// 		start.tv_sec -= delta / 1000000 + 1;
// 		start.tv_usec = start.tv_usec - delta + 1000000;
// 	}
#endif

}

//-------------------------------------------------------------------------
void PXTimer::reset()
{
	zeroClock = clock();
#ifndef _WIN32
	//gettimeofday(&start, NULL);
    clock_gettime(CLOCK_MONOTONIC, &start);
#else

	QueryPerformanceFrequency(&mFrequency);
	QueryPerformanceCounter(&mStartTime);
#endif
}



//-------------------------------------------------------------------------
double PXTimer::get()
{
#ifdef _WIN32
	LARGE_INTEGER curTime;
	QueryPerformanceCounter(&curTime);
	LONGLONG diff = curTime.QuadPart - mStartTime.QuadPart;

	// scale by 1000000 for microseconds
	//unsigned long newMicro = (unsigned long) (1000000 * newTime / mFrequency.QuadPart);

	return (double)diff / mFrequency.QuadPart;

#else
	struct timespec now;
	//gettimeofday(&now, NULL);
    clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec-start.tv_sec)+(now.tv_nsec-start.tv_nsec)*0.000000001;
#endif
}


//-------------------------------------------------------------------------
double PXTimer::getCPU()
{
	clock_t newClock = clock();
	return (double)(newClock-zeroClock) / CLOCKS_PER_SEC;
}