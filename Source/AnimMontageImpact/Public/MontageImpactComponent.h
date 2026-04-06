#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MontageImpactComponent.generated.h"

class USphereComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnMontageImpactHit,
	FName, BoneName,
	float, BoneSpeed,
	const FHitResult&, HitResult);

/**
 * AnimMontage再生中にボーン速度を監視し、閾値を超えたボーンの位置に
 * Sphereコリジョンを自動生成してヒットイベントを発火するコンポーネント。
 *
 * 使い方:
 *   1. アクターにこのコンポーネントをアタッチ
 *   2. VelocityThreshold / CollisionRadius を調整
 *   3. OnMontageImpactHit デリゲートをバインド
 *   4. モンタージュを再生すると自動でコリジョンが生成される
 */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class ANIMMONTAGEIMPACT_API UMontageImpactComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMontageImpactComponent();

	// --- Events ---

	/** 生成されたコリジョンがアクターにヒットした時に発火 */
	UPROPERTY(BlueprintAssignable, Category = "Montage Impact")
	FOnMontageImpactHit OnMontageImpactHit;

	// --- Settings ---

	/** 攻撃コリジョンを生成するボーン速度の閾値 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact", meta = (ClampMin = "0"))
	float VelocityThreshold = 500.0f;

	/** 生成されるSphereコリジョンの半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact", meta = (ClampMin = "1"))
	float CollisionRadius = 50.0f;

	/** 同一アクターへの連続ヒットを防ぐクールダウン (秒)。0 でモンタージュ終了までクリアされない */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact", meta = (ClampMin = "0"))
	float HitCooldown = 0.5f;

	/** コリジョンのプロファイル名。デフォルトはOverlapAllDynamic */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact")
	FName CollisionProfileName = FName(TEXT("OverlapAllDynamic"));

	/** デバッグ描画を有効にする */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact|Debug")
	bool bDebugDraw = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 現在のコリジョンSphereを取得 (非アクティブ時はnullptr) */
	UFUNCTION(BlueprintCallable, Category = "Montage Impact")
	USphereComponent* GetActiveCollision() const { return ImpactSphere; }

	/** 現在攻撃判定がアクティブかどうか */
	UFUNCTION(BlueprintCallable, Category = "Montage Impact")
	bool IsImpactActive() const { return bIsActive; }

private:
	/** 前フレームのボーン位置マップ */
	TMap<FName, FVector> PreviousBoneLocations;

	/** ヒット済みアクター → 時刻 */
	TMap<TWeakObjectPtr<AActor>, double> HitTimestamps;

	/** 動的に生成するSphereコリジョン */
	UPROPERTY()
	TObjectPtr<USphereComponent> ImpactSphere;

	/** 判定アクティブフラグ */
	bool bIsActive = false;

	/** 前フレームでモンタージュ再生中だったか */
	bool bWasMontagePlayingLastFrame = false;

	/** 最速ボーン名 (デバッグ/イベント用) */
	FName FastestBoneName;

	/** 最速ボーン速度 */
	float FastestBoneSpeed = 0.0f;

	/** Owner の SkeletalMeshComponent を探索 */
	USkeletalMeshComponent* FindOwnerSkeletalMesh() const;

	/** 判定開始 */
	void ActivateImpact();

	/** 判定終了 */
	void DeactivateImpact();

	/** Sphere を最速ボーン位置に配置して overlap チェック */
	void UpdateImpactSphere(const FVector& Location);

	/** Overlap コールバック */
	UFUNCTION()
	void OnSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	/** 無視すべきアクターか判定 */
	bool ShouldIgnoreActor(AActor* Actor) const;

	/** クールダウン管理 */
	void CleanupExpiredCooldowns();
};
