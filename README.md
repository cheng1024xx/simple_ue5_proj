# QuestSystem - Unreal Engine 任务系统插件

基于 Unreal Engine 的任务管理系统插件，采用 Subsystem 架构设计，支持 C++ 和蓝图双端调用。

## 项目结构

```
QuestSystem/
├── QuestSystem.uplugin                  # 插件描述文件
├── Resources/
│   └── Icon128.png                      # 插件图标
├── Source/
│   ├── QuestSystem/                     # 运行时模块
│   │   ├── Public/
│   │   │   ├── QuestBase.h             # 任务数据模型
│   │   │   ├── QuestManager.h          # 任务管理器（Subsystem）
│   │   │   ├── QuestSystem.h           # 模块入口
│   │   │   └── QuestTestActor.h        # 测试用 Actor
│   │   ├── Private/
│   │   │   ├── QuestBase.cpp
│   │   │   ├── QuestManager.cpp
│   │   │   ├── QuestSystem.cpp
│   │   │   └── QuestTestActor.cpp
│   │   └── QuestSystem.Build.cs
│   └── QuestEditor/                    # 编辑器模块
│       ├── Public/
│       │   └── QuestEditorModule.h     # 编辑器扩展模块
│       ├── Private/
│       │   └── QuestEditorModule.cpp
│       └── QuestEditor.Build.cs
└── Intermediate/                        # UHT 生成代码（自动生成）
    └── Build/.../UHT/
        ├── QuestBase.gen.cpp / .generated.h
        ├── QuestManager.gen.cpp / .generated.h
        └── QuestTestActor.gen.cpp / .generated.h
```

## 模块划分

| 模块 | 类型 | 加载阶段 | 说明 |
|------|------|---------|------|
| QuestSystem | Runtime | Default | 核心逻辑，随游戏打包发布 |
| QuestEditor | Editor | Default | 编辑器扩展，仅编辑器环境下加载 |

## 核心类

### EQuestStatus - 任务状态枚举

```
Inactive → Active → Completed
                 → Failed
```

| 状态 | 含义 |
|------|------|
| Inactive | 已创建但未激活 |
| Active | 正在进行中 |
| Completed | 已完成（进度达标自动触发） |
| Failed | 已失败 |

### UQuestBase - 任务数据模型

继承自 `UObject`，标记为 `Blueprintable`，可在蓝图中创建子类。

**属性：**

| 属性 | 类型 | 访问级别 | 说明 |
|------|------|---------|------|
| QuestName | FString | EditAnywhere, BlueprintReadWrite | 任务名称 |
| QuestDescription | FString | EditAnywhere, BlueprintReadWrite | 任务描述 |
| RewardXP | int32 | EditAnywhere, BlueprintReadWrite | 完成奖励经验值，默认 100 |
| Status | EQuestStatus | BlueprintReadOnly | 当前状态，默认 Inactive |
| CurrentProgress | int32 | BlueprintReadOnly | 当前进度，默认 0 |
| RequiredProgress | int32 | EditAnywhere, BlueprintReadWrite | 目标进度，默认 1 |

**方法：**

| 方法 | 说明 |
|------|------|
| `Activate()` | 激活任务（Inactive → Active），重置进度为 0 |
| `UpdateProgress(int32 Amount)` | 增加进度，若达到 RequiredProgress 自动调用 Complete() |
| `Complete()` | 完成任务（Active → Completed） |
| `Fail()` | 失败任务（Active → Failed） |

**虚函数（可在子类中重写）：**

| 虚函数 | 触发时机 |
|--------|---------|
| `OnQuestActivated()` | 任务被激活时 |
| `OnQuestCompleted()` | 任务完成时 |
| `OnQuestFailed()` | 任务失败时 |

**状态守卫：** `Activate()` 仅在 Inactive 状态生效，`UpdateProgress()`/`Complete()`/`Fail()` 仅在 Active 状态生效，非法调用会被静默忽略。

### UQuestManager - 任务管理器

继承自 `UGameInstanceSubsystem`，每个 GameInstance 持有一个实例，通过 `GetSubsystem<UQuestManager>()` 获取。

**获取实例：**

```cpp
// C++
UQuestManager* Manager = UQuestManager::Get(WorldContextObject);

// 蓝图
// 调用 Quest Manager > Get 节点，传入 WorldContext
```

**方法：**

| 方法 | 说明 |
|------|------|
| `Get(WorldContextObject)` | 静态方法，获取 QuestManager 实例 |
| `CreateQuest(QuestClass)` | 创建指定类型的任务对象 |
| `AddQuest(Quest)` | 添加任务到管理器并自动激活 |
| `RemoveQuest(Quest)` | 从管理器移除任务 |
| `GetAllActiveQuests()` | 获取所有活跃任务列表 |
| `FindQuestByName(QuestName)` | 按名称查找任务 |

**委托：**

