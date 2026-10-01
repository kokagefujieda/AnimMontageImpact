#include "AnimNotifyState_MontageImpact.h"
#include "MontageImpactComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	UMontageImpactComponent* FindImpactComponent(const USkeletalMeshComponent* MeshComp)
	{
		const AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		return Owner ? Owner->FindComponentByClass<UMontageImpactComponent>() : nullptr;
	}
}

void UAnimNotifyState_MontageImpact::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UMontageImpactComponent* Impact = FindImpactComponent(MeshComp))
	{
		Impact->BeginDetectionWindow();
	}
}

void UAnimNotifyState_MontageImpact::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UMontageImpactComponent* Impact = FindImpactComponent(MeshComp))
	{
		Impact->EndDetectionWindow();
	}
}

FString UAnimNotifyState_MontageImpact::GetNotifyName_Implementation() const
{
	return TEXT("Montage Impact Window");
}
