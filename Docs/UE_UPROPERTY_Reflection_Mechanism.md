# UE UPROPERTY 反射机制：C++ 属性如何暴露给蓝图

## 一、UPROPERTY 与 UFUNCTION 的核心区别

| | UFUNCTION（函数） | UPROPERTY（属性） |
|---|---|---|
| 蓝图访问方式 | 通过 exec 桥接函数指针调用 | 通过内存偏移直接读写 |
| 需要桥接吗 | 需要，C++ 调用约定与蓝图虚拟机不同 | 不需要，属性就是一段内存 |
| 反射对象 | `UFunction`（存储函数指针） | `FProperty`（存储内存偏移） |
| 蓝图读取 | `call 函数指针` | `(uint8*)Obj + Offset → 读内存` |
| 蓝图写入 | 调用 setter 函数 | `(uint8*)Obj + Offset → 写内存` |

## 二、开发者需要做的事

在头文件中给属性添加 `UPROPERTY` 宏：

```cpp
UCLASS()
class QUESTSYSTEM_API AQuestTestActor : public AActor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Test")
    bool bAutoTestOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Test")
    TSubclassOf<UQuestBase> QuestClassToSpawn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Test")
    int32 ProgressAmount = 1;

    UPROPERTY()
    UQuestBase* CurrentQuest;
};
```

### UPROPERTY 常用说明符

| 说明符 | 含义 |
|--------|------|
| `EditAnywhere` | 编辑器和实例中都可编辑 |
| `EditDefaultsOnly` | 仅在 CDO（类默认对象）中可编辑 |
| `EditInstanceOnly` | 仅在实例中可编辑 |
| `BlueprintReadWrite` | 蓝图可读可写 |
| `BlueprintReadOnly` | 蓝图只读 |
| `VisibleAnywhere` | 编辑器中可见但不可编辑 |
| `Category` | 在 Details 面板中的分组 |
| 无说明符 | 仅注册反射信息（用于 GC 等），不暴露给编辑器和蓝图 |

## 三、属性注册流程（从编译时到运行时）

### 4.1 编译时：UHT 生成属性描述数据

UHT 为每个 `UPROPERTY` 生成属性描述数据，类型与 C++ 类型一一对应：

```cpp
// gen.cpp 中生成

// bool 类型 → FBoolPropertyParams
const FBoolPropertyParams NewProp_bAutoTestOnBeginPlay = {
    "bAutoTestOnBeginPlay",                    // 属性名
    nullptr,
    (EPropertyFlags)0x0010000000000005,        // 标志位
    EPropertyGenFlags::Bool | NativeBool,      // 类型标志
    RF_Public|RF_Transient|RF_MarkAsNative,
    nullptr, nullptr,
    1,                                         // ArrayDim
    sizeof(bool),                              // 元素大小
    sizeof(AQuestTestActor),                   // 所在类大小
    &NewProp_bAutoTestOnBeginPlay_SetBit,      // bool 专用 SetBit 函数
    METADATA_PARAMS(...)                       // Category="Quest|Test" 等元数据
};

// int32 类型 → FIntPropertyParams
const FIntPropertyParams NewProp_ProgressAmount = {
    "ProgressAmount",
    nullptr,
    (EPropertyFlags)0x0010000000000005,
    EPropertyGenFlags::Int,
    RF_Public|RF_Transient|RF_MarkAsNative,
    nullptr, nullptr,
    1,
    STRUCT_OFFSET(AQuestTestActor, ProgressAmount),  // ← 关键：内存偏移
    METADATA_PARAMS(...)
};

// TSubclassOf<UQuestBase> → FClassPropertyParams
const FClassPropertyParams NewProp_QuestClassToSpawn = {
    "QuestClassToSpawn",
    nullptr,
    (EPropertyFlags)0x0014000000000005,
    EPropertyGenFlags::Class,
    RF_Public|RF_Transient|RF_MarkAsNative,
    nullptr, nullptr,
    1,
    STRUCT_OFFSET(AQuestTestActor, QuestClassToSpawn),  // ← 内存偏移
    Z_Construct_UClass_UClass_NoRegister,                 // 元类
    Z_Construct_UClass_UQuestBase_NoRegister,             // 限制类
    METADATA_PARAMS(...)
};

// UQuestBase* → FObjectPropertyParams
const FObjectPropertyParams NewProp_CurrentQuest = {
    "CurrentQuest",
    nullptr,
    (EPropertyFlags)0x0040000000000000,         // 无 Edit/Blueprint 标志
    EPropertyGenFlags::Object,
    RF_Public|RF_Transient|RF_MarkAsNative,
    nullptr, nullptr,
    1,
    STRUCT_OFFSET(AQuestTestActor, CurrentQuest),  // ← 内存偏移
    Z_Construct_UClass_UQuestBase_NoRegister,       // 限制类
    METADATA_PARAMS(...)
};
```

