# UE C++ 反射系统与蓝图调用机制

## 一、C++ 如何暴露接口到反射系统

### 1.1 开发者需要做的事

在头文件中添加三个宏：

```cpp
UCLASS()                                          // 类必须标记 UCLASS
class QUESTSYSTEM_API AQuestTestActor : public AActor
{
    GENERATED_BODY()                              // 必须有，展开为反射注册代码

    UFUNCTION(BlueprintCallable, Category = "Quest|Test")  // 声明为蓝图可调用
    void AddProgress();
};
```

头文件中必须包含 `.generated.h`，且放在最后一个 `#include` 位置：

```cpp
#include "QuestBase.h"                    // 自定义头文件
#include "QuestTestActor.generated.h"     // 必须最后一个
```

### 1.2 UFUNCTION 常用说明符

| 说明符 | 含义 |
|--------|------|
| `BlueprintCallable` | 蓝图可以调用，C++ 也可以调用 |
| `BlueprintImplementableEvent` | C++ 声明，蓝图实现（C++ 不写函数体） |
| `BlueprintNativeEvent` | C++ 提供默认实现，蓝图可覆盖 |
| `BlueprintPure` | 纯函数，无副作用，类似 getter |
| `BlueprintAuthorityOnly` | 仅在服务器端可调用 |

### 1.3 UHT 代码生成（编译时）

Unreal Header Tool (UHT) 在编译前扫描头文件，为每个 `UFUNCTION` 生成两个文件：

#### `.generated.h` — 注入到类中

`GENERATED_BODY()` 宏展开后，向类中注入：

```cpp
// 每个UFUNCTION对应一个 static exec 桥接函数声明
static void execAddProgress(UObject* Context, FFrame& Stack, RESULT_DECL);
static void execCompleteCurrentQuest(UObject* Context, FFrame& Stack, RESULT_DECL);
static void execCreateAndAddQuest(UObject* Context, FFrame& Stack, RESULT_DECL);
// ...
```

#### `.gen.cpp` — 反射数据与桥接函数

每个 `UFUNCTION` 生成三样东西：

**① 元数据结构体** — 记录函数名、Category 等：

```cpp
struct Z_Construct_UFunction_AQuestTestActor_AddProgress_Statics
{
    static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
        { "Category", "Quest|Test" },
        { "ModuleRelativePath", "Public/QuestTestActor.h" },
    };
    static const UECodeGen_Private::FFunctionParams FuncParams;
};
```

**② UFunction 构造函数** — 运行时创建反射对象：

```cpp
UFunction* Z_Construct_UFunction_AQuestTestActor_AddProgress()
{
    static UFunction* ReturnFunction = nullptr;
    if (!ReturnFunction)
    {
        UECodeGen_Private::ConstructUFunction(&ReturnFunction, ...FuncParams);
    }
    return ReturnFunction;
}
```

**③ exec 桥接函数** — 蓝图虚拟机到 C++ 的跳板：

```cpp
// 宏展开前
DEFINE_FUNCTION(AQuestTestActor::execAddProgress)
{
    P_FINISH;
    P_NATIVE_BEGIN;
    P_THIS->AddProgress();
    P_NATIVE_END;
}

// 宏展开后
void AQuestTestActor::execAddProgress(UObject* Context, FFrame& Stack, void*const RESULT_PARAM)
{
    Stack.Code += !!Stack.Code;                              // P_FINISH
    { SCOPED_SCRIPT_NATIVE_TIMER(ScopedNativeCallTimer);     // P_NATIVE_BEGIN
    ((ThisClass*)Context)->AddProgress();                     // P_THIS->AddProgress()
    }                                                         // P_NATIVE_END
}
```

#### exec 桥接函数的关键设计

- **统一签名**：所有 exec 函数签名都是 `void (UObject*, FFrame&, void*)`，这样才能放入同一个函数指针数组
- **static 成员函数**：`DECLARE_FUNCTION` 宏包含 `static` 关键字，因为 static 函数指针类型是纯 C 函数指针，可以统一存储
- **手动传递 this**：通过 `Context` 参数接收对象指针，`P_THIS` 宏将其强转为实际类型后调用成员函数
- **参数编组**：有参数的函数从 `FFrame& Stack` 中手动解包，如 `P_GET_PROPERTY(FIntProperty, Z_Param_Amount)`

#### 函数指针注册表

```cpp
static constexpr UE::CodeGen::FClassNativeFunction Funcs[] = {
    { "AddProgress",          &AQuestTestActor::execAddProgress },
    { "CompleteCurrentQuest", &AQuestTestActor::execCompleteCurrentQuest },
    { "CreateAndAddQuest",    &AQuestTestActor::execCreateAndAddQuest },
    { "FailCurrentQuest",     &AQuestTestActor::execFailCurrentQuest },
    { "PrintAllQuests",       &AQuestTestActor::execPrintAllQuests },
};
```

