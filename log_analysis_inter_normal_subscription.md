# 日志分析：跨主机客户端订阅请求

## 日志内容
```
行 15767: onServiceOnlineReg:1422
行 15768: broadServiceAddress:1355  
行 15769: broadcastSvcAddrRemote:586
行 15770: CNameServer: Registry of ivi_server is received from remote.
```

## 分析结论

**是的，这是收到了跨主机（远程）客户端的订阅请求消息。**

## 详细解释

### 1. 调用链分析

从日志可以看到完整的调用链：
```
onServiceOnlineReg (行1422)
  ↓
broadServiceAddress (行1355)  
  ↓
broadcastSvcAddrRemote (行586)
  ↓
打印日志 "Registry of ivi_server is received from remote"
```

### 2. 关键代码分析

在 `onServiceOnlineReg` 函数中（行1411-1414）：
```cpp
if ((subscribe_type == INTER_NORMAL) || (subscribe_type == INTER_MONITOR))
{
    LOG_I("CNameServer: Registry of %s is received from remote.\n",
            svc_name[0] == '\0' ? "all services" : svc_name);
}
```

这个日志只有在订阅类型为 `INTER_NORMAL` 或 `INTER_MONITOR` 时才会打印。

### 3. 订阅类型判断

在 `broadServiceAddress` 函数中（行1379-1381）：
```cpp
else if (subscribe_type == INTER_NORMAL)
{
    broadcastSvcAddrRemote(reg_entry.mTokens, addr_list, msg);
}
```

调用 `broadcastSvcAddrRemote` 说明订阅类型是 `INTER_NORMAL`。

### 4. 远程订阅来源

`INTER_NORMAL` 类型的订阅来自 `CInterNameProxy`，它代表远程主机的 Name Server 代理：

- **CInterNameProxy** 是远程 Name Server 的客户端代理
- 它通过 `mInterNormalSubscriber` 订阅服务状态
- 当远程主机的客户端需要连接本地服务时，远程 Name Server 会通过 CInterNameProxy 订阅本地服务

### 5. 具体场景

根据日志，实际发生的场景是：

1. **远程主机的客户端** 想要连接到本地主机的 `ivi_server` 服务
2. **远程 Name Server** 通过 CInterNameProxy 向本地 Name Server 订阅 `ivi_server` 的状态
3. **本地 Name Server** 收到订阅请求，触发 `onServiceOnlineReg`
4. 查找本地是否有 `ivi_server` 服务
5. 如果有，通过 `broadcastSvcAddrRemote` 将服务地址发送给远程订阅者
6. 打印日志表示收到了远程的服务订阅请求

## 网络拓扑示意

```
远程主机                           本地主机
--------                          --------
Client                            ivi_server
  ↓                                  ↓
Remote Name Server  <---订阅--->  Local Name Server
       ↓                              ↑
  CInterNameProxy ----INTER_NORMAL----↑
                     (订阅请求)
```

## 关键点总结

1. **订阅类型**：`INTER_NORMAL` - 跨主机普通服务订阅
2. **订阅目标**：`ivi_server` 服务
3. **订阅方向**：从远程主机到本地主机
4. **处理方式**：广播服务的 TCP 地址给远程订阅者
5. **安全控制**：通过 token 机制控制访问权限

这个日志序列清楚地表明：本地 Name Server 收到了来自远程主机的客户端（通过远程 Name Server 代理）对 `ivi_server` 服务的订阅请求。