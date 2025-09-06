# FDBus 可导出级别（Exportable Level）设计详解

## 一、概念介绍

FDBus（Fast Distributed Bus）的可导出级别是一个**自定义的分层访问控制机制**，用于控制服务在分布式系统中的可见性和访问范围。这不是一个通用的设计模式，而是 FDBus 特有的实现。

## 二、核心设计理念

### 1. 分层架构

```
                    站点（Site）
                         |
        +----------------+----------------+
        |                                 |
    域A（Domain A）                   域B（Domain B）
        |                                 |
   +----+----+                       +----+----+
   |         |                       |         |
节点1     节点2                    节点3     节点4
```

### 2. 可导出级别定义

```c
#define FDB_EXPORTABLE_NODE_INTERNAL    0      // 节点内部可见
#define FDB_EXPORTABLE_DOMAIN           1      // 域内可见
#define FDB_EXPORTABLE_SITE             9      // 站点内可见
#define FDB_EXPORTABLE_CUSTOM_BEGIN     10     // 自定义级别起始
#define FDB_EXPORTABLE_ANY              INT32_MAX  // 全局可见
```

## 三、实现机制详解

### 1. 三个关键参数

当启动 Host Server 时，有三个重要的参数控制访问级别：

```cpp
CHostServer::CHostServer(const char *domain_name,
                         int32_t self_exportable_level,      // 自身可导出级别
                         int32_t max_domain_exportable_level, // 域内最大可导出级别
                         int32_t min_upstream_exportable_level) // 上游最小可导出级别
{
    // 默认值设置
    mMaxDomainExportableLevel = (max_domain_exportable_level < 0) ?
                                FDB_EXPORTABLE_DOMAIN : max_domain_exportable_level;
    mMinUpstreamExportableLevel = (min_upstream_exportable_level < 0) ?
                                  FDB_EXPORTABLE_DOMAIN : min_upstream_exportable_level;
    setExportableLevel((self_exportable_level < 0) ? 
                       FDB_EXPORTABLE_SITE : self_exportable_level);
}
```

### 2. 级别检查机制

系统通过两种方式检查可导出级别：

#### a. 精确匹配模式（正数）
```cpp
// exportable_level = 1 表示只允许级别为1的服务
if (current_level > desired_level) {
    return false; // 拒绝访问
}
```

#### b. 至少匹配模式（负数）
```cpp
// exportable_level = -1 表示至少需要级别1
if (current_level < abs(desired_level)) {
    return false; // 拒绝访问
}
```

实现代码：
```cpp
bool CSvcAddrUtils::checkExportable(int32_t current, int32_t desired, bool at_least)
{
    if (at_least) {
        // 至少模式：当前级别必须 >= 期望级别
        if (current < desired) {
            return false;
        }
    } else {
        // 精确模式：当前级别必须 <= 期望级别
        if (current > desired) {
            return false;
        }
    }
    return true;
}
```

### 3. 地址分配器的级别设置

不同的网络接口有不同的默认导出级别：

```cpp
// Name Server 初始化
#if defined(__WIN32__) || defined(CONFIG_FORCE_LOCALHOST)
    mLocalAllocator.setInterfaceIp(FDB_LOCAL_HOST);
    mLocalAllocator.setExportableLevel(FDB_EXPORTABLE_NODE_INTERNAL); // 本地回环，仅节点内部
#else
    mIPCAllocator.setExportableLevel(FDB_EXPORTABLE_NODE_INTERNAL);   // IPC，仅节点内部
#endif

// TCP 接口默认设置
allocator.setInterfaceIp(FDB_IP_ALL_INTERFACE);  // 0.0.0.0
allocator.setExportableLevel(FDB_EXPORTABLE_DOMAIN); // 域级别可见
```

## 四、服务发现和路由机制

### 1. 三个服务表

Host Server 维护三个服务表：

