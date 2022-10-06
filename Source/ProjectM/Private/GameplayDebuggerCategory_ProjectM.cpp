#include "GameplayDebuggerCategory_ProjectM.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "MassAudioPerceptionProcessor.h"

FGameplayDebuggerCategory_ProjectM::FGameplayDebuggerCategory_ProjectM()
{
  bShowOnlyWithDebugActor = false;
}

void FGameplayDebuggerCategory_ProjectM::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
  if (!OwnerPC)
  {
		return;
  }
}

void FGameplayDebuggerCategory_ProjectM::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ProjectM::MakeInstance()
{
  return MakeShareable(new FGameplayDebuggerCategory_ProjectM());
}

#endif // WITH_GAMEPLAY_DEBUGGER