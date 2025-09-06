# Name Server 和 Host Server 开机启动流程总结

## 一、系统架构概述

FDBus (Fast Distributed Bus) 是一个分布式通信框架，其中 Name Server 和 Host Server 是核心组件：

- **Name Server (NS)**: 服务命名服务器，负责服务注册、发现和地址分配
- **Host Server (HS)**: 主机服务器，负责多主机/多域环境下的主机管理和服务路由

## 二、Name Server 启动流程

### 1. 启动入口 (main_ns.cpp)

```
启动命令示例：
name-server -u tcp://192.168.1.1:60000 -n android -d 0:0 -x 60000:65000
```

主要参数：
- `-u`: Host Server URL 列表
- `-n`: 主机名称
- `-i`: 监听的网络接口列表
- `-x`: 端口范围（最小:最大）
- `-d`: 看门狗参数（间隔:重试次数）
- `-s`: 静态主机URL
- `-l`: 自身可导出级别
- `-o`: 域内最大可导出级别
- `-p`: 向上游最小可导出级别

### 2. 初始化阶段

#### 2.1 CNameServer 构造函数
```cpp
CNameServer(host_name, self_exportable_level, max_domain_exportable_level, min_upstream_exportable_level)
```

主要初始化工作：
- 设置导出级别（默认 FDB_EXPORTABLE_DOMAIN）
- 获取主机名（如果未指定则使用系统主机名）
- 导入安全配置
- 注册消息处理回调：
  - REQ_ALLOC_SERVICE_ADDRESS: 分配服务地址请求
  - REQ_REGISTER_SERVICE: 注册服务请求
  - REQ_UNREGISTER_SERVICE: 注销服务请求
  - REQ_QUERY_SERVICE: 查询服务请求
  - REQ_QUERY_SERVICE_INTER_MACHINE: 跨机器查询服务
  - REQ_QUERY_HOST_LOCAL: 查询本地主机
- 创建发布者对象：
  - IntraNormalPublisher: 域内普通服务发布
  - IntraMonitorPublisher: 域内监控服务发布
  - InterNormalPublisher: 跨域普通服务发布
  - InterMonitorPublisher: 跨域监控服务发布
  - BindAddrPublisher: 绑定地址发布
- 设置地址分配器（LocalAllocator/IPCAllocator）

### 3. 上线阶段 (online方法)

#### 3.1 导入网络接口
- 解析网络接口参数（IP地址+导出级别+接口ID）
- 支持IP地址或接口名称

#### 3.2 创建TCP分配器
- 如果设置了强制绑定地址，创建TCP地址分配器

#### 3.3 注册自身服务
- 在注册表中创建Name Server自身的条目
- 分配令牌（Token）
- 设置服务属性（SID、端点名、安全设置等）
- 添加服务地址

#### 3.4 绑定监听地址
```cpp
bindNsAddress(reg_entry.mAddrTbl)
```
- 遍历地址表，绑定所有待绑定的地址
- 记录UDP端口信息
- 标记绑定状态

#### 3.5 连接到Host Server
如果配置了Host Server URL：
- 创建 CIntraHostProxy 代理对象
- 建立与Host Server的连接

#### 3.6 连接静态主机
如果配置了静态主机URL：
- 创建 CInterNameProxy 代理对象
- 直接连接到其他Name Server（无需Host Server中转）

### 4. 启动运行
```cpp
FDB_CONTEXT->start(FDB_WORKER_EXE_IN_PLACE)
```
启动FDBus上下文，进入事件循环

## 三、Host Server 启动流程

### 1. 启动入口 (main_hs.cpp)

```
启动命令示例：
host_server -n domain_name -u upstream_url1,upstream_url2
```

主要参数：
- `-n`: 域名称
- `-u`: 上游Host Server URL列表
- `-l`: 自身可导出级别
- `-o`: 域内最大可导出级别
- `-p`: 向上游最小可导出级别

### 2. 初始化阶段

#### 2.1 CHostServer 构造函数
```cpp
CHostServer(domain_name, self_exportable_level, max_domain_exportable_level, min_upstream_exportable_level)
```

主要初始化工作：
- 设置域名（默认"Unknown-Domain"）
- 设置导出级别（默认 FDB_EXPORTABLE_SITE）
- 导入安全配置
- 注册消息处理回调：
  - REQ_REGISTER_LOCAL_HOST: 注册本地主机
  - REQ_REGISTER_EXTERNAL_DOMAIN: 注册外部域
  - REQ_UNREGISTER_LOCAL_HOST: 注销本地主机
  - REQ_QUERY_HOST: 查询主机
  - REQ_HEARTBEAT_OK: 心跳确认
  - REQ_LOCAL_HOST_READY: 本地主机就绪
  - REQ_EXTERNAL_HOST_READY: 外部主机就绪
  - REQ_POST_LOCAL_SVC_ADDRESS: 发布本地服务地址
  - REQ_POST_DOMAIN_SVC_ADDRESS: 发布域服务地址
  - REQ_HS_QUERY_EXPORTABLE_SERVICE: 查询可导出服务