```cpp
// 1. 本地服务表 - 本域内的服务
std::map<FdbSessionId_t, CLocalHostInfo> mLocalHostTbl;

// 2. 下游域服务表 - 连接到本 HS 的下游域
std::map<FdbSessionId_t, CExternalDomainInfo> mExternalDomainTbl;

// 3. 上游域服务表 - 本 HS 连接到的上游域
std::map<std::string, CInterHostProxy*> mHostProxyTbl;
```

### 2. 服务广播规则

```cpp
// 向本地主机广播（表2 + 表3）
void CHostServer::buildAddrTblForLocalHost(FdbMsgExportableSvcAddress &svc_address_tbl)
{
    populateUpstreamHost(svc_address_tbl, mMaxDomainExportableLevel);    // 表3
    populateDownstreamHost(svc_address_tbl, mMaxDomainExportableLevel);  // 表2
}

// 向上游主机发送（表1 + 表2）
void CHostServer::buildAddrTblForUpstreamHost(FdbMsgExportableSvcAddress &svc_address_tbl)
{
    populateDownstreamHost(svc_address_tbl, -mMinUpstreamExportableLevel); // 至少模式
    populateLocalHost(svc_address_tbl, -mMinUpstreamExportableLevel);      // 至少模式
}

// 向下游主机广播（表1 + 表2 + 表3）
void CHostServer::buildAddrTblForDownstreamHost(FdbMsgExportableSvcAddress &svc_address_tbl)
{
    populateUpstreamHost(svc_address_tbl, mMaxDomainExportableLevel);    // 表3
    populateDownstreamHost(svc_address_tbl, mMaxDomainExportableLevel);  // 表2
    populateLocalHost(svc_address_tbl, mMaxDomainExportableLevel);       // 表1
}
```

## 五、实际使用示例

### 示例1：默认配置（不带参数启动）

```bash
./host_server
```

效果：
- `self_exportable_level = FDB_EXPORTABLE_SITE (9)` - 在整个站点内可见
- `max_domain_exportable_level = FDB_EXPORTABLE_DOMAIN (1)` - 域内服务最多导出到域级别
- `min_upstream_exportable_level = FDB_EXPORTABLE_DOMAIN (1)` - 只连接域级别或更高的上游

### 示例2：创建隔离的私有域

```bash
./host_server -l 0 -o 0 -p 0
```

参数说明：
- `-l 0`: self_exportable_level = NODE_INTERNAL，自己不对外暴露
- `-o 0`: max_domain_exportable_level = NODE_INTERNAL，域内服务不导出
- `-p 0`: min_upstream_exportable_level = NODE_INTERNAL，不连接上游

效果：完全隔离的私有域

### 示例3：创建公共服务域

```bash
./host_server -l 2147483647 -o 2147483647 -p 1
```

参数说明：
- `-l INT32_MAX`: 全局可见
- `-o INT32_MAX`: 域内服务可以导出到任何级别
- `-p 1`: 只连接域级别或更高的上游

效果：完全开放的公共服务域

## 六、实际应用场景

### 1. 多租户隔离

```cpp
// 租户A的 Host Server
CHostServer tenant_a_hs("TenantA", 
    FDB_EXPORTABLE_DOMAIN,  // 只在租户A域内可见
    FDB_EXPORTABLE_DOMAIN,  // 域内服务不外泄
    FDB_EXPORTABLE_SITE);   // 可以连接到站点级服务

// 租户B的 Host Server  
CHostServer tenant_b_hs("TenantB",
    FDB_EXPORTABLE_DOMAIN,  // 只在租户B域内可见
    FDB_EXPORTABLE_DOMAIN,  // 域内服务不外泄
    FDB_EXPORTABLE_SITE);   // 可以连接到站点级服务

// 公共服务 Host Server
CHostServer public_hs("PublicServices",
    FDB_EXPORTABLE_SITE,    // 站点内所有租户可见
    FDB_EXPORTABLE_SITE,    // 提供站点级服务
    FDB_EXPORTABLE_DOMAIN); // 接受域级别的连接
```