#### STRUCT_OFFSET 就是 offsetof

```cpp
#define STRUCT_OFFSET(struc, member)  offsetof(struc, member)
// 等价于
#define STRUCT_OFFSET(struc, member)  __builtin_offsetof(struc, member)
```

`STRUCT_OFFSET(AQuestTestActor, ProgressAmount)` 在编译时求值为一个**常量整数**，表示 `ProgressAmount` 在 `AQuestTestActor` 对象内存布局中的偏移字节数。这是蓝图能直接访问属性的核心——不需要函数调用，只需要知道偏移。

#### 属性指针数组

```cpp
const FPropertyParamsBase* const PropPointers[] = {
    &NewProp_bAutoTestOnBeginPlay,   // → 构造 FBoolProperty
    &NewProp_QuestClassToSpawn,      // → 构造 FClassProperty
    &NewProp_ProgressAmount,         // → 构造 FIntProperty
    &NewProp_CurrentQuest,           // → 构造 FObjectProperty
};
```

#### bool 的 SetBit 辅助函数

```cpp
void NewProp_bAutoTestOnBeginPlay_SetBit(void* Obj)
{
    ((AQuestTestActor*)Obj)->bAutoTestOnBeginPlay = 1;
}
```

bool 需要特殊处理，因为 C++ 中 bool 的内存布局不统一——不同编译器可能用 1 字节、4 字节或位域。`SetBit` 函数确保蓝图写入时用 C++ 的方式正确设置值。

#### ClassParams 携带属性数组

```cpp
const FClassParams ClassParams = {
    &AQuestTestActor::StaticClass,
    "Engine",
    &StaticCppClassTypeInfo,
    DependentSingletons,
    FuncInfo,                                    // ← 函数
    PropPointers,                                // ← 属性
    nullptr,
    UE_ARRAY_COUNT(DependentSingletons),
    UE_ARRAY_COUNT(FuncInfo),
    UE_ARRAY_COUNT(PropPointers),                // ← 4 个属性
    0,
    0x009000A4u,
    METADATA_PARAMS(...)
};
```

### 4.2 DLL 加载时：全局对象注册类信息

与 UFUNCTION 相同，DLL 加载时全局对象 `FRegisterCompiledInInfo` 构造，调用 `RegisterCompiledInInfo()` 把类信息注册到 CoreUObject 的延迟注册表。此时只是"登记"，还没有创建 `FProperty` 对象。

### 4.3 引擎初始化时：构造 FProperty 对象并挂载到 UClass

这是属性注册的核心阶段，分为六个步骤：

#### 第一步：ConstructUClass 入口

```cpp
// UObjectConstructInternal.h
void ConstructUClassHelper(UClass*& OutClass, const FClassParams& Params, ...)
{
    // 注册函数
    NewClass->CreateLinkAndAddChildFunctionsToMap(Params.FunctionLinkArray, Params.NumFunctions);
    
    // 注册属性 ← 属性入口
    ConstructFProperties(NewClass, Params.PropertyArray, Params.NumProperties);
    
    // 链接（建立属性链表、GC 引用等）
    NewClass->StaticLink();
}
```

#### 第二步：ConstructFProperties 遍历属性数组

```cpp
// UObjectGlobals.cpp
void ConstructFProperties(UObject* Outer, const FPropertyParamsBase* const* PropertyArray, int32 NumProperties)
{
    PropertyArray += NumProperties;  // 移到末尾
    while (NumProperties)
    {
        ConstructFProperty(Outer, PropertyArray, NumProperties);  // 逐个构造
    }
}
```

从后往前遍历 `PropPointers[]`，逐个构造 `FProperty`。

#### 第三步：ConstructFProperty 根据类型创建 FProperty 子类

