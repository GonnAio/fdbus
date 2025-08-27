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
    , mCondition(false)
{
}

CBaseConditionVariable::~CBaseConditionVariable()
{
}

void CBaseConditionVariable::notify_all() noexcept
{
    mCondition = true;
    mConditionVariable.notify_all();
}

void CBaseConditionVariable::notify_one() noexcept
{
    mCondition = true;
    mConditionVariable.notify_one();
}

void CBaseConditionVariable::wait(CBaseMutexLock &mutex)
{
    mConditionVariable.wait(mutex);
}

template <typename TimePoint>
std::cv_status CBaseConditionVariable::wait_until(CBaseMutexLock &mutex, const TimePoint& timePoint)
{
    std::cv_status status = std::cv_status::no_timeout;
    mCondition = false;

    bool ret = mConditionVariable.wait_until(mutex, timePoint, [this] { return mCondition; });
    if( ret ) {
        status = std::cv_status::no_timeout;
    }
    else{
        status = std::cv_status::timeout;
    }

    return status;
}

}
}