### 2. 分层服务架构

```cpp
// 定义自定义级别
#define FDB_EXPORTABLE_DEPARTMENT  15  // 部门级
#define FDB_EXPORTABLE_COMPANY     20  // 公司级
#define FDB_EXPORTABLE_GROUP       25  // 集团级

// 部门服务器
CHostServer dept_hs("Engineering", 
    FDB_EXPORTABLE_DEPARTMENT,
    FDB_EXPORTABLE_DEPARTMENT,
    FDB_EXPORTABLE_DEPARTMENT);

// 公司服务器
CHostServer company_hs("CompanyHub",
    FDB_EXPORTABLE_COMPANY,
    FDB_EXPORTABLE_COMPANY,
    FDB_EXPORTABLE_DEPARTMENT);  // 可以连接部门级服务

// 集团服务器
CHostServer group_hs("GroupCenter",
    FDB_EXPORTABLE_GROUP,
    FDB_EXPORTABLE_GROUP,
    FDB_EXPORTABLE_COMPANY);     // 可以连接公司级服务
```

### 3. 安全边界控制

```cpp
// DMZ 区域 - 有限暴露
CHostServer dmz_hs("DMZ",
    FDB_EXPORTABLE_DOMAIN,      // 只在DMZ域内可见
    FDB_EXPORTABLE_NODE_INTERNAL, // 域内服务不导出
    FDB_EXPORTABLE_SITE);       // 可以访问内网服务

// 内网核心服务
CHostServer internal_hs("InternalCore",
    FDB_EXPORTABLE_SITE,        // 内网可见
    FDB_EXPORTABLE_SITE,        // 服务可在内网传播
    FDB_EXPORTABLE_DOMAIN);     // 只接受域级别连接

// 公网接入点
CHostServer public_hs("PublicGateway",
    FDB_EXPORTABLE_ANY,         // 全局可见
    FDB_EXPORTABLE_DOMAIN,      // 限制服务导出范围
    FDB_EXPORTABLE_DOMAIN);     // 只连接可信域
```

## 七、关键实现细节

### 1. 服务过滤机制

当服务在不同级别之间传播时，会根据导出级别进行过滤：

```cpp
void CSvcAddrUtils::populateFromHostInfo(
    const tSvcAddrDescTbl &svc_addr_tbl,
    FdbMsgExportableSvcAddress &svc_address_tbl,
    int32_t exportable_level)
{
    for (auto svc_it = svc_addr_tbl.begin(); svc_it != svc_addr_tbl.end(); ++svc_it)
    {
        bool at_least;
        int32_t level;
        if (!getExportable(exportable_level, level, at_least))
        {
            return;
        }
        
        // 遍历服务的所有地址
        for (auto it = exported_addr.mAddrTbl.begin(); it != exported_addr.mAddrTbl.end(); ++it)
        {
            // 检查地址的导出级别是否满足要求
            if (!checkExportable(it->mExportableLevel, level, at_least))
            {
                continue; // 不满足，跳过这个地址
            }
            
            // 满足要求，添加到导出列表
            setAddressItem(*it, export_addr_list.add_address_list());
        }
    }
}
```

### 2. 双向验证

系统在两个方向上进行级别验证：

1. **服务注册时**：检查服务的导出级别是否允许在当前域传播
2. **服务发现时**：检查请求方是否有权限访问该级别的服务

## 八、总结

FDBus 的可导出级别机制提供了：

1. **灵活的访问控制**：通过数字级别精确控制服务可见性
2. **层次化的服务架构**：支持复杂的多层服务拓扑
3. **安全隔离**：不同级别的服务自动隔离
4. **可扩展性**：支持自定义级别，适应特定业务需求
5. **默认安全**：默认配置提供合理的安全边界

这种设计特别适合：
- 多租户系统
- 分层的企业架构
- 需要精细访问控制的分布式系统
- 混合云部署（公有云/私有云/边缘计算）