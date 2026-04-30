#include "QuestEditorModule.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "QuestManager.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "FQuestEditorModule"

void FQuestEditorModule::StartupModule()
{
    FLevelEditorModule& LevelEditorModule =
        FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

    MenuExtender = MakeShareable(new FExtender());
    MenuExtender->AddMenuExtension(
        "WindowLayout",
        EExtensionHook::After,
        nullptr,
        FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& Builder)
            {
                Builder.AddMenuEntry(
                    LOCTEXT("QuestSystemMenu", "Quest System"),
                    LOCTEXT("QuestSystemMenuTooltip", "Open Quest System Window"),
                    FSlateIcon(),
                    FUIAction(FExecuteAction::CreateLambda([]()
                        {
                            UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
                            if (UQuestManager* Manager = UQuestManager::Get(EditorWorld))
                            {
                                TArray<UQuestBase*> ActiveQuests = Manager->GetAllActiveQuests();
                                UE_LOG(LogTemp, Log, TEXT("Quest System: %d active quests"), ActiveQuests.Num());
                                for (UQuestBase* Quest : ActiveQuests)
                                {
                                    if (Quest)
                                    {
                                        UE_LOG(LogTemp, Log, TEXT("  - %s (Status: %d, Progress: %d/%d)"),
                                            *Quest->QuestName,
                                            static_cast<int32>(Quest->Status),
                                            Quest->CurrentProgress,
                                            Quest->RequiredProgress);
                                    }
                                }
                            }
                            else
                            {
                                UE_LOG(LogTemp, Warning, TEXT("Quest System: QuestManager not available in editor context"));
                            }
                        }))
                );
            })
    );
    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);

    UE_LOG(LogTemp, Log, TEXT("QuestEditor Module has been loaded"));
}

void FQuestEditorModule::ShutdownModule()
{
    if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
    {
        FLevelEditorModule& LevelEditorModule =
            FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
        LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(MenuExtender);
    }
    MenuExtender.Reset();
    UE_LOG(LogTemp, Log, TEXT("QuestEditor Module has been unloaded"));
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FQuestEditorModule, QuestEditor)
