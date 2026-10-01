#include "MontageImpactComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogMontageImpact, Log, All);

namespace
{
	/** GEngine は commandlet 等で null になり得る */
	void ScreenMessage(int32 Key, float TimeToDisplay, const FColor& Color, const FString& Message)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(Key, TimeToDisplay, Color, Message);
		}
	}

	constexpr int32 DebugStatusKey = 200;
	constexpr int32 MaxHitHistoryBeforePrune = 64;
}

UMontageImpactComponent::UMontageImpactComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// アニメーション評価が終わった後のボーン位置を読む
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	HitObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Destructible));
}

void UMontageImpactComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveMeshComponent();
}

void UMontageImpactComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeactivateImpact();
	Super::EndPlay(EndPlayReason);
}

void UMontageImpactComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// アクターごと破棄される場合、Sphere は Owner 側が破棄する
	if (!bDestroyingHierarchy && IsValid(ImpactSphere))
	{
		ImpactSphere->DestroyComponent();
	}
	ImpactSphere = nullptr;

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UMontageImpactComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner) return;

	USkeletalMeshComponent* MeshComp = ResolveMeshComponent();

	// --- Montage state detection ---
	bool bMontagePlaying = false;
	UAnimMontage* ActiveMontage = nullptr;
	if (MeshComp)
	{
		if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
		{
			bMontagePlaying = AnimInst->IsAnyMontagePlaying();
			if (bMontagePlaying)
			{
				ActiveMontage = AnimInst->GetCurrentActiveMontage();
			}
		}
	}

	if (bMontagePlaying && !bIsActive)
	{
		ActivateImpact(ActiveMontage);
	}
	else if (!bMontagePlaying && bIsActive)
	{
		DeactivateImpact();
	}
	else if (bMontagePlaying && ActiveMontage != CurrentMontage.Get())
	{
		// 再生が途切れないまま別のモンタージュへ切り替わった → 新しい攻撃として扱う
		CurrentMontage = ActiveMontage;
		ResetTracking();
		HitTimestamps.Reset();
	}

	// --- Detection window ---
	const bool bDetecting = bIsActive && MeshComp && IsDetectionWindowOpen();
	if (bDetecting != bWasDetecting)
	{
		bWasDetecting = bDetecting;
		ResetTracking();
		if (bDetecting)
		{
			// 判定窓ごとに「1回だけヒット」をやり直す
			if (bRequireNotifyWindow)
			{
				HitTimestamps.Reset();
			}
		}
		else
		{
			HideImpactSphere();
		}
	}
	if (!bDetecting) return;

	// --- Bone velocity measurement ---
	if (bBoneCacheDirty || MeshComp->GetNumBones() != TrackedMeshBoneCount)
	{
		RebuildBoneCache(*MeshComp);
	}

	const FTransform MeshTransform = MeshComp->GetComponentTransform();
	const bool bCanMeasure = bHasPreviousSample && DeltaTime > KINDA_SMALL_NUMBER;

	TArray<FMontageImpactBoneSample, TInlineAllocator<16>> Candidates;
	for (int32 Slot = 0; Slot < TrackedBoneIndices.Num(); ++Slot)
	{
		const int32 BoneIndex = TrackedBoneIndices[Slot];
		const FVector CurLocal = MeshComp->GetBoneTransform(BoneIndex, FTransform::Identity).GetLocation();

		if (bCanMeasure)
		{
			const FVector PrevLocal = PreviousBoneLocalLocations[Slot];
			const FVector CurWorld = MeshTransform.TransformPosition(CurLocal);
			const FVector PrevWorld = PreviousMeshTransform.TransformPosition(PrevLocal);

			const double Distance = bIgnoreOwnerMovement
				? MeshTransform.TransformVector(CurLocal - PrevLocal).Size()
				: FVector::Dist(CurWorld, PrevWorld);
			const float Speed = static_cast<float>(Distance / DeltaTime);

			if (Speed >= VelocityThreshold)
			{
				FMontageImpactBoneSample& Sample = Candidates.AddDefaulted_GetRef();
				Sample.Name = MeshComp->GetBoneName(BoneIndex);
				Sample.World = CurWorld;
				Sample.PrevWorld = PrevWorld;
				Sample.Speed = Speed;
				Sample.Radius = GetRadiusForBone(Sample.Name);
			}
		}

		PreviousBoneLocalLocations[Slot] = CurLocal;
	}
	PreviousMeshTransform = MeshTransform;
	bHasPreviousSample = true;

	// --- Select the fastest bones (skip bones overlapping an already selected one) ---
	Candidates.Sort([](const FMontageImpactBoneSample& A, const FMontageImpactBoneSample& B)
	{
		return A.Speed > B.Speed;
	});

	const int32 MaxBones = (DetectionMode == EMontageImpactDetectionMode::Sweep)
		? FMath::Max(1, MaxSimultaneousBones)
		: 1;

	TArray<FMontageImpactBoneSample, TInlineAllocator<8>> Selected;
	for (const FMontageImpactBoneSample& Candidate : Candidates)
	{
		if (Selected.Num() >= MaxBones) break;

		bool bOverlapsSelected = false;
		for (const FMontageImpactBoneSample& Other : Selected)
		{
			if (FVector::DistSquared(Candidate.World, Other.World) < FMath::Square(Candidate.Radius))
			{
				bOverlapsSelected = true;
				break;
			}
		}
		if (!bOverlapsSelected)
		{
			Selected.Add(Candidate);
		}
	}

	FastestBoneName = Selected.Num() > 0 ? Selected[0].Name : FName(NAME_None);
	FastestBoneSpeed = Selected.Num() > 0 ? Selected[0].Speed : 0.0f;

	// --- Below threshold: hide collision ---
	if (Selected.Num() == 0)
	{
		HideImpactSphere();

		if (bDebugDraw)
		{
			ScreenMessage(DebugStatusKey, 0.0f, FColor::Silver,
				FString::Printf(TEXT("[AMI] No bone above Threshold: %.0f"), VelocityThreshold));
		}
		return;
	}

	// --- Above threshold: detect ---
	TArray<AActor*> AttachedActors;
	Owner->GetAttachedActors(AttachedActors);

	if (bDebugDraw)
	{
		UWorld* World = GetWorld();
		for (const FMontageImpactBoneSample& Bone : Selected)
		{
			DrawDebugSphere(World, Bone.World, Bone.Radius, 12, FColor::Red, false, 0.0f, 0, 2.0f);
			if (DetectionMode == EMontageImpactDetectionMode::Sweep)
			{
				DrawDebugLine(World, Bone.PrevWorld, Bone.World, FColor::Orange, false, 0.0f, 0, 2.0f);
			}
		}
		ScreenMessage(DebugStatusKey, 0.0f, FColor::Yellow,
			FString::Printf(TEXT("[AMI] HIT ACTIVE: %s Speed=%.0f (%d bone)"),
				*FastestBoneName.ToString(), FastestBoneSpeed, Selected.Num()));
	}

	if (DetectionMode == EMontageImpactDetectionMode::Overlap)
	{
		const FMontageImpactBoneSample& Bone = Selected[0];
		if (EnsureImpactSphere())
		{
			UpdateImpactSphere(Bone.World, Bone.Radius);
			ProcessSphereOverlaps(Bone, AttachedActors);
		}
	}
	else
	{
		HideImpactSphere();
		for (const FMontageImpactBoneSample& Bone : Selected)
		{
			SweepBone(Bone, AttachedActors);
			if (!bIsActive) return; // イベントハンドラ内で破棄された
		}
	}
}

