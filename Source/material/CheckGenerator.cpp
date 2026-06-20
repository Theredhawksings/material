// Fill out your copyright notice in the Description page of Project Settings.

#include "CheckGenerator.h"
#include "Generator.h" 
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/StaticMeshComponent.h"

ACheckGenerator::ACheckGenerator()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ACheckGenerator::BeginPlay()
{
    Super::BeginPlay();
    
    const FVector NormDir = OpenDirection.GetSafeNormal();

    if (LeftDoorActor)
    {
        LeftStartLocation  = LeftDoorActor->GetActorLocation();
        LeftTargetLocation = LeftStartLocation + NormDir * OpenDistance;
    }
    if (RightDoorActor)
    {
        RightStartLocation  = RightDoorActor->GetActorLocation();
        RightTargetLocation = RightStartLocation - NormDir * OpenDistance;
    }
}

void ACheckGenerator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ==============================================================
    // 1. 할당된 발전기들이 '실제로 돌아가고 있는지' 확인
    // ==============================================================
    if (!bActivated && AssignedGenerators.Num() > 0)
    {
        bool bAllOperating = true;

        for (AGenerator* Gen : AssignedGenerators)
        {
            // ★ 여기가 핵심 변경점! ★
            // 발전기가 없거나, EMF(전력)가 0이면 (= 코일이 안 돌아가고 멈춰있으면) 탈락!
            if (!Gen || FMath::IsNearlyZero(Gen->GetCurrentEMF()))
            {
                bAllOperating = false;
                break;
            }
        }

        // 모든 발전기가 실제로 빙글빙글 돌아가고 있다면 문 열기 발동
        if (bAllOperating)
        {
            bActivated = true;
            bIsOpening = true;

            if (DoorOpenSound)
            {
                UGameplayStatics::PlaySoundAtLocation(this, DoorOpenSound, GetActorLocation());
            }

            auto DisableCollision = [](AActor* Door)
            {
                if (!Door) return;
                TArray<UStaticMeshComponent*> Meshes;
                Door->GetComponents<UStaticMeshComponent>(Meshes);
                for (UStaticMeshComponent* M : Meshes)
                {
                    if (M) M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
            };
            
            DisableCollision(LeftDoorActor);
            DisableCollision(RightDoorActor);
        }
    }

    // ==============================================================
    // 2. 문 열리는 애니메이션 로직
    // ==============================================================
    if (bIsOpening && !bIsOpen)
    {
        CurrentTime += DeltaTime * OpenSpeed;
        if (CurrentTime >= 1.0f)
        {
            CurrentTime = 1.0f;
            bIsOpen     = true;
            bIsOpening  = false;
        }

        if (LeftDoorActor)
            LeftDoorActor->SetActorLocation(FMath::Lerp(LeftStartLocation, LeftTargetLocation, CurrentTime));

        if (RightDoorActor)
            RightDoorActor->SetActorLocation(FMath::Lerp(RightStartLocation, RightTargetLocation, CurrentTime));
    }
}