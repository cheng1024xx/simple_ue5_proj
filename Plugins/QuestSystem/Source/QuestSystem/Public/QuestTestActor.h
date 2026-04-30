#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuestBase.h"
#include "QuestTestActor.generated.h"

UCLASS()
class QUESTSYSTEM_API AQuestTestActor : public AActor
{
    GENERATED_BODY()

public:
    AQuestTestActor();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category = "Quest|Test")
    void CreateAndAddQuest();

    UFUNCTION(BlueprintCallable, Category = "Quest|Test")
    void AddProgress();

    UFUNCTION(BlueprintCallable, Category = "Quest|Test")
    void CompleteCurrentQuest();

    UFUNCTION(BlueprintCallable, Category = "Quest|Test")
    void FailCurrentQuest();

    UFUNCTION(BlueprintCallable, Category = "Quest|Test")
    void PrintAllQuests();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Test")
    bool bAutoTestOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Test")
    TSubclassOf<UQuestBase> QuestClassToSpawn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Test")
    int32 ProgressAmount = 1;

private:
    UPROPERTY()
    UQuestBase* CurrentQuest;
};