```cpp
// UObjectGlobals.cpp
void ConstructFProperty(FFieldVariant Outer, const FPropertyParamsBase* const*& PropertyArray, int32& NumProperties)
{
    const FPropertyParamsBase* PropBase = *--PropertyArray;  // 取出一个
    
    FProperty* NewProp = nullptr;
    switch (PropBase->Flags & PropertyTypeMask)  // 根据类型标志分发
    {
        case EPropertyGenFlags::Int:
            NewProp = NewFProperty<FIntProperty, FIntPropertyParams>(Outer, *PropBase);
            break;
        case EPropertyGenFlags::Bool:
            NewProp = NewFProperty<FBoolProperty, FBoolPropertyParams>(Outer, *PropBase);
            break;
        case EPropertyGenFlags::Object:
            NewProp = NewFProperty<FObjectProperty, FObjectPropertyParams>(Outer, *PropBase);
            break;
        case EPropertyGenFlags::Class:
            NewProp = NewFProperty<FClassProperty, FClassPropertyParams>(Outer, *PropBase);
            break;
        // ... 所有类型
    }
}
```

类型分发对应表：

```
FBoolPropertyParams    → FBoolProperty
FIntPropertyParams     → FIntProperty
FFloatPropertyParams   → FFloatProperty
FStrPropertyParams     → FStrProperty
FObjectPropertyParams  → FObjectProperty
FClassPropertyParams   → FClassProperty
FArrayPropertyParams   → FArrayProperty
FMapPropertyParams     → FMapProperty
FStructPropertyParams  → FStructProperty
FEnumPropertyParams    → FEnumProperty
```

#### 第四步：NewFProperty 创建 FProperty 对象

```cpp
// UObjectGlobals.cpp
template<typename PropertyType, typename PropertyParamsType>
PropertyType* NewFProperty(FFieldVariant Outer, const FPropertyParamsBase& PropBase)
{
    const PropertyParamsType& Prop = (const PropertyParamsType&)PropBase;
    
    // new 一个 FProperty 子类对象，传入描述数据
    PropertyType* NewProp = new PropertyType(Outer, Prop);
    
    // 设置元数据（Category 等）
    if (Prop.NumMetaData)
    {
        for (auto& MetaData : Prop.MetaDataArray)
        {
            NewProp->SetMetaData(MetaData.NameUTF8, MetaData.ValueUTF8);
        }
    }
    return NewProp;
}
```

#### 第五步：FProperty 构造函数读取偏移和标志

```cpp
// Property.cpp
FProperty::FProperty(FFieldVariant InOwner, const FPropertyParamsBaseWithOffset& Prop, ...)
    : Super(InOwner, UTF8_TO_TCHAR(Prop.NameUTF8), Prop.ObjectFlags)  // 属性名
    , PropertyFlags(Prop.PropertyFlags | AdditionalPropertyFlags)      // 标志位
    , Offset_Internal(0)
{
    this->Offset_Internal = Prop.Offset;  // ← 关键！从 STRUCT_OFFSET 写入偏移
    Init();  // 把自己挂到 UClass 上
}
```

FProperty 的核心字段：

```cpp
class FProperty : public FField
{
    int32 ArrayDim;              // 数组维度
    int32 ElementSize;           // 每个元素的字节大小
    EPropertyFlags PropertyFlags; // 标志位（Edit/Blueprint/Replicated 等）
    int32 Offset_Internal;       // ← 内存偏移！蓝图访问属性的钥匙
    FName RepNotifyFunc;         // RepNotify 回调函数名
};
```

#### 第六步：Init() 把 FProperty 挂到 UClass 的属性链表

```cpp
// Property.cpp
void FProperty::Init()
{
    if (GetOwner<UObject>())
    {
        UField* OwnerField = GetOwnerChecked<UField>();
        OwnerField->AddCppProperty(this);  // ← 挂到 UClass 上
    }
}

// Class.cpp
void UStruct::AddCppProperty(FProperty* Property)
{
    Property->Next = ChildProperties;  // 头插法
    ChildProperties = Property;
}
```

#### 第七步：StaticLink() 建立属性索引

```cpp
// Class.cpp
void UStruct::StaticLink(bool bRelinkExistingProperties)
{
    FNullArchive ArDummy;
    Link(ArDummy, bRelinkExistingProperties);  // 遍历 ChildProperties 链表
    // Link() 内部会：
    // - 建立 PropertyLink 链表（用于序列化）
    // - 建立 DestructorLink 链表（用于析构）
    // - 建立 ReferenceLink 链表（用于 GC 追踪）
    // - 计算属性在类中的偏移验证
}
```

