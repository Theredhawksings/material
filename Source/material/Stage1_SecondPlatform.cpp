// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage1_SecondPlatform.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Transformation_actor.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h" // 사운드 재생을 위해 추가
#include "UObject/ConstructorHelpers.h" // 에셋 경로 로드를 위해 추가

AStage1_SecondPlatform::AStage1_SecondPlatform()
    : bCharacterOnPlatform(false)
    , bTransformActorOnPlatform(false)
    , bActivated(false)
    , bIsOpening(false)
    , bIsOpen(false)
    , CurrentTime(0.0f)
{
    PrimaryActorTick.bCanEverTick = true;

    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;
    
    TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
    TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AStage1_SecondPlatform::OnOverlapBegin);
    TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AStage1_SecondPlatform::OnOverlapEnd);

    PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
    PlatformMesh->SetupAttachment(RootComponent);

    // 알려준 경로로 사운드 에셋 기본값 설정
    static ConstructorHelpers::FObjectFinder<USoundBase> SoundAsset(TEXT("/Script/Engine.SoundWave'/Game/Sound/sound_door_opening.sound_door_opening'"));
    if (SoundAsset.Succeeded())
    {
        DoorOpenSound = SoundAsset.Object;
    }
}

void AStage1_SecondPlatform::BeginPlay()
{
    Super::BeginPlay();

    if (LeftDoorActor)
    {
        LeftStartLocation = LeftDoorActor->GetActorLocation();
        LeftTargetLocation = LeftStartLocation + FVector(0.0f, -OpenDistance, 0.0f);
    }

    if (RightDoorActor)
    {
        RightStartLocation = RightDoorActor->GetActorLocation();
        RightTargetLocation = RightStartLocation + FVector(0.0f, OpenDistance, 0.0f);
    }
}

void AStage1_SecondPlatform::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bCharacterOnPlatform && !bTransformActorOnPlatform && !bActivated)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow, TEXT("액터를 올려주세요"));
        }
    }

    if (bIsOpening && !bIsOpen)
    {
        CurrentTime += DeltaTime * OpenSpeed;
        
        if (CurrentTime >= 1.0f)
        {
            CurrentTime = 1.0f;
            bIsOpen = true;
            bIsOpening = false;
        }

        if (LeftDoorActor)
        {
            FVector NewLeftLocation = FMath::Lerp(LeftStartLocation, LeftTargetLocation, CurrentTime);
            LeftDoorActor->SetActorLocation(NewLeftLocation);
        }

        if (RightDoorActor)
        {
            FVector NewRightLocation = FMath::Lerp(RightStartLocation, RightTargetLocation, CurrentTime);
            RightDoorActor->SetActorLocation(NewRightLocation);
        }
    }
}

void AStage1_SecondPlatform::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor)
        return;

    if (OtherActor->IsA(ACharacter::StaticClass()))
    {
        bCharacterOnPlatform = true;
    }
    else if (OtherActor->IsA(ATransformation_actor::StaticClass()))
    {
        bTransformActorOnPlatform = true;

        if (!bActivated)
        {
            bActivated = true;
            bIsOpening = true;

            // --- 사운드 재생 추가 부분 ---
            if (DoorOpenSound)
            {
                UGameplayStatics::PlaySoundAtLocation(this, DoorOpenSound, GetActorLocation());
            }
            // -----------------------------

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("플랫폼 활성화! 문이 열립니다"));
            }

            if (LeftDoorActor)
            {
                TArray<UStaticMeshComponent*> MeshComps;
                LeftDoorActor->GetComponents<UStaticMeshComponent>(MeshComps);
                for (UStaticMeshComponent* Mesh : MeshComps)
                {
                    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
            }

            if (RightDoorActor)
            {
                TArray<UStaticMeshComponent*> MeshComps;
                RightDoorActor->GetComponents<UStaticMeshComponent>(MeshComps);
                for (UStaticMeshComponent* Mesh : MeshComps)
                {
                    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }
            }
        }
    }
}

void AStage1_SecondPlatform::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor)
        return;

    if (OtherActor->IsA(ACharacter::StaticClass()))
    {
        bCharacterOnPlatform = false;
    }
    else if (OtherActor->IsA(ATransformation_actor::StaticClass()))
    {
        bTransformActorOnPlatform = false;
    }
}