// =====================================================================
// Window (UAnimNotifyState_MontageImpact)
// =====================================================================
void UMontageImpactComponent::BeginDetectionWindow()
{
	++NotifyWindowDepth;
}

void UMontageImpactComponent::EndDetectionWindow()
{
	NotifyWindowDepth = FMath::Max(0, NotifyWindowDepth - 1);
}

// =====================================================================
// Owner SkeletalMesh discovery
// =====================================================================
USkeletalMeshComponent* UMontageImpactComponent::FindOwnerSkeletalMesh() const
{
	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;

	// ACharacter shortcut
	if (ACharacter* Char = Cast<ACharacter>(Owner))
	{
		return Char->GetMesh();
	}

	return Owner->FindComponentByClass<USkeletalMeshComponent>();
}

USkeletalMeshComponent* UMontageImpactComponent::ResolveMeshComponent()
{
	if (USkeletalMeshComponent* Cached = CachedMesh.Get())
	{
		return Cached;
	}

	USkeletalMeshComponent* Found = FindOwnerSkeletalMesh();
	if (Found)
	{
		// メッシュのアニメーション評価後に Tick させる
		AddTickPrerequisiteComponent(Found);
		CachedMesh = Found;
		bBoneCacheDirty = true;
	}
	return Found;
}

