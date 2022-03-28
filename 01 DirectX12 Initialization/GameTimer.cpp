#include <windows.h>
#include "GameTimer.h"

GameTimer::GameTimer()
        : mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0),
          mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false)
{
    LARGE_INTEGER countsPerSec;
    // 获取性能计时器的频率（单位：计数/秒）
    QueryPerformanceFrequency(&countsPerSec);
    // 每个计数所代表的秒数（或称几分之一秒），即为上述性能计时器频率的倒数
    mSecondsPerCount = 1.0 / (double) countsPerSec.QuadPart;
}


float GameTimer::DeltaTime() const
{
    return (float) mDeltaTime;
}

void GameTimer::Reset()
{
    __int64 currTime;
    // 性能计时器所用的时间度量单位叫作计数（count）。可调用 QueryPerformanceCounter 函数来
    // 获取性能计时器测量的当前时刻值（以计数为单位）
    QueryPerformanceCounter((LARGE_INTEGER*) &currTime);

    mBaseTime = currTime;
    mPrevTime = currTime;
    mStopTime = 0;
    mStopped  = false;
}

void GameTimer::Start()
{
    __int64 startTime;
    QueryPerformanceCounter((LARGE_INTEGER*) &startTime);

    // 累加调用 stop 和 start 这对方法之间的暂停时刻间隔
    //
    //                     |<-------d------->|
    // ----*---------------*-----------------*------------> 时间
    // mBase Time        mStopTime            startTime

    // 如果从停止状态继续计时的话……
    if (mStopped)
    {
        // 累加暂停时间
        mPausedTime += (startTime - mStopTime);

        // 在重新开启计时器时，前一帧的时间 mPrevTime 是无效的，这是因为它存储的是暂停时前一
        // 帧的开始时刻，因此需要将它重置为当前时刻

        mPrevTime = startTime;

        // 已不再是停止状态……
        mStopTime = 0;
        mStopped  = false;
    }
}

void GameTimer::Stop()
{
    // 如果已经处于停止状态，那就什么也不做
    if (!mStopped)
    {
        __int64 currTime;
        QueryPerformanceCounter((LARGE_INTEGER*) &currTime);

        // 否则，保存停止的时刻，并设置布尔标志，指示计时器已经停止
        mStopTime = currTime;
        mStopped  = true;
    }
}

void GameTimer::Tick()
{
    if (mStopped)
    {
        mDeltaTime = 0.0;
        return;
    }
    // 获得本帧开始显示的时刻
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*) &currTime);
    mCurrTime  = currTime;
    // 本帧与前一帧的时间差
    mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;
    // 准备计算本帧与下一帧的时间差
    mPrevTime  = mCurrTime;

    // 使时间差为非负值。DXSDK 中的 CDXUTTimer 示例注释里提到：如果处理器处于节能模式，或者在
    // 计算两帧间时间差的过程中切换到另一个处理器时（即 QueryPerformanceCounter 函数的两次调
    // 用并非在同一处理器上），则 mDeltaTime 有可能会成为负值
    if (mDeltaTime < 0.0)
    {
        mDeltaTime = 0.0;
    }
}

// 返回自调用 Reset 函数开始不计暂停时间的总时间
float GameTimer::TotalTime() const
{

    if (mStopped)
    {
        // 如果正处于停止状态，则忽略本次停止时刻至当前时刻的这段时间。此外，如果之前已有过暂停的情况，
        // 那么也不应统计 mStopTime – mBaseTime 这段时间内的暂停时间
        // 为了做到这一点，可以从 mStopTime 中再减去暂停时间 mPausedTime
        //
        //                         前一次暂停时间                 当前的暂停时间
        //                     |<--------------->|            |<---------->|
        // ----*---------------*-----------------*------------*------------*------> 时间
        // mBase Time      mStopTime0            startTime    mStopTime     mCurrTime
        return (float) (((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
    } else
    {
        // 我们并不希望统计 mCurrTime – mBaseTime 内的暂停时间
        // 可以通过从 mCurrTime 中再减去暂停时间 mPausedTime 来实现这一点
        //
        // (mCurrTime - mPausedTime) - mBaseTime
        //
        //                     |<--  暂停时间  -->|
        // ----*---------------*-----------------*------------*------> 时间
        // mBaseTime        mStopTime          startTime     mCurrTime
        return (float) (((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
    }
}
