#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "QuestBase.h"
#include "QuestManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestUpdated, UQuestBase*, Quest);

UCLASS()
class QUESTSYSTEM_API UQuestManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Quest", meta = (WorldContext = "WorldContextObject"))
    static UQuestManager* Get(const UObject* WorldContextObject);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    UQuestBase* CreateQuest(TSubclassOf<UQuestBase> QuestClass);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void AddQuest(UQuestBase* Quest);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    bool RemoveQuest(UQuestBase* Quest);

    UFUNCTION(BlueprintPure, Category = "Quest")
    TArray<UQuestBase*> GetAllActiveQuests() const;

    UFUNCTION(BlueprintPure, Category = "Quest")
    UQuestBase* FindQuestByName(const FString& QuestName) const;

    UPROPERTY(BlueprintAssignable, Category = "Quest")
    FOnQuestUpdated OnQuestAdded;

    UPROPERTY(BlueprintAssignable, Category = "Quest")
    FOnQuestUpdated OnQuestRemoved;

protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    UPROPERTY()
    TArray<UQuestBase*> ActiveQuests;
};
