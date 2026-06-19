// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Warp.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class MATERIAL_API AWarp : public AActor
{
	GENERATED_BODY()

public:
	AWarp();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

public:
	virtual void Tick(float DeltaTime) override;

	// 같은 맵 안에서 순간이동할 도착 위치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp")
	FVector TargetLocation = FVector(0.f, 0.f, 300.f);

	// 도착 후 바라볼 방향 (안 쓰면 그대로 둬도 됨)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp")
	FRotator TargetRotation = FRotator::ZeroRotator;

	// 도착 회전까지 적용할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp")
	bool bApplyRotation = true;
};