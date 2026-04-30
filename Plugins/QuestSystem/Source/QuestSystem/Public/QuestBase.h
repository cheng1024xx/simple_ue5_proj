#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "QuestBase.generated.h"

UENUM(BlueprintType)
enum class EQuestStatus : uint8
{
    Inactive    UMETA(DisplayName = "Inactive"),
    Active      UMETA(DisplayName = "Active"),
    Completed   UMETA(DisplayName = "Completed"),
    Failed      UMETA(DisplayName = "Failed")
};

UCLASS(Blueprintable)
class QUESTSYSTEM_API UQuestBase : public UObject
{
    GENERATED_BODY()

public:
    UQuestBase();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FString QuestName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FString QuestDescription;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    int32 RewardXP = 100;

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    EQuestStatus Status = EQuestStatus::Inactive;

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    int32 CurrentProgress = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    int32 RequiredProgress = 1;

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void Activate();

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void UpdateProgress(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void Complete();

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void Fail();

protected:
    virtual void OnQuestActivated() {}
    virtual void OnQuestCompleted() {}
    virtual void OnQuestFailed() {}
};
