# 对端Name Server接收地址的调用流程和日志分析

## 完整调用流程

当对端Name Server收到本机发送的服务地址和端口后，会经历以下调用流程：

### 1. TCP接收阶段

```cpp
CInterNameProxy::ConnectAddrSubscriber::onBroadcast()  // TCP消息到达
    ↓
CInterNameProxy::onServiceBroadcast()                  // 处理服务广播
    ↓
CNameServer::broadcastSvcAddrLocal()                   // 转发给本地客户端
    ↓
CIntraNameProxy::onServiceBroadcast()                  // 客户端接收
    ↓
CIntraNameProxy::connectToServer()                     // 发起连接
    ↓
CIntraNameProxy::doConnectToServer()                   // 实际连接处理
    ↓
CIntraNameProxy::doConnectToAddress()                  // 建立TCP连接
    ↓
CBaseClient::doConnect()                               // 底层连接
```

## 详细接口调用和日志

### 阶段1：CInterNameProxy接收TCP消息

```cpp
// 1.1 订阅消息到达
void CInterNameProxy::ConnectAddrSubscriber::onBroadcast(CBaseJob::Ptr &msg_ref)
{
    // 解析消息
    FdbMsgAddressList msg_addr_list;
    CFdbParcelableParser parser(msg_addr_list);
    if (!msg->deserialize(parser))
    {
        return;  // 解析失败，无日志
    }
    
    // 调用服务广播处理
    mNsProxy->onServiceBroadcast(msg, mType);
}

// 此阶段无日志输出
```

### 阶段2：处理服务广播

```cpp
// 1.2 处理服务地址广播
void CInterNameProxy::onServiceBroadcast(CFdbMessage *msg, SubscribeType subscribe_type)
{
    // subscribe_type = INTER_NORMAL
    auto forward_type = INTRA_NORMAL;  // 转换为本地订阅类型
    
    // 解析地址列表
    FdbMsgAddressList msg_addr_list;
    // msg_addr_list = {
    //     service_name: "ivi_server"
    //     address: "tcp://192.168.1.100:3456"
    //     instance_id: 0
    //     host_name: "remote_host"
    // }
    
    // 检查是否有本地客户端订阅了该服务
    tSubscribedSessionSets sessions;
    mContainer->nameServer()->getSvcSubscribeTable(msg->code(), svc_name, sessions, forward_type);
    
    if (sessions.empty())
    {
        // 如果没有本地客户端订阅，取消远程订阅
        name_server->recallServiceListener(msg->code(), svc_name, subscribe_type);
        return;
    }
    
    // 验证和调整URL（处理0.0.0.0等特殊地址）
    validateUrl(msg_addr_list, session);
    
    // 转发给本地客户端
    name_server->broadcastSvcAddrLocal(tokens, msg_addr_list);
}

// 此阶段无日志输出（除非出错）
```

### 阶段3：本地Name Server转发

```cpp
void CNameServer::broadcastSvcAddrLocal(const CFdbToken::tTokenList &tokens,
                                        FdbMsgAddressList &addr_list)
{
    // 通过IPC发送给本地客户端
    // 使用mIntraNormalPublisher广播
    
    // 此阶段无日志
}
```

### 阶段4：客户端接收并连接

