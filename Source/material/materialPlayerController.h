#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "materialPlayerController.generated.h"

UCLASS()
class MATERIAL_API AmaterialPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AmaterialPlayerController();
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	
private:
	void OnMouseClick();
	void OnEscapeKey();
};