这些函数指针在**链接时**由链接器在同一个 DLL 内解析为具体地址偏移，不需要 dllexport。

### 1.4 反射注册（DLL 加载时）

#### 全局对象自动构造

```cpp
// .gen.cpp 中的全局静态对象
static FRegisterCompiledInInfo Z_CompiledInDeferFile_..._652561753{
    TEXT("/Script/QuestSystem"),
    ClassInfo,  // 包含 Z_Construct_UClass_AQuestTestActor 等函数指针
    ...
};
```

DLL 被 LoadLibrary 加载时，C++ 运行时自动初始化全局/静态对象。`FRegisterCompiledInInfo` 的构造函数调用 `RegisterCompiledInInfo()`，把类信息注册到 CoreUObject 的延迟注册表 `FClassDeferredRegistry` 中。

此时只是"登记"，还没有创建 `UClass` 和 `UFunction` 对象。

#### 引擎初始化时构造反射对象

引擎调用 `UClassRegisterAllCompiledInClasses()`，遍历注册表：

1. 调用 `Z_Construct_UClass_AQuestTestActor()` 构造 `UClass` 对象
2. 为每个 `UFUNCTION` 调用 `Z_Construct_UFunction_...()` 构造 `UFunction` 对象
3. 为每个 `UPROPERTY` 创建 `FProperty` 对象
4. 调用 `StaticRegisterNativesAQuestTestActor()` 注册 exec 指针：

```cpp
void AQuestTestActor::StaticRegisterNativesAQuestTestActor()
{
    UClass* Class = AQuestTestActor::StaticClass();
    FNativeFunctionRegistrar::RegisterFunctions(Class, MakeConstArrayView(Funcs));
    // 把 Funcs[]（含已解析的函数指针）传给 CoreUObject
}
```

注册完成后，`UClass` 内部建立映射：`"AddProgress" → UFunction(exec指针)`。

### 1.5 跨 DLL 交互机制

反射系统位于 `UnrealEditor-CoreUObject.dll`，与插件 DLL 是分离的。跨 DLL 交互有两种方向：

| 方向 | 调用什么 | 机制 |
|------|---------|------|
| 插件 → CoreUObject | `RegisterCompiledInInfo()`、`RegisterFunctions()` | CoreUObject **dllexport** 了这些函数，插件 **dllimport** 调用 |
| CoreUObject → 插件 | `execAddProgress()` 等函数指针 | 插件通过 `RegisterFunctions` **主动递交**了指针，CoreUObject 直接 `call` 地址 |

CoreUObject 永远不会主动去插件 DLL 里查找函数。所有信息都是插件 DLL 加载时主动注册上去的。

**函数指针传递 vs DLL 符号导出**：

- DLL 符号导出（dllexport）：调用者不知道地址，通过 OS 加载器按名字在导出表中查找
- 函数指针传递：DLL 自己知道地址（链接时确定），主动把地址作为参数传给另一个 DLL，对方存下后直接 call

CPU 执行 `call` 指令只需要一个地址，不关心这个地址是否在 DLL 导出表中注册。因此 `execAddProgress` 等函数不需要 dllexport，它们通过函数指针主动递交的方式暴露给 CoreUObject。

`_API` 宏（如 `QUESTSYSTEM_API`）的 dllexport 是给**其他模块的 C++ 代码跨 DLL 调用**使用的（如 QuestEditor.dll 调用 QuestSystem.dll 的函数），与蓝图调用无关。

---

## 二、蓝图如何找到 C++ 接口并调用

### 2.1 蓝图调用的完整链路

```
蓝图节点 "AddProgress"
    │
    │  1. 通过对象获取 UClass
    ▼
UClass* Class = QuestTestActor->GetClass()
    │
    │  2. 通过函数名查找 UFunction
    ▼
UFunction* Func = Class->FindFunction(FName("AddProgress"))
    │  ← 在 UClass 内部的 TMap<FName, UFunction*> 中查找
    │  ← 不是查 DLL 导出符号表
    ▼
Func->Invoke(QuestTestActor, Params)
    │  ← 调用已注册的 exec 函数指针
    ▼
AQuestTestActor::execAddProgress(Context, Stack, RESULT_PARAM)
    │
    │  3. exec 内部调用真正的 C++ 成员函数
    ▼
((AQuestTestActor*)Context)->AddProgress()   ← 你写的代码
```

### 2.2 蓝图虚拟机的参数传递

对于有参数的函数（如 `UQuestBase::UpdateProgress(int32 Amount)`），exec 函数从蓝图虚拟机的栈帧中解包参数：

```cpp
DEFINE_FUNCTION(UQuestBase::execUpdateProgress)
{
    P_GET_PROPERTY(FIntProperty, Z_Param_Amount);  // 从 Stack 中弹出 int32
    P_FINISH;
    P_NATIVE_BEGIN;
    P_THIS->UpdateProgress(Z_Param_Amount);         // 传给真正的 C++ 函数
    P_NATIVE_END;
}
```

