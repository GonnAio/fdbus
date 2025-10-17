# 跨主机服务TCP连接完整流程分析

## 概述
在FDBus分布式系统中，跨主机服务连接经过以下阶段：
1. 远程客户端订阅服务
2. 本地Name Server返回服务地址（IP+端口）
3. 远程Name Server转发地址给客户端
4. 客户端建立TCP连接

## 完整流程

### 第一阶段：远程订阅请求

```
远程主机                              本地主机
--------                             --------
Client                               ivi_server (已注册)
  ↓ connect("svc://ivi_server")          ↑
  ↓                                      ↑
Remote Name Server                   Local Name Server
  ↓                                      ↑
CInterNameProxy ──INTER_NORMAL订阅──→    ↑
                                         ↑
                              onServiceOnlineReg()
```

### 第二阶段：地址信息返回

```cpp
// 1. 本地Name Server处理订阅
onServiceOnlineReg(svc_name="ivi_server", subscribe_type=INTER_NORMAL)
    ↓
// 2. 查找本地服务
findService("ivi_server") // 找到已注册的服务
    ↓
// 3. 广播服务地址
broadServiceAddress(reg_entry, msg, INTER_NORMAL)
    ↓
// 4. 准备TCP地址列表（跨主机只能用TCP，不能用IPC）
populateAddrList(reg_entry.mAddrTbl, addr_list, FDB_SOCKET_TCP)
    ↓
// 5. 发送给远程订阅者
broadcastSvcAddrRemote(tokens, addr_list, msg)
```

### 第三阶段：远程接收和转发

```cpp
// 远程CInterNameProxy接收到地址
CInterNameProxy::onServiceBroadcast(msg, INTER_NORMAL)
    ↓
// 解析地址列表
FdbMsgAddressList msg_addr_list {
    service_name: "ivi_server"
    instance_id: 0
    address_list: [
        {
            tcp_ipc_url: "tcp://192.168.1.100:3456"
            address_type: FDB_SOCKET_TCP
            tcp_ipc_address: "192.168.1.100"
            tcp_port: 3456
            is_secure: false
        }
    ]
    host_name: "local_host"
    token_list: [security_tokens]
}
    ↓
// 转发给本地客户端（INTER_NORMAL → INTRA_NORMAL）
name_server->broadcastSvcAddrLocal(tokens, addr_list)
```

### 第四阶段：客户端建立TCP连接

```cpp
// 客户端收到服务地址广播
CIntraNameProxy::onServiceBroadcast()
    ↓
// 连接到服务器
doConnectToServer(context, ep_id, msg_addr_list)
    ↓
// 对每个匹配的客户端实例
for (each client matching service_name) {
    doConnectToAddress(client, msg_addr_list)
        ↓
    // 实际建立TCP连接
    client->doConnect("tcp://192.168.1.100:3456", "local_host")
        ↓
    // 创建客户端socket
    CClientSocket* sk = new CClientSocket(...)
        ↓
    // 建立TCP连接
    session = sk->connect()  // 底层TCP连接建立
}
```

### 第五阶段：TCP连接建立

```cpp
// CBaseClient::doConnect实现
CClientSocket* CBaseClient::doConnect(const char *url, ...) 
{
    // 1. 解析URL
    parseUrl("tcp://192.168.1.100:3456", addr)
    
    // 2. 创建socket实现
    auto client_imp = CBaseSocketFactory::createClientSocket(addr)
    // 这里会创建CTcpClientSocket
    
    // 3. 创建客户端socket容器
    auto sk = new CClientSocket(this, skid, client_imp, ...)
    
    // 4. 发起TCP连接
    auto session = sk->connect()
    // 底层调用 socket() + connect() 系统调用
    
    // 5. 连接成功，创建会话
    if (session) {
        addConnectedSession(sk, session)
        notifyConnectReady(CONNECTED)
    }
    
    return sk;
}
```

## 数据流转细节

### 1. 地址信息格式
```protobuf
FdbMsgAddressList {
    string service_name        // 服务名称
    uint32 instance_id         // 实例ID
    repeated AddressItem {
        string tcp_ipc_url     // 完整URL："tcp://IP:PORT"
        string tcp_ipc_address // IP地址："192.168.1.100"
        int32 tcp_port         // 端口号：3456
        bool is_secure         // 是否安全连接
        int32 address_type     // FDB_SOCKET_TCP
    }
    string host_name           // 主机名
    repeated string tokens     // 安全令牌
}
```

### 2. 地址过滤规则
- **跨主机只发送TCP地址**：IPC（Unix Domain Socket）地址被过滤
- **安全级别控制**：根据exportable_level决定是否导出
- **令牌过滤**：根据远程主机的安全级别发送相应令牌

### 3. 连接优先级
```cpp
// validateUrl()中的优先级顺序
1. IPC (Unix Domain Socket) - 跨主机不可用
2. localhost (127.0.0.1)     - 跨主机不可用
3. 具体IP地址                 - 跨主机使用
4. 0.0.0.0 (all interfaces)  - 替换为实际IP
```

## 关键函数调用时序

```
时间 →
T1: 远程客户端 connect("svc://ivi_server")
T2: 远程NS → 本地NS: INTER_NORMAL订阅请求
T3: 本地NS: onServiceOnlineReg()
T4: 本地NS: broadcastSvcAddrRemote() 返回 "tcp://192.168.1.100:3456"
T5: 远程NS: onServiceBroadcast() 接收地址
T6: 远程NS → 远程客户端: 转发地址信息
T7: 远程客户端: doConnect("tcp://192.168.1.100:3456")
T8: TCP三次握手建立连接
T9: 连接成功，开始通信
```

## 重要特性

### 1. 自动地址发现
- 客户端只需指定服务名，不需要知道具体地址
- Name Server自动返回可用地址

### 2. 跨网络支持
- 自动过滤不可跨网络的地址（IPC）
- 只返回TCP地址给远程客户端

### 3. 安全控制
- 通过令牌机制控制访问权限
- 支持安全和非安全连接

### 4. 负载均衡潜力
- 可以返回多个地址供选择
- 客户端可以选择最优地址连接

## 总结

**是的，在这个流程之后，对端（远程客户端）就能够收到本机的IP+端口，并开始进行TCP连接。**

具体步骤：
1. 远程收到地址信息（如 "tcp://192.168.1.100:3456"）
2. 解析出IP地址和端口号
3. 创建TCP socket
4. 调用connect()系统调用建立连接
5. 完成TCP三次握手
6. 建立会话，开始FDBus协议通信

这个机制使得FDBus能够透明地处理跨主机服务发现和连接，应用程序只需要知道服务名称，底层的地址解析和TCP连接建立都由框架自动完成。