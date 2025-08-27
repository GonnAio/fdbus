/*
 * Copyright (C) 2015   Jeremy Chen jeremy_cz@yahoo.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fdbus/CBaseConditionVariable.h>

namespace ipc {
namespace fdbus {
template std::cv_status CBaseConditionVariable::wait_until<std::chrono::steady_clock::time_point>(CBaseMutexLock&, const std::chrono::steady_clock::time_point&);
template std::cv_status CBaseConditionVariable::wait_until<std::chrono::system_clock::time_point>(CBaseMutexLock&, const std::chrono::system_clock::time_point&);

CBaseConditionVariable::CBaseConditionVariable(ClockType type)
    : mClockType(type)
{
    pthread_condattr_init(&mThreadCondAttr);
    if (ClockType::CLOCK_STEADY == type) {
        pthread_condattr_setclock(&mThreadCondAttr, CLOCK_MONOTONIC);
    } else {
        pthread_condattr_setclock(&mThreadCondAttr, CLOCK_REALTIME);
    }
    pthread_cond_init(&mThreadCond,&mThreadCondAttr);
}

CBaseConditionVariable::~CBaseConditionVariable()
{
    pthread_condattr_destroy(&mThreadCondAttr);
    pthread_cond_destroy(&mThreadCond);
}

void CBaseConditionVariable::notify_all() noexcept
{
    pthread_cond_broadcast(&mThreadCond);
}

void CBaseConditionVariable::notify_one() noexcept
{
    pthread_cond_signal(&mThreadCond);
}

void CBaseConditionVariable::wait(CBaseMutexLock &mutex)
{
    pthread_cond_wait(&mThreadCond, &mutex.mMutex);
}

template <typename TimePoint>
std::cv_status CBaseConditionVariable::wait_until(CBaseMutexLock& mutex, const TimePoint& timePoint)
{
    std::cv_status status = std::cv_status::no_timeout;

    std::chrono::nanoseconds ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(timePoint).time_since_epoch();
    std::chrono::seconds sec = std::chrono::duration_cast<std::chrono::seconds>(ns);

    timespec ts;
    ts.tv_sec = sec.count();
    ts.tv_nsec = (ns - sec).count();

    int32_t ret = pthread_cond_timedwait(&mThreadCond, &mutex.mMutex, &ts);
    if(0 == ret)
    {
        status = std::cv_status::no_timeout;
    }
    else if(ETIMEDOUT == ret)
    {
        status = std::cv_status::timeout;
    }
    else
    {
        status = std::cv_status::no_timeout;
    }
    return status;
}

}
}