// =====================================================================
// Activate / Deactivate
// =====================================================================
void UMontageImpactComponent::ActivateImpact(UAnimMontage* Montage)
{
	bIsActive = true;
	CurrentMontage = Montage;
	ResetTracking();
	HitTimestamps.Reset();
	HideImpactSphere();

	if (bDebugDraw)
	{
		ScreenMessage(-1, 2.0f, FColor::Cyan, TEXT("[AMI] Impact detection ACTIVATED"));
	}
}

void UMontageImpactComponent::DeactivateImpact()
{
	const bool bWasActive = bIsActive;

	bIsActive = false;
	bWasDetecting = false;
	NotifyWindowDepth = 0;
	CurrentMontage.Reset();
	ResetTracking();
	HitTimestamps.Reset();
	HideImpactSphere();

	if (bWasActive && bDebugDraw)
	{
		ScreenMessage(-1, 2.0f, FColor::Cyan, TEXT("[AMI] Impact detection DEACTIVATED"));
	}
}

void UMontageImpactComponent::ResetTracking()
{
	bHasPreviousSample = false;
	bBoneCacheDirty = true;
}

// =====================================================================
// Bone cache
// =====================================================================
void UMontageImpactComponent::RebuildBoneCache(const USkeletalMeshComponent& Mesh)
{
	TrackedBoneIndices.Reset();

	const int32 NumBones = Mesh.GetNumBones();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		if (PassesBoneFilter(Mesh, Mesh.GetBoneName(BoneIndex)))
		{
			TrackedBoneIndices.Add(BoneIndex);
		}
	}

	PreviousBoneLocalLocations.Reset();
	PreviousBoneLocalLocations.SetNumZeroed(TrackedBoneIndices.Num());

	TrackedMeshBoneCount = NumBones;
	bHasPreviousSample = false;
	bBoneCacheDirty = false;
}

bool UMontageImpactComponent::PassesBoneFilter(const USkeletalMeshComponent& Mesh, FName BoneName) const
{
	auto Matches = [&](const TArray<FName>& List)
	{
		for (const FName& ListedBone : List)
		{
			if (BoneName == ListedBone) return true;
			if (bFilterAppliesToChildBones && Mesh.BoneIsChildOf(BoneName, ListedBone)) return true;
		}
		return false;
	};

	if (IncludeBones.Num() > 0 && !Matches(IncludeBones)) return false;
	if (ExcludeBones.Num() > 0 && Matches(ExcludeBones)) return false;
	return true;
}

float UMontageImpactComponent::GetRadiusForBone(FName BoneName) const
{
	if (const float* Override = BoneRadiusOverrides.Find(BoneName))
	{
		return FMath::Max(1.0f, *Override);
	}
	return FMath::Max(1.0f, CollisionRadius);
}

