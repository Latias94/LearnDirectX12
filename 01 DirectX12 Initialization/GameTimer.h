#pragma once

#include <cstdio>
// 为了精确地度量时间，我们将采用性能计时器（performance timer。或称性能计数器，performance counter）
// 为了计算从程序开始到某时刻的总时间，我们给程序创建了一个 GameTimer 实例。
// 其实，我们也可以再创建一个 GameTimer 实例，把它当作一个通用的“秒表”。
// 例如，当游戏中的炸弹被点燃时，我们可以开启一个新的 GameTimer 实例，当 TotalTime 达到 5 秒时就触发爆炸事件。
class GameTimer
{
public:
    GameTimer();

    float TotalTime() const; // 用秒作为单位
    float DeltaTime() const; // 用秒作为单位

    void Reset(); // 在开始消息循环之前调用
    void Start(); // 解除计时器暂停时调用
    void Stop();  // 暂停计时器时调用
    void Tick();  // 每帧都要调用

private:
    double mSecondsPerCount;
    double mDeltaTime;

    // 统计总时间
    __int64 mBaseTime; // 可以把这个时刻当作应用程序的开始时刻。
    __int64 mPausedTime; // 所有暂停时间之和
    __int64 mStopTime; // 计时器停止（暂停）的时刻，借此即可记录暂停的时间。

    __int64 mPrevTime;
    __int64 mCurrTime;

    bool mStopped;
};
