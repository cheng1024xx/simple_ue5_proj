#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FQuestEditorModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FExtender> MenuExtender;
};