// =====================================================================
// Sphere (Overlap mode)
// =====================================================================
bool UMontageImpactComponent::EnsureImpactSphere()
{
	if (IsValid(ImpactSphere)) return true;

	AActor* Owner = GetOwner();
	if (!Owner) return false;

	USceneComponent* Root = Owner->GetRootComponent();
	if (!Root)
	{
		if (!bLoggedMissingRoot)
		{
			bLoggedMissingRoot = true;
			UE_LOG(LogMontageImpact, Warning,
				TEXT("%s has no root component; Overlap mode needs one to attach its sphere. Use Sweep mode instead."),
				*GetNameSafe(Owner));
		}
		return false;
	}

	ImpactSphere = NewObject<USphereComponent>(Owner, NAME_None, RF_Transient);
	ImpactSphere->SetSphereRadius(CollisionRadius);
	ImpactSphere->SetCollisionProfileName(CollisionProfileName);
	ImpactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ImpactSphere->SetGenerateOverlapEvents(true);
	ImpactSphere->SetCanEverAffectNavigation(false);
	ImpactSphere->SetHiddenInGame(!bDebugDraw);
	ImpactSphere->SetVisibility(false);
	ImpactSphere->SetupAttachment(Root);
	ImpactSphere->RegisterComponent();
	return true;
}

void UMontageImpactComponent::HideImpactSphere()
{
	if (IsValid(ImpactSphere))
	{
		ImpactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ImpactSphere->SetVisibility(false);
	}
}

void UMontageImpactComponent::UpdateImpactSphere(const FVector& Location, float Radius)
{
	if (!IsValid(ImpactSphere)) return;

	ImpactSphere->SetSphereRadius(Radius);
	ImpactSphere->SetWorldLocation(Location);
	ImpactSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ImpactSphere->SetHiddenInGame(!bDebugDraw);
	ImpactSphere->SetVisibility(bDebugDraw);
	ImpactSphere->UpdateOverlaps();
}

void UMontageImpactComponent::ProcessSphereOverlaps(const FMontageImpactBoneSample& Bone, const TArray<AActor*>& AttachedActors)
{
	if (!IsValid(ImpactSphere)) return;

	TArray<UPrimitiveComponent*> Overlaps;
	ImpactSphere->GetOverlappingComponents(Overlaps);

	const FVector MoveDirection = (Bone.World - Bone.PrevWorld).GetSafeNormal();

	for (UPrimitiveComponent* Component : Overlaps)
	{
		if (!IsValid(Component)) continue;

		AActor* OtherActor = Component->GetOwner();
		if (!OtherActor) continue;
		if (ShouldIgnoreActor(OtherActor, AttachedActors)) continue;
		if (!PassesObjectFilter(Component)) continue;
		if (IsOnCooldown(OtherActor)) continue;

		FHitResult Hit;
		Hit.bBlockingHit = true;
		Hit.HitObjectHandle = FActorInstanceHandle(OtherActor);
		Hit.Component = Component;
		Hit.ImpactPoint = Bone.World;
		Hit.Location = Bone.World;
		Hit.ImpactNormal = -MoveDirection;
		Hit.Normal = -MoveDirection;
		Hit.TraceStart = Bone.PrevWorld;
		Hit.TraceEnd = Bone.World;

		HandleHit(OtherActor, Hit, Bone);
		if (!bIsActive) return; // イベントハンドラ内で破棄された
	}
}

