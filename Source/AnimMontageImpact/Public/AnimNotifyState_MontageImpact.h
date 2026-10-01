#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_MontageImpact.generated.h"

/**
 * モンタージュ内の攻撃判定区間 (判定窓) を指定する Notify State。
 * Owner の UMontageImpactComponent の bRequireNotifyWindow が ON の場合、
 * この区間の間だけボーン速度による判定が行われる。
 */
UCLASS(meta = (DisplayName = "Montage Impact Window"))
class ANIMMONTAGEIMPACT_API UAnimNotifyState_MontageImpact : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
