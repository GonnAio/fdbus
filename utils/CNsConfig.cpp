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

#include <utils/CNsConfig.h>

namespace ipc {
namespace fdbus {

int32_t CNsConfig::mMinPort = 60000;
int32_t CNsConfig::mMaxPort = 65000;
int32_t CNsConfig::mAddressBindRetryCnt = NS_CFG_ADDRESS_BIND_RETRY_CNT;

const char *CNsConfig::getNameServerTCPUrl(bool secure)
{
    static std::string url;
    if(secure)
    {
        url = std::string(FDB_URL_TCPS) + FDB_LOCAL_HOST + ":" + std::to_string(mMinPort + 3);
    }
    else
    {
        url = std::string(FDB_URL_TCP) + FDB_LOCAL_HOST + ":" + std::to_string(mMinPort + 2);
    }
    return url.c_str();
}

}
}
