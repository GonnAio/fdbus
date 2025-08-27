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
#ifndef _CBASECONDITIONVARIABLE_HPP_
#define _CBASECONDITIONVARIABLE_HPP_

#include <fdbus/CBaseMutexLock.h>
#include <condition_variable>
#include <fdbus/CBaseSysDep.h>

namespace ipc {
namespace fdbus {

class CBaseConditionVariable
{

public:
    enum class cv_status { no_timeout = 0, timeout = 1, abnormal = 2 };
    enum class ClockType { CLOCK_SYSTEM = 0, CLOCK_STEADY = 1 };

    CBaseConditionVariable(ClockType type);
    virtual ~CBaseConditionVariable();
    CBaseConditionVariable(const CBaseConditionVariable&) = delete;
    CBaseConditionVariable& operator=(const CBaseConditionVariable&) = delete;

    void notify_all() noexcept;
    void notify_one() noexcept;
    void wait(CBaseMutexLock &mutex);

    template <class Rep, class Period>
    std::cv_status wait_for(CBaseMutexLock &mutex, const std::chrono::duration<Rep, Period>& duration){
        if (ClockType::CLOCK_STEADY == mClockType) {
            const std::chrono::steady_clock::time_point monotonicTime = std::chrono::steady_clock::now() + duration;
            return wait_until<std::chrono::steady_clock::time_point>(mutex,monotonicTime);
        }
        else {
            const std::chrono::system_clock::time_point wallTime = std::chrono::system_clock::now() + duration;
            return wait_until<std::chrono::system_clock::time_point>(mutex,wallTime);
        }
    }

    template <typename TimePoint>
    std::cv_status wait_until(CBaseMutexLock& mutex, const TimePoint& timePoint);

private:

    ClockType mClockType;

#ifndef __WIN32__
    CBASE_tThreadCondAttrHnd mThreadCondAttr;
    CBASE_tThreadCondHnd     mThreadCond;
#else
    std::condition_variable_any mConditionVariable;
    bool mCondition;
#endif
};

//Note: The templates for the following data types are defined.
//template std::cv_status CBaseConditionVariable::wait_until<std::chrono::steady_clock::time_point>(CBaseMutexLock&, const std::chrono::steady_clock::time_point&);
//template std::cv_status CBaseConditionVariable::wait_until<std::chrono::system_clock::time_point>(CBaseMutexLock&, const std::chrono::system_clock::time_point&);

}
}
#endif /* Guard for _CBASECONDITIONVARIABLE_HPP_*/