参数不是通过 C++ 函数参数列表传递的，而是从 `FFrame& Stack` 中手动解包。返回值则写入 `RESULT_PARAM` 缓冲区。

---

## 三、完整流程图

```
编译时
═══════════════════════════════════════════════════════════════

  开发者写的代码                    UHT 生成的代码
  ─────────────                    ────────────────

  UFUNCTION(BlueprintCallable)     
  void AddProgress();              
         │                         
         ▼ UHT 扫描头文件          
                                    .generated.h
                                    ├─ static void execAddProgress(UObject*, FFrame&, void*)
                                    │  注入到类中（GENERATED_BODY 展开）
                                    │
                                    .gen.cpp
                                    ├─ execAddProgress() 定义
                                    │  { P_THIS->AddProgress(); }
                                    │
                                    ├─ Z_Construct_UFunction_...AddProgress()
                                    │  构造 UFunction 反射对象
                                    │
                                    ├─ Funcs[] = { "AddProgress", &execAddProgress }
                                    │  函数指针注册表（链接时填入地址）
                                    │
                                    └─ FRegisterCompiledInInfo 全局对象
                                       DLL加载时自动注册

         │                         
         ▼ 编译 & 链接             
                                   
  QuestTestActor.obj  +  QuestTestActor.gen.obj
         │                         
         ▼ 链接器合并              
                                   
  UnrealEditor-QuestSystem.dll
  Funcs[] 中函数指针已解析为内存地址


DLL 加载时
═══════════════════════════════════════════════════════════════

  UnrealEditor-CoreUObject.dll          UnrealEditor-QuestSystem.dll
  （反射系统，先加载）                    （插件，后加载）
  ┌──────────────────────┐             ┌──────────────────────┐
  │                      │             │                      │
  │  RegisterCompiledIn  │ ←──调用──── │  FRegisterCompiledIn │
  │  Info()  [导出]      │             │  Info 全局对象构造    │
  │                      │             │                      │
  │  延迟注册表           │             │  类信息存入注册表      │
  │  FClassDeferred      │             │                      │
  │  Registry            │             │                      │
  └──────────────────────┘             └──────────────────────┘


引擎初始化时
═══════════════════════════════════════════════════════════════

  CoreUObject                           QuestSystem
  ┌──────────────────────┐             ┌──────────────────────┐
  │                      │             │                      │
  │  UClassRegisterAll   │             │                      │
  │  CompiledInClasses() │             │                      │
  │         │            │             │                      │
  │         ▼            │ ←──调用──── │  Z_Construct_UClass  │
  │  构造 UClass 对象     │             │  AQuestTestActor()   │
  │         │            │             │                      │
  │         ▼            │ ←──调用──── │  StaticRegisterNative│
  │  RegisterFunctions() │             │  sAQuestTestActor()  │
  │  [导出]              │             │  传入 Funcs[] 数组    │
  │         │            │             │                      │
  │         ▼            │             │                      │
  │  UClass 内部建立映射  │             │                      │
  │  "AddProgress" →     │             │                      │
  │  UFunction(exec指针) │             │                      │
  └──────────────────────┘             └──────────────────────┘


运行时 — 蓝图调用
═══════════════════════════════════════════════════════════════

  蓝图虚拟机                            C++ 代码
  ┌──────────────────────┐             ┌──────────────────────┐
  │                      │             │                      │
  │  蓝图节点 AddProgress │             │                      │
  │         │            │             │                      │
  │         ▼            │             │                      │
  │  FindFunction        │             │                      │
  │  ("AddProgress")     │             │                      │
  │         │            │             │                      │
  │         ▼            │             │                      │
  │  UFunction::Invoke() │             │                      │
  │  call exec指针 ──────│────────────→│  execAddProgress()   │
  │                      │             │    P_THIS→AddProgress│
  │                      │             │         │            │
  │                      │             │         ▼            │
  │                      │             │  你写的 C++ 代码执行  │
  └──────────────────────┘             └──────────────────────┘
```

---

## 四、关键概念总结

| 概念 | 说明 |
|------|------|
| UHT | Unreal Header Tool，编译前扫描宏标记，生成反射胶水代码 |
| exec 桥接函数 | 统一签名的 static 成员函数，从蓝图虚拟机跳转到 C++ 成员函数 |
| 函数指针注册 | 插件 DLL 主动把 exec 指针递交给 CoreUObject，而非 CoreUObject 查找导出表 |
| UClass / UFunction | 运行时反射对象，存储类和函数的元数据 |
| FFrame | 蓝图虚拟机的栈帧，用于参数编组 |
| `_API` 宏 | dllexport，给 C++ 跨 DLL 调用使用，与蓝图调用无关 |
| `.generated.h` | 必须是最后一个 include，UHT 向类中注入声明 |
| `.gen.cpp` | 反射数据定义 + exec 函数定义 + 全局注册对象 |
