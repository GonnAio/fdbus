# onServiceOnlineReg 接口调用场景分析

## 概述
`onServiceOnlineReg` 是 FDBus Name Server 中的一个重要接口，用于处理服务上线订阅注册。当有客户端订阅服务上线通知时，该接口会被调用。

## 接口定义
```cpp
void CNameServer::onServiceOnlineReg(
    const char *svc_name,           // 服务名称
    FdbInstanceId_t instance_id,    // 实例ID
    CBaseJob::Ptr &msg_ref,         // 消息引用
    SubscribeType subscribe_type    // 订阅类型
)
```

## 订阅类型 (SubscribeType)
```cpp
enum SubscribeType {
    INTRA_NORMAL = FDB_CUSTOM_OBJECT_BEGIN,  // 本地普通服务订阅
    INTRA_MONITOR,                            // 本地服务监控订阅
    INTER_NORMAL,                             // 跨主机普通服务订阅
    INTER_MONITOR,                            // 跨主机服务监控订阅
    MORE_ADDRESS                              // 更多地址订阅
};
```

## 调用场景

### 1. 客户端连接服务时
当客户端（CBaseClient）需要连接到服务器时，会触发以下流程：

```
客户端调用 connect() 
    ↓
CIntraNameProxy::listenOnService()
    ↓
订阅服务上线通知 (INTRA_NORMAL)
    ↓
ConnectAddrPublisher::onSubscribe()
    ↓
CNameServer::onServiceOnlineReg()
```

**具体场景**：
- 客户端启动并尝试连接到指定的服务
- 客户端需要获取服务的地址信息（TCP/IPC/UDP端口）
- 客户端监听服务的上线状态变化

### 2. 服务注册时的广播
当新服务注册到 Name Server 时：

```
服务端调用 bind()
    ↓
CIntraNameProxy::registerService()
    ↓
Name Server 注册服务
    ↓
广播给所有订阅者
    ↓
触发已有订阅者的 onServiceOnlineReg()
```

**具体场景**：
- 新服务启动并注册到 Name Server
- 服务重启后重新注册
- 服务地址发生变化时的更新通知

### 3. 跨主机服务发现
在分布式环境中，当远程主机的服务信息需要同步时：

```
远程 Name Server 连接
    ↓
CInterNameProxy 订阅服务 (INTER_NORMAL)
    ↓
ConnectAddrPublisher::onSubscribe()
    ↓
CNameServer::onServiceOnlineReg()
```

**具体场景**：
- 多主机环境中的服务发现
- 远程服务的地址信息同步
- 跨网络的服务连接

### 4. 服务监控场景
当需要监控服务状态时：

```
监控客户端订阅 (INTRA_MONITOR/INTER_MONITOR)
    ↓
ConnectAddrPublisher::onSubscribe()
    ↓
CNameServer::onServiceOnlineReg()
```

**具体场景**：
- 系统监控工具监控服务状态
- 服务健康检查
- 服务拓扑图的实时更新

## 接口处理逻辑

当 `onServiceOnlineReg` 被调用时，它会执行以下操作：

1. **查找匹配的服务**
   - 根据 `svc_name` 和 `instance_id` 查找已注册的服务
   - 支持通配符匹配（空字符串表示所有服务）

2. **广播服务地址**
   - 向订阅者发送服务的地址信息
   - 包括 TCP、IPC、UDP 等不同类型的地址

3. **转发到其他 Name Server**
   - 对于跨主机场景，将服务信息转发给其他主机的 Name Server
   - 确保服务信息在整个分布式系统中同步

4. **权限和安全控制**
   - 根据订阅类型和服务的导出级别进行访问控制
   - 区分本地服务和远程服务的访问权限

## 使用示例

### 客户端订阅服务上线通知
```cpp
// 客户端代码
CBaseClient client("client_name");
client.connect("svc://service_name");  // 自动触发服务订阅
```

### 服务端注册服务
```cpp
// 服务端代码
CBaseServer server("server_name");
server.bind("svc://service_name");  // 注册后会通知所有订阅者
```

## 重要说明

1. **订阅机制**：采用发布-订阅模式，支持一对多的通知
2. **实时性**：服务状态变化会立即通知所有订阅者
3. **可靠性**：支持服务重连和地址更新
4. **扩展性**：支持跨主机和分布式部署

## 总结

`onServiceOnlineReg` 是 FDBus 服务发现机制的核心接口，主要在以下场景被调用：
- 客户端连接服务时的地址发现
- 服务注册/注销时的状态广播
- 跨主机服务信息同步
- 服务监控和健康检查

该接口确保了 FDBus 系统中服务的动态发现和连接管理，是实现分布式服务架构的关键组件。