| 委托 | 触发时机 |
|------|---------|
| `OnQuestAdded` | 任务被添加时广播 |
| `OnQuestRemoved` | 任务被移除时广播 |

### AQuestTestActor - 测试用 Actor

用于在编辑器中验证任务系统的功能。拖入场景后点击 Play 即可自动测试。

**属性：**

| 属性 | 说明 |
|------|------|
| bAutoTestOnBeginPlay | 是否在 BeginPlay 时自动执行测试，默认 true |
| QuestClassToSpawn | 要创建的任务类，默认使用 UQuestBase |
| ProgressAmount | 每次增加的进度值，默认 1 |

**方法：**

| 方法 | 说明 |
|------|------|
| `CreateAndAddQuest()` | 创建并添加一个任务 |
| `AddProgress()` | 为当前任务增加进度 |
| `CompleteCurrentQuest()` | 完成当前任务 |
| `FailCurrentQuest()` | 使当前任务失败 |
| `PrintAllQuests()` | 打印所有活跃任务信息到屏幕 |

### FQuestEditorModule - 编辑器扩展模块

在编辑器菜单栏 Window 下添加 "Quest System" 菜单项，点击后在 Output Log 中打印当前活跃任务信息。

## 快速开始

### 1. 编译插件

在 VS2022 中编译项目，或使用 UE 编辑器点击 Compile。

### 2. 创建自定义任务蓝图

1. Content Browser 右键 → Blueprint Class
2. 父类选择 **UQuestBase**（搜索 QuestBase）
3. 命名为 `BP_MyQuest`
4. 在蓝图 Details 面板中设置：
   - Quest Name: "收集 5 个宝石"
   - Quest Description: "在地图中找到并收集 5 个宝石"
   - Required Progress: 5
   - Reward XP: 200

### 3. 在蓝图中使用

```
Event BeginPlay
    │
    ▼
Quest Manager > Get (WorldContext = self)
    │
    ▼
Create Quest (Quest Class = BP_MyQuest)
    │
    ▼
Add Quest (Quest = 返回值)
    │
    ▼
（游戏逻辑中调用 UpdateProgress 增加进度）
```

### 4. 使用测试 Actor 验证

1. 将 `AQuestTestActor` 拖入场景
2. 在 Details 面板中将 `Quest Class To Spawn` 设为你的蓝图任务类
3. 点击 Play
4. 屏幕左上角将显示测试结果

### 5. 验证编辑器菜单

编辑器菜单栏 → Window → Quest System，点击后在 Output Log 中查看活跃任务。

## 架构设计

### Subsystem 单例模式

`UQuestManager` 使用 `UGameInstanceSubsystem` 而非传统 C++ static 单例：

- `GetSubsystem<UQuestManager>()` 是 per-GameInstance 的单例
- PIE（Play In Editor）多窗口场景下每个实例独立隔离
- 生命周期由引擎管理，无需手动 Initialize/Deinitialize
- 避免悬挂指针和逻辑冲突

### 导出宏规则

模块导出宏必须与模块名严格匹配：

- 模块名 `QuestSystem` → 导出宏 `QUESTSYSTEM_API`
- UBT 在 `Definitions.QuestSystem.h` 中自动生成此宏
- 写错（如 `QUESTCORE_API`）会导致链接错误

### 模块依赖关系

```
QuestEditor ──依赖──→ QuestSystem ──依赖──→ CoreUObject
                                          Engine
                                          Core
                                          InputCore
```

QuestEditor 模块通过 `QuestEditor.Build.cs` 的 `PublicDependencyModuleNames` 声明对 QuestSystem 的依赖，可以调用 QuestSystem 的公共接口。

## 技术文档

项目 Docs 目录下包含 UE 反射系统的详细技术文档：

| 文档 | 内容 |
|------|------|
| [UE_CPP_Reflection_And_Blueprint_Call_Mechanism.md](Docs/UE_CPP_Reflection_And_Blueprint_Call_Mechanism.md) | C++ 函数如何通过 UFUNCTION 暴露给蓝图，蓝图调用 C++ 的完整流程 |
| [UE_UPROPERTY_Reflection_Mechanism.md](Docs/UE_UPROPERTY_Reflection_Mechanism.md) | C++ 属性如何通过 UPROPERTY 暴露给蓝图，属性注册与访问机制 |

## 编译产物

| 文件 | 路径 | 说明 |
|------|------|------|
| UnrealEditor-QuestSystem.dll | `Plugins/QuestSystem/Binaries/Win64/` | 运行时模块 DLL |
| UnrealEditor-QuestEditor.dll | `Plugins/QuestSystem/Binaries/Win64/` | 编辑器模块 DLL |
| QuestBase.gen.cpp 等 | `Plugins/QuestSystem/Intermediate/.../UHT/` | UHT 生成的反射代码 |

## 环境要求

- Unreal Engine 5.7
- Visual Studio 2022
- Windows 10/11