// =====================================================================
// Sweep mode
// =====================================================================
void UMontageImpactComponent::SweepBone(const FMontageImpactBoneSample& Bone, const TArray<AActor*>& AttachedActors)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner) return;

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(Owner);
	QueryParams.AddIgnoredActors(AttachedActors);

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(
		Hits,
		Bone.PrevWorld,
		Bone.World,
		FQuat::Identity,
		BuildObjectQueryParams(),
		FCollisionShape::MakeSphere(Bone.Radius),
		QueryParams);

	const FVector MoveDirection = (Bone.World - Bone.PrevWorld).GetSafeNormal();

	for (const FHitResult& SweepHit : Hits)
	{
		AActor* HitActor = SweepHit.GetActor();
		if (!HitActor) continue;
		if (ShouldIgnoreActor(HitActor, AttachedActors)) continue;
		if (IsOnCooldown(HitActor)) continue;

		FHitResult Hit = SweepHit;
		if (Hit.bStartPenetrating)
		{
			// 開始時点で重なっている場合、エンジンは有効な衝突点を返さない
			Hit.ImpactPoint = Bone.PrevWorld;
			Hit.Location = Bone.PrevWorld;
			if (Hit.ImpactNormal.IsNearlyZero())
			{
				Hit.ImpactNormal = -MoveDirection;
				Hit.Normal = -MoveDirection;
			}
		}
		Hit.TraceStart = Bone.PrevWorld;
		Hit.TraceEnd = Bone.World;

		HandleHit(HitActor, Hit, Bone);
		if (!bIsActive) return; // イベントハンドラ内で破棄された
	}
}

FCollisionObjectQueryParams UMontageImpactComponent::BuildObjectQueryParams() const
{
	if (HitObjectTypes.Num() == 0)
	{
		return FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects);
	}

	FCollisionObjectQueryParams Params;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : HitObjectTypes)
	{
		Params.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType.GetValue()));
	}
	return Params;
}

bool UMontageImpactComponent::PassesObjectFilter(const UPrimitiveComponent* Component) const
{
	if (HitObjectTypes.Num() == 0) return true;
	if (!Component) return false;

	const EObjectTypeQuery ObjectType = UEngineTypes::ConvertToObjectType(Component->GetCollisionObjectType());
	return HitObjectTypes.Contains(TEnumAsByte<EObjectTypeQuery>(ObjectType));
}

// =====================================================================
// Ignore / cooldown / hit
// =====================================================================
bool UMontageImpactComponent::ShouldIgnoreActor(const AActor* Actor, const TArray<AActor*>& AttachedActors) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Actor) return true;
	if (Actor == Owner) return true;
	if (Actor->GetOwner() == Owner) return true;

	// Ignore attached actors (weapons, etc.)
	return AttachedActors.Contains(Actor);
}

bool UMontageImpactComponent::IsOnCooldown(AActor* Actor) const
{
	const double* LastHitTime = HitTimestamps.Find(TWeakObjectPtr<AActor>(Actor));
	if (!LastHitTime) return false;

	// HitCooldown = 0: 判定が終わる (モンタージュ終了 / 判定窓が閉じる) まで再ヒットしない
	if (HitCooldown <= 0.0f) return true;

	const UWorld* World = GetWorld();
	if (!World) return true;

	return (World->GetTimeSeconds() - *LastHitTime) < HitCooldown;
}

void UMontageImpactComponent::HandleHit(AActor* HitActor, const FHitResult& Hit, const FMontageImpactBoneSample& Bone)
{
	const UWorld* World = GetWorld();
	if (!World || !HitActor) return;

	HitTimestamps.Add(TWeakObjectPtr<AActor>(HitActor), World->GetTimeSeconds());
	PruneHitHistory();

	OnMontageImpactHit.Broadcast(Bone.Name, Bone.Speed, Hit);

	if (bDebugDraw)
	{
		ScreenMessage(-1, 2.0f, FColor::Magenta,
			FString::Printf(TEXT("[AMI] HIT: %s (Bone: %s, Speed: %.0f)"),
				*HitActor->GetName(), *Bone.Name.ToString(), Bone.Speed));
		UE_LOG(LogMontageImpact, Verbose, TEXT("Hit %s (Bone: %s, Speed: %.0f)"),
			*HitActor->GetName(), *Bone.Name.ToString(), Bone.Speed);
	}
}

void UMontageImpactComponent::PruneHitHistory()
{
	if (HitTimestamps.Num() <= MaxHitHistoryBeforePrune) return;

	for (auto It = HitTimestamps.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
