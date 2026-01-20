#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "materialPlayerController.generated.h"

UCLASS()
class MATERIAL_API AmaterialPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