- 注册订阅回调：
  - NTF_HOST_ONLINE: 主机上线通知
  - NTF_EXPORT_SVC_ADDRESS_INTERNAL: 内部服务地址导出
  - NTF_EXPORT_SVC_ADDRESS_EXTERNAL: 外部服务地址导出
- 创建心跳定时器

### 3. 上线阶段 (online方法)

#### 3.1 绑定服务地址
```cpp
bind("svc://host_server")
```
绑定Host Server服务地址，等待Name Server连接

#### 3.2 连接上游Host Server
如果配置了上游Host Server：
- 创建 CInterHostProxy 代理对象
- 连接到上游Host Server

### 4. 运行时处理

#### 4.1 本地主机注册流程
当Name Server连接并注册时：
1. 接收注册请求（REQ_REGISTER_LOCAL_HOST）
2. 验证主机凭证
3. 分配令牌
4. 回复注册确认
5. 启动心跳定时器（如果是第一个连接）

#### 4.2 外部域注册流程
当其他域的Host Server连接时：
1. 接收注册请求（REQ_REGISTER_EXTERNAL_DOMAIN）
2. 验证域凭证
3. 分配令牌
4. 回复注册确认

#### 4.3 服务地址管理
- 维护三个服务表：
  - 本地表（Local Table）：本域内Name Server的服务
  - 下游表（Downstream Table）：下游Host Server的服务
  - 上游表（Upstream Table）：上游Host Server的服务
- 根据导出级别过滤和转发服务信息

## 四、关键数据结构

### 1. Name Server 核心数据结构

- **CSvcRegistryEntry**: 服务注册条目
  - 服务名称、实例ID
  - 地址表（tAddressDescTbl）
  - 令牌列表
  - 导出级别

- **CFdbAddressDesc**: 地址描述符
  - Socket地址信息
  - 状态（空闲/待定/已绑定）
  - UDP端口
  - 重绑定计数

- **CAddressAllocator**: 地址分配器
  - 接口IP
  - 端口范围
  - 导出级别
  - 接口ID

### 2. Host Server 核心数据结构

- **CLocalHostInfo**: 本地主机信息
  - 主机名、IP地址
  - 地址表
  - Name Server URL
  - 心跳计数
  - 授权状态
  - 服务地址表

- **CExternalDomainInfo**: 外部域信息
  - 域名
  - 心跳计数
  - 授权状态
  - 服务地址表

## 五、服务发现机制

### 1. 服务注册流程
1. 服务进程向Name Server发送注册请求
2. Name Server分配服务地址（根据配置的端口范围）
3. Name Server更新本地注册表
4. Name Server通知订阅者（服务上线）
5. 如果服务可导出，通知Host Server

### 2. 服务查询流程
1. 客户端向Name Server查询服务
2. Name Server先查本地注册表
3. 如果本地没有，向其他Name Server查询（通过Host Server或静态连接）
4. 返回服务地址列表给客户端

### 3. 跨域服务发现
1. Name Server将可导出服务报告给Host Server
2. Host Server根据导出级别过滤服务
3. Host Server向上游/下游传播服务信息
4. 实现跨域的服务发现

## 六、高可用性设计

### 1. 心跳机制
- Host Server定期向所有连接的Name Server发送心跳
- 检测连接状态，处理断线重连

### 2. 看门狗（Watchdog）
- Name Server支持看门狗功能
- 监控客户端进程状态
- 异常时广播通知

### 3. 多地址绑定
- 支持同时绑定多个网络接口
- 支持不同的导出级别配置
- 提供冗余和负载均衡

## 七、安全机制

### 1. 令牌认证
- Host Server为每个连接分配唯一令牌
- 用于后续通信的身份验证

### 2. 凭证验证
- 支持主机/域级别的凭证配置
- 注册时验证凭证
- 拒绝未授权的连接

### 3. TLS/SSL支持
- 支持安全和普通TCP连接
- 可配置是否启用安全连接

## 八、启动顺序建议

1. **先启动Host Server**（如果使用多主机架构）
   - Host Server需要先就绪，等待Name Server连接

2. **再启动Name Server**
   - Name Server启动后自动连接到Host Server
   - 开始提供服务注册和发现功能

3. **最后启动应用服务**
   - 应用服务向Name Server注册
   - 客户端通过Name Server发现服务

## 九、Android 系统集成

从 `fdbus-name-server.rc` 文件可以看出，在Android系统中：

1. **服务类别**: `class early_hal`
   - Name Server作为早期HAL服务启动
   - 在系统启动的较早阶段运行

2. **运行用户**: `system`
   - 以system用户权限运行
   - 具有必要的系统访问权限

3. **启动时机**: `on post-fs-data`
   - 在文件系统数据分区准备好后启动
   - 确保有可写的数据目录

4. **默认配置**:
   - Host Server地址: tcp://192.168.1.1:60000
   - 主机名: android
   - 端口范围: 60000-65000
   - 看门狗: 启用（0:0表示使用默认值）

这种设计确保了FDBus基础设施在Android系统启动早期就绪，为后续的系统服务和应用提供通信支持。