using UnrealBuildTool;

public class QuestEditor: ModuleRules
{
    public QuestEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "QuestSystem"      // “¿¿µ QuestSystem Module
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate",
            "SlateCore",
            "EditorStyle",
            "UnrealEd"
        });
    }
}
