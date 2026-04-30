#include "QuestBase.h"

UQuestBase::UQuestBase()
{
    QuestName = TEXT("Default Quest");
    QuestDescription = TEXT("Complete this quest");
}

void UQuestBase::Activate()
{
    if (Status == EQuestStatus::Inactive)
    {
        Status = EQuestStatus::Active;
        CurrentProgress = 0;
        OnQuestActivated();
        UE_LOG(LogTemp, Log, TEXT("Quest '%s' has been activated"), *QuestName);
    }
}

void UQuestBase::UpdateProgress(int32 Amount)
{
    if (Status == EQuestStatus::Active)
    {
        CurrentProgress += Amount;

        if (CurrentProgress >= RequiredProgress)
        {
            Complete();
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("Quest '%s' progress: %d/%d"),
                *QuestName, CurrentProgress, RequiredProgress);
        }
    }
}

void UQuestBase::Complete()
{
    if (Status == EQuestStatus::Active)
    {
        Status = EQuestStatus::Completed;
        OnQuestCompleted();
        UE_LOG(LogTemp, Log, TEXT("Quest '%s' completed! Reward: %d XP"),
            *QuestName, RewardXP);
    }
}

void UQuestBase::Fail()
{
    if (Status == EQuestStatus::Active)
    {
        Status = EQuestStatus::Failed;
        OnQuestFailed();
        UE_LOG(LogTemp, Warning, TEXT("Quest '%s' failed!"), *QuestName);
    }
}
