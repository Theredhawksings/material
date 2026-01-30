#include "Wire.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"

AWire::AWire()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
    Spline->SetupAttachment(Root);

    // Spline Point 0번에 작은 Sphere 충돌 (배터리 연결용)
    ConnectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ConnectionSphere"));
    ConnectionSphere->SetupAttachment(Root);
    ConnectionSphere->SetSphereRadius(30.f);  // 작은 반경
    ConnectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ConnectionSphere->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    ConnectionSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
    ConnectionSphere->SetGenerateOverlapEvents(true);

    bPowered = false;
}

void AWire::BeginPlay()
{
    Super::BeginPlay();

    UpdateConnectionPoint();
    RefreshConnectedActors();
    ApplyPower();

    if (RefreshInterval > 0.f)
    {
        GetWorldTimerManager().SetTimer(
            RefreshTimerHandle,
            this,
            &AWire::RefreshConnectedActors,
            RefreshInterval,
            true
        );
    }
}

void AWire::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(RefreshTimerHandle);
    Super::EndPlay(EndPlayReason);
}

void AWire::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    UpdateConnectionPoint();
    RebuildSplineMeshes();
    ApplyPower();
}

void AWire::UpdateConnectionPoint()
{
    if (!Spline || !ConnectionSphere) return;

    // Spline Point 0번 위치로 Sphere 이동
    FVector Point0Location = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
    ConnectionSphere->SetRelativeLocation(Point0Location);
    ConnectionSphere->SetSphereRadius(OverlapRadius);
}

void AWire::SetPowered(bool bNewPowered)
{
    bPowered = bNewPowered;
    ApplyPower();
    PropagatePowerToConnected();
}

void AWire::ApplyPower()
{
    UMaterialInterface* TargetMat = bPowered ? OnMaterial : OffMaterial;
    
    if (!TargetMat) 
    {
        UE_LOG(LogTemp, Error, TEXT("Wire: TargetMat이 NULL입니다!"));
        return;
    }

    for (USplineMeshComponent* Mesh : SegmentMeshes)
    {
        if (Mesh)
        {
            // 1. 머터리얼을 세팅하기 전에 슬롯을 강제로 업데이트
            int32 NumMaterials = Mesh->GetNumMaterials();
            for (int32 i = 0; i < NumMaterials; ++i)
            {
                // SetMaterial만으로 안 바뀐다면 렌더링 리소스를 다시 불러오도록 강제
                Mesh->SetMaterial(i, TargetMat);
            }
            
            // 2. 물리적/시각적 갱신 강제 (중요)
            Mesh->MarkRenderStateDirty();
            Mesh->UpdateBounds();
        }
    }
    
    // 로그에 찍히는 머터리얼 이름이 실제 에디터의 'AnimSharingGreen'과 일치하는지 꼭 확인하세요!
    UE_LOG(LogTemp, Warning, TEXT("Wire: %s ApplyPower -> bPowered: %s (Mat: %s)"), 
        *GetName(), bPowered ? TEXT("TRUE") : TEXT("FALSE"), *TargetMat->GetName());
}
void AWire::ClearGeneratedMeshes()
{
    for (USplineMeshComponent* Comp : SegmentMeshes)
    {
        if (IsValid(Comp))
        {
            Comp->DestroyComponent();
        }
    }
    SegmentMeshes.Reset();
    MIDArray.Reset();
}

void AWire::RebuildSplineMeshes()
{
    ClearGeneratedMeshes();
    if (!Spline || !SegmentMesh) return;

    const int32 NumPoints = Spline->GetNumberOfSplinePoints();
    if (NumPoints < 2) return;

    for (int32 i = 0; i < NumPoints - 1; ++i)
    {
        // 1. 컴포넌트 생성 시 Outer를 this(Wire 액터)로 확실히 지정
        USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
        if (!SplineMesh) continue;

        // 2. 모빌리티 및 생성 방법 설정
        SplineMesh->SetMobility(EComponentMobility::Movable);
        SplineMesh->CreationMethod = EComponentCreationMethod::UserConstructionScript;
        
        // 3. 계층 구조 연결 및 등록 (순서가 중요합니다)
        SplineMesh->SetupAttachment(Spline);
        SplineMesh->SetStaticMesh(SegmentMesh);
        SplineMesh->SetForwardAxis(ESplineMeshAxis::Z, false);

        // 4. 현재 전력 상태에 맞는 머터리얼 즉시 적용
        UMaterialInterface* InitialMat = bPowered ? OnMaterial : OffMaterial;
        if (InitialMat)
        {
            SplineMesh->SetMaterial(0, InitialMat);
        }

        // 5. 위치 설정 및 물리적 등록 완료
        const FVector StartPos = Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
        const FVector StartTan = Spline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
        const FVector EndPos   = Spline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);
        const FVector EndTan   = Spline->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);

        SplineMesh->SetStartAndEnd(StartPos, StartTan, EndPos, EndTan, true);
        SplineMesh->RegisterComponent(); // 반드시 마지막에 호출
        
        // 6. 배열에 추가하여 ApplyPower에서 접근 가능하게 함
        SegmentMeshes.Add(SplineMesh);
    }
}

void AWire::GatherOverlapsAt(const FVector& WorldPos, TArray<AActor*>& OutActors) const
{
    UWorld* World = GetWorld();
    if (!World) return;

    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WireOverlap), false);
    QueryParams.AddIgnoredActor(this);

    TArray<FOverlapResult> Hits;
    World->OverlapMultiByObjectType(
        Hits, WorldPos, FQuat::Identity, ObjParams, 
        FCollisionShape::MakeSphere(OverlapRadius), QueryParams);

    for (const FOverlapResult& Hit : Hits)
    {
        AActor* A = Hit.GetActor();
        if (A && (!ConnectableClass || A->IsA(ConnectableClass)))
        {
            OutActors.Add(A);
        }
    }
}

void AWire::RefreshConnectedActors()
{
    ConnectedActors.Reset();
    if (!Spline) return;

    TArray<AActor*> Found;
    
    // Spline Point 0번 위치에서 충돌 검사
    const FVector Point0World = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
    GatherOverlapsAt(Point0World, Found);

    for (AActor* A : Found)
    {
        ConnectedActors.AddUnique(A);
    }
    
    PropagatePowerToConnected();
}

void AWire::PropagatePowerToConnected()
{
    for (AActor* Target : ConnectedActors)
    {
        if (!Target) continue;
        
        if (UFunction* Func = Target->FindFunction(FName("SetPowered")))
        {
            struct FParams { bool bNewPowered; };
            FParams Params;
            Params.bNewPowered = bPowered;
            Target->ProcessEvent(Func, &Params);
        }
    }
}