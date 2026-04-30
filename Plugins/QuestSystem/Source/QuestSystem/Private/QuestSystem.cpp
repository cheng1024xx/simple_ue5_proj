// Copyright Epic Games, Inc. All Rights Reserved.

#include "QuestSystem.h"
#include "QuestManager.h"

#define LOCTEXT_NAMESPACE "FQuestSystemModule"

void FQuestSystemModule::StartupModule()
{
    if (bIsInitialized)
    {
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("QuestSystem Module has been loaded"));
    bIsInitialized = true;
}

void FQuestSystemModule::ShutdownModule()
{
    if (!bIsInitialized)
    {
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("QuestSystem Module has been unloaded"));
    bIsInitialized = false;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FQuestSystemModule, QuestSystem)