### 4.4 属性注册完整流程图

```
ConstructUClass(ClassParams)
│
│  ClassParams 包含：
│  ├─ FuncInfo[]         → 函数注册
│  ├─ PropPointers[]     → 属性注册
│  │   ├─ &NewProp_bAutoTestOnBeginPlay  (FBoolPropertyParams)
│  │   ├─ &NewProp_QuestClassToSpawn     (FClassPropertyParams)
│  │   ├─ &NewProp_ProgressAmount       (FIntPropertyParams)
│  │   └─ &NewProp_CurrentQuest         (FObjectPropertyParams)
│  └─ NumProperties = 4
│
├─→ CreateLinkAndAddChildFunctionsToMap(FuncInfo)  // 函数注册
│
└─→ ConstructFProperties(Class, PropPointers, 4)   // 属性注册入口
     │
     │  从后往前遍历 PropPointers[]
     │
     ├─→ ConstructFProperty(Class, PropPointers, 4)
     │    │
     │    │  取出 &NewProp_CurrentQuest
     │    │  Flags = EPropertyGenFlags::Object
     │    │
     │    └─→ NewFProperty<FObjectProperty, FObjectPropertyParams>(Class, Prop)
     │         │
     │         └─→ new FObjectProperty(Class, Prop)
     │              │  构造函数中：
     │              │  Offset_Internal = Prop.Offset  (STRUCT_OFFSET 值)
     │              │  PropertyFlags = Prop.PropertyFlags
     │              │  PropertyClass = UQuestBase
     │              │
     │              └─→ Init()
     │                   └─→ Class->AddCppProperty(this)
     │                        │  Property->Next = ChildProperties
     │                        │  ChildProperties = Property
     │                        │  （头插法挂到 UClass 的属性链表）
     │
     ├─→ ConstructFProperty(...)  // ProgressAmount → FIntProperty
     ├─→ ConstructFProperty(...)  // QuestClassToSpawn → FClassProperty
     └─→ ConstructFProperty(...)  // bAutoTestOnBeginPlay → FBoolProperty
     │
     ▼
  UClass 的 ChildProperties 链表：
  bAutoTestOnBeginPlay → QuestClassToSpawn → ProgressAmount → CurrentQuest → nullptr
  （头插法，所以顺序和声明相反）
     │
     ▼
  StaticLink()
  ├─ 建立 PropertyLink 链表（序列化用）
  ├─ 建立 DestructorLink 链表（析构用）
  ├─ 建立 ReferenceLink 链表（GC 追踪用）
  └─ 验证偏移、计算类大小等
```

### 4.5 属性注册 vs 函数注册对比

| | 属性注册 | 函数注册 |
|---|---|---|
| 入口 | `ConstructFProperties()` | `CreateLinkAndAddChildFunctionsToMap()` |
| 数据来源 | `PropPointers[]`（`FPropertyParamsBase*` 数组） | `FuncInfo[]`（`FClassFunctionLinkInfo` 数组） |
| 创建对象 | `new FIntProperty(Owner, PropParams)` | `new UFunction(...)` |
| 核心数据 | `Offset_Internal` = `STRUCT_OFFSET` 值 | `Func` = exec 函数指针 |
| 挂载方式 | `AddCppProperty()` → `ChildProperties` 链表 | `AddFunctionToFunctionMap()` → `TMap<FName, UFunction*>` |
| 链接 | `StaticLink()` 建立各种链表 | 函数注册时直接加入 Map |
| 额外注册 | 不需要 `RegisterFunctions` | 需要 `StaticRegisterNatives` 注册 exec 指针 |

**关键区别**：属性不需要像函数那样单独注册 exec 指针，因为属性是通过偏移直接访问内存的，不需要桥接函数。`FProperty` 对象在构造时通过 `Init()` → `AddCppProperty()` 就直接挂到了 `UClass` 上，后续 `StaticLink()` 建立好索引链表，蓝图就可以通过 `FindPropertyByName` 查找并访问了。

## 四、蓝图如何读写属性

### 5.1 读取属性

```cpp
// 1. 通过名字查找 FProperty
FProperty* Prop = Class->FindPropertyByName(FName("ProgressAmount"));

// 2. 获取内存偏移
int32 Offset = Prop->GetOffset_ForInternal();  // 例如 0x0048

// 3. 计算属性在对象中的地址
void* PropAddr = (uint8*)QuestTestActor + Offset;

// 4. 根据属性类型读取值
FIntProperty* IntProp = (FIntProperty*)Prop;
int32 Value = IntProp->GetPropertyValue(PropAddr);  // 直接读内存
```

