#include "QuestManager.h"

UQuestManager* UQuestManager::Get(const UObject* WorldContextObject)
{
    if (UGameInstance* GI = WorldContextObject->GetWorld()->GetGameInstance())
    {
        return GI->GetSubsystem<UQuestManager>();
    }
    return nullptr;
}

void UQuestManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("QuestManager initialized"));
}

void UQuestManager::Deinitialize()
{
    ActiveQuests.Empty();
    UE_LOG(LogTemp, Log, TEXT("QuestManager deinitialized"));
    Super::Deinitialize();
}

UQuestBase* UQuestManager::CreateQuest(TSubclassOf<UQuestBase> QuestClass)
{
    if (!QuestClass)
    {
        return nullptr;
    }

    UQuestBase* NewQuest = NewObject<UQuestBase>(this, QuestClass);
    return NewQuest;
}

void UQuestManager::AddQuest(UQuestBase* Quest)
{
    if (Quest && !ActiveQuests.Contains(Quest))
    {
        ActiveQuests.Add(Quest);
        Quest->Activate();
        OnQuestAdded.Broadcast(Quest);
    }
}

bool UQuestManager::RemoveQuest(UQuestBase* Quest)
{
    if (ActiveQuests.Remove(Quest) > 0)
    {
        OnQuestRemoved.Broadcast(Quest);
        return true;
    }
    return false;
}

TArray<UQuestBase*> UQuestManager::GetAllActiveQuests() const
{
    return ActiveQuests;
}

UQuestBase* UQuestManager::FindQuestByName(const FString& QuestName) const
{
    for (UQuestBase* Quest : ActiveQuests)
    {
        if (Quest && Quest->QuestName == QuestName)
        {
            return Quest;
        }
    }
    return nullptr;
}