```cpp
// 4.1 客户端的CIntraNameProxy接收广播
void CIntraNameProxy::onServiceBroadcast(CBaseJob::Ptr &msg_ref)
{
    connectToServer(msg, ctx_id, ep_id);
}

// 4.2 连接到服务器
void CIntraNameProxy::doConnectToServer(CFdbBaseContext *context, 
                                        FdbEndpointId_t ep_id,
                                        FdbMsgAddressList &msg_addr_list, 
                                        bool is_init_response)
{
    auto svc_name = msg_addr_list.service_name().c_str();  // "ivi_server"
    auto instance_id = msg_addr_list.instance_id();        // 0
    const std::string &host_name = msg_addr_list.host_name(); // "remote_host"
    bool is_offline = msg_addr_list.address_list().empty();
    
    // 遍历所有匹配的客户端
    for (each matching client)
    {
        if (is_offline)
        {
            // 服务下线
            client->doDisconnect();
            
            // 🔴 日志1：服务下线
            LOG_E("CIntraNameProxy Client %s:%d is disconnected by %s!\n", 
                  svc_name, instance_id, host_name.c_str());
            // 输出: "CIntraNameProxy Client ivi_server:0 is disconnected by remote_host!"
        }
        else
        {
            // 服务上线，建立连接
            doConnectToAddress(client, msg_addr_list, udp_failure, udp_success, tcp_failure);
        }
    }
}

// 4.3 建立实际连接
void CIntraNameProxy::doConnectToAddress(CBaseClient *client,
                                         FdbMsgAddressList &msg_addr_list,
                                         bool &udp_failure,
                                         bool &udp_success,
                                         bool &tcp_failure)
{
    const char *svc_name = msg_addr_list.service_name().c_str();
    const char *host_name = msg_addr_list.host_name().c_str();
    auto instance_id = msg_addr_list.instance_id();
    
    // 遍历地址列表（通常只有一个TCP地址）
    for (auto it = addr_list.vpool().begin(); it != addr_list.vpool().end(); ++it)
    {
        // it->tcp_ipc_url() = "tcp://192.168.1.100:3456"
        
        auto session_container = client->doConnect(it->tcp_ipc_url().c_str(),
                                                   host_name, udp_port);
        
        if (session_container)
        {
            if (client->UDPEnabled() && (udp_port > FDB_INET_PORT_NOBIND))
            {
                if (!获取UDP端口成功)
                {
                    // 🔴 日志2：UDP连接失败
                    LOG_I("CIntraNameProxy: Server: %s:%d, address %s UDP %d is connected but UDP fail.\n",
                          svc_name, instance_id, it->tcp_ipc_url().c_str(), udp_port);
                    // 输出: "CIntraNameProxy: Server: ivi_server:0, address tcp://192.168.1.100:3456 UDP 4567 is connected but UDP fail."
                }
                else
                {
                    // 🟢 日志3：连接成功（含UDP）
                    LOG_I("CIntraNameProxy: Server: %s:%d, address %s UDP %d is connected.\n",
                          svc_name, instance_id, it->tcp_ipc_url().c_str(), socket_info.mAddress->mPort);
                    // 输出: "CIntraNameProxy: Server: ivi_server:0, address tcp://192.168.1.100:3456 UDP 4567 is connected."
                }
            }
        }
        else
        {
            // 🔴 日志4：TCP连接失败
            LOG_I("CIntraNameProxy: Server: %s:%d, address %s fail to connect TCP.\n",
                  svc_name, instance_id, it->tcp_ipc_url().c_str());
            // 输出: "CIntraNameProxy: Server: ivi_server:0, address tcp://192.168.1.100:3456 fail to connect TCP."
        }
    }
}
```

### 阶段5：底层TCP连接

```cpp
CClientSocket *CBaseClient::doConnect(const char *url, const char *host_name, int32_t udp_port)
{
    // url = "tcp://192.168.1.100:3456"
    
    CFdbSocketAddr addr;
    if (!CBaseSocketFactory::parseUrl(url, addr))
    {
        // 🔴 日志5：URL解析失败
        LOG_E("CBaseClient: unable to parse url: %s!\n", url);
        return 0;
    }
    
    if (addr.mSecure && !TCPSecureEnabled())
    {
        // 🔴 日志6：安全连接失败
        LOG_I("CBaseClient: unable to connect to %s since security is not enabled!\n", url);
        return 0;
    }
    
    // 创建TCP socket并连接
    auto client_imp = CBaseSocketFactory::createClientSocket(addr);
    auto sk = new CClientSocket(this, skid, client_imp, host_name, udp_port, addr.mSecure);
    auto session = sk->connect();
    
    // 连接成功后，通知上层
    notifyConnectReady(CONNECTED);
    
    return sk;
}
```

## 预期日志输出

### 成功场景（TCP连接成功）

```bash
# 对端接收到地址后的日志序列：
[CIntraNameProxy] Server: ivi_server:0, address tcp://192.168.1.100:3456 UDP 4567 is connected.
```

### 失败场景1（TCP连接失败）

```bash
[CIntraNameProxy] Server: ivi_server:0, address tcp://192.168.1.100:3456 fail to connect TCP.
```

### 失败场景2（服务下线）

```bash
[CIntraNameProxy] Client ivi_server:0 is disconnected by remote_host!
```

### 失败场景3（UDP失败但TCP成功）

```bash
[CIntraNameProxy] Server: ivi_server:0, address tcp://192.168.1.100:3456 UDP 4567 is connected but UDP fail.
```

## 关键点总结

### 调用链
1. **CInterNameProxy** - 通过TCP接收远程Name Server的消息
2. **onServiceBroadcast** - 解析地址并验证
3. **broadcastSvcAddrLocal** - 通过IPC转发给本地客户端
4. **CIntraNameProxy** - 本地客户端接收地址
5. **doConnect** - 建立TCP连接到远程服务

### 日志特征
- **连接成功**：打印 "Server: xxx is connected"
- **连接失败**：打印 "fail to connect TCP"
- **服务下线**：打印 "Client xxx is disconnected"
- **大部分中间步骤无日志**，只在最终连接阶段才有日志输出

### 数据转换
- 订阅类型：`INTER_NORMAL` → `INTRA_NORMAL`
- 通信协议：TCP（Name Server间） → IPC（本地） → TCP（连接服务）
- 地址格式：保持 `tcp://IP:PORT` 格式不变

这个流程确保了跨主机服务发现的透明性和可靠性。