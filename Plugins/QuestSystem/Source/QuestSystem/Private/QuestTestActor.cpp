#include "QuestTestActor.h"
#include "QuestManager.h"
#include "Engine/Engine.h"
#include "Kismet/KismetSystemLibrary.h"

AQuestTestActor::AQuestTestActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AQuestTestActor::BeginPlay()
{
    Super::BeginPlay();

    if (!bAutoTestOnBeginPlay)
    {
        return;
    }

    UKismetSystemLibrary::PrintString(this, TEXT("=== Quest Auto Test Start ==="), true, false, FLinearColor::Green, 60.0f);

    CreateAndAddQuest();

    if (CurrentQuest)
    {
        AddProgress();
        PrintAllQuests();
    }

    UKismetSystemLibrary::PrintString(this, TEXT("=== Quest Auto Test Complete ==="), true, false, FLinearColor::Green, 60.0f);
}

void AQuestTestActor::CreateAndAddQuest()
{
    UQuestManager* Manager = UQuestManager::Get(this);
    if (!Manager)
    {
        UKismetSystemLibrary::PrintString(this, TEXT("[ERROR] QuestManager not found!"), true, false, FLinearColor::Red, 60.0f);
        return;
    }

    if (!QuestClassToSpawn)
    {
        QuestClassToSpawn = UQuestBase::StaticClass();
    }

    CurrentQuest = Manager->CreateQuest(QuestClassToSpawn);
    if (CurrentQuest)
    {
        Manager->AddQuest(CurrentQuest);
        FString Msg = FString::Printf(TEXT("C++ CreateAndAddQuest: quest created, Name='%s'"), *CurrentQuest->QuestName);
        UKismetSystemLibrary::PrintString(this, Msg, true, false, FLinearColor(0.f, 1.f, 1.f, 1.f), 60.0f);
    }
}

void AQuestTestActor::AddProgress()
{
    if (CurrentQuest)
    {
        CurrentQuest->UpdateProgress(ProgressAmount);
        FString Msg = FString::Printf(TEXT("C++ AddProgress: %d -> %d/%d"),
            ProgressAmount, CurrentQuest->CurrentProgress, CurrentQuest->RequiredProgress);
        UKismetSystemLibrary::PrintString(this, Msg, true, false, FLinearColor::Yellow, 60.0f);
    }
    else
    {
        UKismetSystemLibrary::PrintString(this, TEXT("[WARN] No current quest"), true, false, FLinearColor::Red, 60.0f);
    }
}

void AQuestTestActor::CompleteCurrentQuest()
{
    if (CurrentQuest)
    {
        CurrentQuest->Complete();
        FString Msg = FString::Printf(TEXT("C++ CompleteCurrentQuest: '%s' completed!"), *CurrentQuest->QuestName);
        UKismetSystemLibrary::PrintString(this, Msg, true, false, FLinearColor::Green, 60.0f);
    }
}

void AQuestTestActor::FailCurrentQuest()
{
    if (CurrentQuest)
    {
        CurrentQuest->Fail();
        FString Msg = FString::Printf(TEXT("C++ FailCurrentQuest: '%s' failed!"), *CurrentQuest->QuestName);
        UKismetSystemLibrary::PrintString(this, Msg, true, false, FLinearColor::Red, 60.0f);
    }
}

void AQuestTestActor::PrintAllQuests()
{
    UQuestManager* Manager = UQuestManager::Get(this);
    if (!Manager)
    {
        UKismetSystemLibrary::PrintString(this, TEXT("[ERROR] QuestManager not found!"), true, false, FLinearColor::Red, 60.0f);
        return;
    }

    TArray<UQuestBase*> Quests = Manager->GetAllActiveQuests();
    FString Header = FString::Printf(TEXT("C++ PrintAllQuests: %d active quests"), Quests.Num());
    UKismetSystemLibrary::PrintString(this, Header, true, false, FLinearColor::White, 60.0f);

    for (UQuestBase* Quest : Quests)
    {
        if (Quest)
        {
            FString Msg = FString::Printf(TEXT("  - %s | Status: %d | Progress: %d/%d"),
                *Quest->QuestName,
                static_cast<int32>(Quest->Status),
                Quest->CurrentProgress,
                Quest->RequiredProgress);
            UKismetSystemLibrary::PrintString(this, Msg, true, false, FLinearColor::White, 60.0f);
        }
    }
}
