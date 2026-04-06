#include "MontageImpactComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UMontageImpactComponent::UMontageImpactComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UMontageImpactComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMontageImpactComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeactivateImpact();
	Super::EndPlay(EndPlayReason);
}

void UMontageImpactComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// --- Montage state detection ---
	bool bIsMontagePlayingNow = false;
	USkeletalMeshComponent* MeshComp = FindOwnerSkeletalMesh();
	if (MeshComp)
	{
		if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
		{
			bIsMontagePlayingNow = AnimInst->IsAnyMontagePlaying();
		}
	}

	// Montage started
	if (bIsMontagePlayingNow && !bWasMontagePlayingLastFrame)
	{
		ActivateImpact();
	}
	// Montage ended
	else if (!bIsMontagePlayingNow && bWasMontagePlayingLastFrame)
	{
		DeactivateImpact();
	}

	bWasMontagePlayingLastFrame = bIsMontagePlayingNow;

	// --- Bone velocity measurement ---
	if (!bIsActive || !MeshComp) return;

	TArray<FName> BoneNames;
	MeshComp->GetBoneNames(BoneNames);

	FastestBoneName = NAME_None;
	FastestBoneSpeed = 0.0f;
	FVector FastestBoneLocation = FVector::ZeroVector;

	for (const FName& BoneName : BoneNames)
	{
		FVector CurrentLoc = MeshComp->GetSocketLocation(BoneName);

		if (PreviousBoneLocations.Contains(BoneName) && DeltaTime > KINDA_SMALL_NUMBER)
		{
			float Speed = FVector::Dist(CurrentLoc, PreviousBoneLocations[BoneName]) / DeltaTime;
			if (Speed > FastestBoneSpeed)
			{
				FastestBoneSpeed = Speed;
				FastestBoneName = BoneName;
				FastestBoneLocation = CurrentLoc;
			}
		}

		PreviousBoneLocations.Add(BoneName, CurrentLoc);
	}

	// --- Threshold check ---
	if (FastestBoneSpeed < VelocityThreshold || FastestBoneLocation.IsZero())
	{
		// Below threshold: hide collision
		if (ImpactSphere)
		{
			ImpactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ImpactSphere->SetVisibility(false);
		}

		if (bDebugDraw)
		{
			GEngine->AddOnScreenDebugMessage(200, 0.0f, FColor::Silver,
				FString::Printf(TEXT("[AMI] MaxSpeed: %.0f < Threshold: %.0f"), FastestBoneSpeed, VelocityThreshold));
		}
		return;
	}

	// --- Above threshold: activate collision at fastest bone ---
	UpdateImpactSphere(FastestBoneLocation);
	CleanupExpiredCooldowns();

	if (bDebugDraw)
	{
		DrawDebugSphere(GetWorld(), FastestBoneLocation, CollisionRadius, 12, FColor::Red, false, 0.0f, 0, 2.0f);
		GEngine->AddOnScreenDebugMessage(200, 0.0f, FColor::Yellow,
			FString::Printf(TEXT("[AMI] HIT ACTIVE: %s Speed=%.0f"), *FastestBoneName.ToString(), FastestBoneSpeed));
	}
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

// =====================================================================
// Activate / Deactivate
// =====================================================================
void UMontageImpactComponent::ActivateImpact()
{
	bIsActive = true;
	PreviousBoneLocations.Empty();
	HitTimestamps.Empty();

	// Create sphere if not yet created
	if (!ImpactSphere)
	{
		AActor* Owner = GetOwner();
		if (!Owner) return;

		ImpactSphere = NewObject<USphereComponent>(Owner, NAME_None, RF_Transient);
		ImpactSphere->SetSphereRadius(CollisionRadius);
		ImpactSphere->SetCollisionProfileName(CollisionProfileName);
		ImpactSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ImpactSphere->SetGenerateOverlapEvents(true);
		ImpactSphere->SetHiddenInGame(!bDebugDraw);
		ImpactSphere->SetVisibility(bDebugDraw);
		ImpactSphere->RegisterComponent();
		ImpactSphere->AttachToComponent(
			Owner->GetRootComponent(),
			FAttachmentTransformRules::KeepWorldTransform);

		ImpactSphere->OnComponentBeginOverlap.AddDynamic(
			this, &UMontageImpactComponent::OnSphereBeginOverlap);
	}

	// Start disabled until a bone exceeds the threshold
	ImpactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ImpactSphere->SetVisibility(false);

	if (bDebugDraw)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[AMI] Impact detection ACTIVATED"));
	}
}

void UMontageImpactComponent::DeactivateImpact()
{
	bIsActive = false;
	PreviousBoneLocations.Empty();
	HitTimestamps.Empty();

	if (ImpactSphere)
	{
		ImpactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ImpactSphere->SetVisibility(false);
	}

	if (bDebugDraw)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[AMI] Impact detection DEACTIVATED"));
	}
}

// =====================================================================
// Sphere placement
// =====================================================================
void UMontageImpactComponent::UpdateImpactSphere(const FVector& Location)
{
	if (!ImpactSphere) return;

	ImpactSphere->SetSphereRadius(CollisionRadius);
	ImpactSphere->SetWorldLocation(Location);
	ImpactSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ImpactSphere->SetVisibility(bDebugDraw);
	ImpactSphere->UpdateOverlaps();
}

// =====================================================================
// Overlap callback
// =====================================================================
void UMontageImpactComponent::OnSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!OtherActor || !bIsActive) return;
	if (ShouldIgnoreActor(OtherActor)) return;

	// Cooldown check
	TWeakObjectPtr<AActor> WeakActor(OtherActor);
	if (HitTimestamps.Contains(WeakActor)) return;

	// Record hit
	const double Now = GetWorld()->GetTimeSeconds();
	if (HitCooldown > 0.0f)
	{
		HitTimestamps.Add(WeakActor, Now);
	}

	// Build HitResult
	FHitResult HitResult;
	HitResult.HitObjectHandle = FActorInstanceHandle(OtherActor);
	HitResult.Component = OtherComp;
	HitResult.ImpactPoint = ImpactSphere ? ImpactSphere->GetComponentLocation() : FVector::ZeroVector;
	HitResult.Location = HitResult.ImpactPoint;
	HitResult.bBlockingHit = true;

	OnMontageImpactHit.Broadcast(FastestBoneName, FastestBoneSpeed, HitResult);

	if (bDebugDraw)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta,
			FString::Printf(TEXT("[AMI] HIT: %s (Bone: %s, Speed: %.0f)"),
				*OtherActor->GetName(), *FastestBoneName.ToString(), FastestBoneSpeed));
	}
}

// =====================================================================
// Ignore logic
// =====================================================================
bool UMontageImpactComponent::ShouldIgnoreActor(AActor* Actor) const
{
	AActor* Owner = GetOwner();
	if (!Owner || !Actor) return true;
	if (Actor == Owner) return true;
	if (Actor->GetOwner() == Owner) return true;

	// Ignore attached actors (weapons, etc.)
	TArray<AActor*> AttachedActors;
	Owner->GetAttachedActors(AttachedActors);
	return AttachedActors.Contains(Actor);
}

// =====================================================================
// Cooldown cleanup
// =====================================================================
void UMontageImpactComponent::CleanupExpiredCooldowns()
{
	if (HitCooldown <= 0.0f) return;

	const double Now = GetWorld()->GetTimeSeconds();
	for (auto It = HitTimestamps.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid() || (Now - It->Value) > HitCooldown)
		{
			It.RemoveCurrent();
		}
	}
}