### 5.2 写入属性

```cpp
// 1-3 步同上

// 4. 根据属性类型写入值
FIntProperty* IntProp = (FIntProperty*)Prop;
IntProp->SetPropertyValue(PropAddr, 5);  // 直接写内存
```

### 5.3 复杂类型的读写

对于 `FString`、`TArray`、`TMap` 等复杂类型，不能简单 memcpy，需要通过 `FProperty` 的虚函数处理：

```cpp
// FProperty 提供的读写接口
virtual void CopySingleValueToScriptVM(void* Dest, void const* Src) const;    // C++ → 蓝图
virtual void CopySingleValueFromScriptVM(void* Dest, void const* Src) const;  // 蓝图 → C++
virtual void CopyCompleteValue(void* Dest, void const* Src) const;            // 完整拷贝
virtual void DestroyValue(void* Dest) const;                                  // 析构
virtual void InitializeValue(void* Dest) const;                               // 初始化
```

每个 `FProperty` 子类重写这些虚函数，处理各自类型的内存布局差异。

## 五、PropertyFlags 控制可见性

不同说明符组合产生不同的标志位，控制属性在编辑器和蓝图中的可见性：

```cpp
// EditAnywhere | BlueprintReadWrite
(EPropertyFlags)0x0010000000000005
// 0x00000001 = CPF_Edit              → 编辑器可见
// 0x00000004 = CPF_BlueprintVisible  → 蓝图可见
// 0x0010000000000000                 → EditAnywhere 组合标志

// UPROPERTY() 无说明符
(EPropertyFlags)0x0040000000000000
// 0x0040000000000000 = CPF_Transient → 不序列化
// 没有 CPF_Edit，没有 CPF_BlueprintVisible
// → 编辑器和蓝图都看不到，仅注册反射信息（用于 GC 追踪等）
```

| 说明符组合 | CPF_Edit | CPF_BlueprintVisible | CPF_BlueprintReadOnly | 效果 |
|-----------|----------|---------------------|----------------------|------|
| `EditAnywhere, BlueprintReadWrite` | ✓ | ✓ | | 编辑器可编辑，蓝图可读写 |
| `EditAnywhere, BlueprintReadOnly` | ✓ | ✓ | ✓ | 编辑器可编辑，蓝图只读 |
| `BlueprintReadWrite` | | ✓ | | 编辑器不可见，蓝图可读写 |
| `VisibleAnywhere, BlueprintReadOnly` | | ✓ | ✓ | 编辑器可见不可编辑，蓝图只读 |
| 无说明符 | | | | 仅注册反射，编辑器和蓝图都不可见 |

## 六、不同属性类型的特殊处理

| 属性类型 | FProperty 子类 | 特殊处理 |
|---------|---------------|---------|
| `bool` | `FBoolProperty` | 需要 `SetBit` 辅助函数，处理 bool 内存布局差异 |
| `int8/int16/int32/int64` | `FIntProperty` | 直接通过偏移读写，区分有符号/无符号 |
| `float` | `FFloatProperty` | 直接通过偏移读写，4 字节浮点 |
| `FString` | `FStrProperty` | 读写需要调用 FString 构造/析构，不能简单 memcpy |
| `FName` | `FNameProperty` | 内部为索引+实例号，读写需要 FName 特殊处理 |
| `UObject*` | `FObjectProperty` | 需要记录所引用的 UClass（限制类型），GC 需要追踪引用 |
| `TSubclassOf<>` | `FClassProperty` | 需要记录元类和限制类，蓝图选择时只显示子类 |
| `TArray<>` | `FArrayProperty` | 需要内层数据的 FProperty，动态大小，通过 ScriptArray 访问 |
| `TMap<>` | `FMapProperty` | 需要键和值的 FProperty，通过 ScriptMap 访问 |
| `TSet<>` | `FSetProperty` | 需要元素的 FProperty，通过 ScriptSet 访问 |
| `枚举` | `FEnumProperty` | 需要关联 UEnum，底层可能是不同大小的整数 |
| `结构体` | `FStructProperty` | 需要关联 UScriptStruct，包含子属性链 |

## 七、完整流程图

```
编译时
═══════════════════════════════════════════════════════════

  开发者写的代码                           UHT 生成的代码
  ─────────────                           ────────────────

  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  int32 ProgressAmount;
         │
         ▼ UHT 扫描

                                          .gen.cpp 中生成：
                                          ├─ FIntPropertyParams NewProp_ProgressAmount = {
                                          │    "ProgressAmount",
                                          │    STRUCT_OFFSET(AQuestTestActor, ProgressAmount),
                                          │    CPF_Edit | CPF_BlueprintVisible,
                                          │    ...
                                          │  }
                                          │
                                          ├─ PropPointers[] = {
                                          │    &NewProp_ProgressAmount,
                                          │    ...
                                          │  }
                                          │
                                          └─ ClassParams 中包含 PropPointers


引擎初始化时
═══════════════════════════════════════════════════════════

  ConstructUClass() 遍历 PropPointers[]
         │
         ▼
  为每个属性创建 FProperty 子类对象
  ├─ FBoolProperty("bAutoTestOnBeginPlay")
  │    Offset_Internal = STRUCT_OFFSET 值
  │    PropertyFlags = CPF_Edit | CPF_BlueprintVisible
  │
  ├─ FIntProperty("ProgressAmount")
  │    Offset_Internal = STRUCT_OFFSET 值
  │    PropertyFlags = CPF_Edit | CPF_BlueprintVisible
  │
  ├─ FClassProperty("QuestClassToSpawn")
  │    Offset_Internal = STRUCT_OFFSET 值
  │    MetaClass = UClass
  │    RestrictClass = UQuestBase
  │
  └─ FObjectProperty("CurrentQuest")
       Offset_Internal = STRUCT_OFFSET 值
       PropertyClass = UQuestBase
       PropertyFlags = CPF_Transient（无 Edit/Blueprint）

  存入 UClass 的属性链表和映射
  UClass: "ProgressAmount" → FIntProperty(Offset=0x0048)


运行时 — 蓝图访问
═══════════════════════════════════════════════════════════

  蓝图读取 ProgressAmount：
  ┌──────────────────────────────────────────────────┐
  │                                                  │
  │  1. FindPropertyByName("ProgressAmount")         │
  │     → FIntProperty*                              │
  │                                                  │
  │  2. GetOffset_ForInternal()                      │
  │     → 0x0048                                     │
  │                                                  │
  │  3. (uint8*)Obj + 0x0048                         │
  │     → 属性在内存中的地址                          │
  │                                                  │
  │  4. GetPropertyValue(Addr)                       │
  │     → 直接读内存，得到 int32 值                   │
  │                                                  │
  └──────────────────────────────────────────────────┘

  蓝图写入 ProgressAmount = 5：
  ┌──────────────────────────────────────────────────┐
  │                                                  │
  │  1-3 步同上                                      │
  │                                                  │
  │  4. SetPropertyValue(Addr, 5)                    │
  │     → 直接写内存                                 │
  │                                                  │
  └──────────────────────────────────────────────────┘
```

## 八、UPROPERTY 与 UFUNCTION 对比总结

| 阶段 | UPROPERTY | UFUNCTION |
|------|-----------|-----------|
| 开发者标记 | `UPROPERTY(EditAnywhere, BlueprintReadWrite)` | `UFUNCTION(BlueprintCallable)` |
| UHT 生成 | `FxxxPropertyParams`（含 `STRUCT_OFFSET`） | `exec` 桥接函数 + `UFunction` 构造函数 |
| 反射对象 | `FProperty` 子类（存储内存偏移） | `UFunction`（存储函数指针） |
| 注册方式 | 通过 `PropPointers[]` 传入 `ConstructUClass` | 通过 `RegisterFunctions` 递交 exec 指针 |
| 蓝图访问 | `对象基址 + Offset` 直接读写内存 | `call 函数指针` 通过桥接调用 |
| 跨 DLL | 不需要，偏移在同 DLL 内链接时确定 | 不需要，exec 指针在同 DLL 内链接时确定 |
| 复杂类型 | 通过 `FProperty` 虚函数处理内存布局差异 | 通过 `FFrame` 栈帧手动编组参数 |

**核心区别**：UFUNCTION 需要桥接函数是因为 C++ 成员函数的调用约定（this 指针、参数压栈）与蓝图虚拟机不同；UPROPERTY 不需要桥接是因为属性就是一段内存，知道偏移就能直接读写。
