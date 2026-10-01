#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "MontageImpactComponent.generated.h"

class UAnimMontage;
class USphereComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnMontageImpactHit,
	FName, BoneName,
	float, BoneSpeed,
	const FHitResult&, HitResult);

/** 当たり判定の方式 */
UENUM(BlueprintType)
enum class EMontageImpactDetectionMode : uint8
{
	/** Sphere コンポーネントを最速ボーンへ移動し、重なっているコンポーネントを判定 (v1.0 互換) */
	Overlap UMETA(DisplayName = "Overlap (Sphere Component)"),

	/** 前フレーム位置 → 現在位置を Sphere でスイープ。高速でも素通りしない (推奨) */
	Sweep UMETA(DisplayName = "Sweep")
};

/** 速度計測の結果 (内部用) */
struct FMontageImpactBoneSample
{
	FName Name = NAME_None;
	FVector World = FVector::ZeroVector;
	FVector PrevWorld = FVector::ZeroVector;
	float Speed = 0.0f;
	float Radius = 0.0f;
};

/**
 * AnimMontage再生中にボーン速度を監視し、閾値を超えたボーンの位置で
 * 当たり判定を行ってヒットイベントを発火するコンポーネント。
 *
 * 使い方:
 *   1. アクターにこのコンポーネントをアタッチ
 *   2. VelocityThreshold / CollisionRadius を調整
 *   3. OnMontageImpactHit デリゲートをバインド
 *   4. モンタージュを再生すると自動で判定される
 *
 * 任意: モンタージュに "Montage Impact Window" (UAnimNotifyState_MontageImpact) を置き、
 *       bRequireNotifyWindow を ON にすると、その区間だけ判定される。
 */
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class ANIMMONTAGEIMPACT_API UMontageImpactComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMontageImpactComponent();

	// --- Events ---

	/** 判定がアクターにヒットした時に発火 */
	UPROPERTY(BlueprintAssignable, Category = "Montage Impact")
	FOnMontageImpactHit OnMontageImpactHit;

	// --- Settings ---

	/** 当たり判定の方式。Sweep 推奨 (Overlap は v1.0 互換) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact")
	EMontageImpactDetectionMode DetectionMode = EMontageImpactDetectionMode::Sweep;

	/** 判定を発生させるボーン速度の閾値 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact", meta = (ClampMin = "0"))
	float VelocityThreshold = 500.0f;

	/**
	 * ON: ボーン速度をメッシュ基準で計測する (アクターの移動・回転・テレポートを除外)。
	 * OFF: ワールド空間で計測する (v1.0 の挙動。走るだけで閾値を超えやすい)。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact")
	bool bIgnoreOwnerMovement = true;

	/** 判定に使う Sphere の半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact", meta = (ClampMin = "1"))
	float CollisionRadius = 50.0f;

	/**
	 * 同一アクターへの再ヒットを防ぐクールダウン (秒)。
	 * 0 の場合はモンタージュ終了まで (bRequireNotifyWindow が ON なら判定窓が閉じるまで) 再ヒットしない。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact", meta = (ClampMin = "0"))
	float HitCooldown = 0.5f;

	/** Overlap モードで生成する Sphere のコリジョンプロファイル名 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact",
		meta = (EditCondition = "DetectionMode == EMontageImpactDetectionMode::Overlap"))
	FName CollisionProfileName = FName(TEXT("OverlapAllDynamic"));

	/**
	 * ヒット対象のオブジェクトタイプ。空の場合は全タイプが対象。
	 * デフォルトは Pawn / PhysicsBody / WorldDynamic / Destructible (床・壁などの WorldStatic は対象外)。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact|Filter")
	TArray<TEnumAsByte<EObjectTypeQuery>> HitObjectTypes;

	// --- Bones ---

	/** 同時に判定する最速ボーンの最大数 (Sweep モードのみ。Overlap モードは常に 1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact|Bones",
		meta = (ClampMin = "1", ClampMax = "8",
			EditCondition = "DetectionMode == EMontageImpactDetectionMode::Sweep"))
	int32 MaxSimultaneousBones = 1;

	/** 対象とするボーン。空の場合は全ボーン */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact|Bones")
	TArray<FName> IncludeBones;

	/** 対象から除外するボーン */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact|Bones")
	TArray<FName> ExcludeBones;

	/** ON: Include / Exclude の指定がそのボーンの子孫ボーンにも適用される */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact|Bones")
	bool bFilterAppliesToChildBones = true;

	/** ボーン別の判定半径。未指定のボーンは CollisionRadius を使用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact|Bones")
	TMap<FName, float> BoneRadiusOverrides;

	// --- Notify window ---

	/**
	 * ON: モンタージュ内の "Montage Impact Window" 区間でのみ判定する。
	 * 区間が無いモンタージュでは判定されない。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact")
	bool bRequireNotifyWindow = false;

	// --- Debug ---

	/** デバッグ描画を有効にする */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage Impact|Debug")
	bool bDebugDraw = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Overlap モードで生成済みの Sphere を取得 (未生成、または Sweep モードでは nullptr) */
	UFUNCTION(BlueprintCallable, Category = "Montage Impact")
	USphereComponent* GetActiveCollision() const { return ImpactSphere; }

	/** 現在攻撃判定がアクティブかどうか */
	UFUNCTION(BlueprintCallable, Category = "Montage Impact")
	bool IsImpactActive() const { return bIsActive && IsDetectionWindowOpen(); }

	/** 判定窓を開く (UAnimNotifyState_MontageImpact から呼ばれる。多重に開ける) */
	void BeginDetectionWindow();

	/** 判定窓を閉じる */
	void EndDetectionWindow();

private:
	/** 判定窓が開いているか (bRequireNotifyWindow が OFF なら常に true) */
	bool IsDetectionWindowOpen() const { return !bRequireNotifyWindow || NotifyWindowDepth > 0; }

	// --- State ---

	/** ヒット済みアクター → 時刻 */
	TMap<TWeakObjectPtr<AActor>, double> HitTimestamps;

	/** Overlap モードで動的に生成する Sphere コリジョン */
	UPROPERTY(Transient)
	TObjectPtr<USphereComponent> ImpactSphere;

	/** キャッシュ済みの Owner の SkeletalMeshComponent */
	TWeakObjectPtr<USkeletalMeshComponent> CachedMesh;

	/** 再生中のモンタージュ (切り替わり検知用) */
	TWeakObjectPtr<UAnimMontage> CurrentMontage;

	/** 計測対象ボーンのインデックス */
	TArray<int32> TrackedBoneIndices;

	/** 前フレームのボーン位置 (メッシュコンポーネント空間)。TrackedBoneIndices と同じ並び */
	TArray<FVector> PreviousBoneLocalLocations;

	/** 前フレームのメッシュコンポーネントのワールドトランスフォーム */
	FTransform PreviousMeshTransform;

	/** 計測時のメッシュのボーン数 (変化したらキャッシュ再構築) */
	int32 TrackedMeshBoneCount = 0;

	/** 開いている判定窓の数 */
	int32 NotifyWindowDepth = 0;

	/** 最速ボーン名 (デバッグ用) */
	FName FastestBoneName;

	/** 最速ボーン速度 (デバッグ用) */
	float FastestBoneSpeed = 0.0f;

	/** モンタージュ再生中か */
	bool bIsActive = false;

	/** 前フレームで判定中だったか */
	bool bWasDetecting = false;

	/** 前フレームのサンプルが有効か */
	bool bHasPreviousSample = false;

	/** ボーンキャッシュの再構築が必要か */
	bool bBoneCacheDirty = true;

	/** ルートが無い警告を出したか */
	bool bLoggedMissingRoot = false;

	// --- Helpers ---

	/** Owner の SkeletalMeshComponent を探索 */
	USkeletalMeshComponent* FindOwnerSkeletalMesh() const;

	/** キャッシュ済みのメッシュを返す。未取得なら探索し、Tick の前提条件に登録する */
	USkeletalMeshComponent* ResolveMeshComponent();

	/** モンタージュ再生開始 */
	void ActivateImpact(UAnimMontage* Montage);

	/** モンタージュ再生終了 */
	void DeactivateImpact();

	/** 速度計測の履歴を破棄 */
	void ResetTracking();

	/** 計測対象ボーンのキャッシュを再構築 */
	void RebuildBoneCache(const USkeletalMeshComponent& Mesh);

	/** Include / Exclude 設定を満たすボーンか */
	bool PassesBoneFilter(const USkeletalMeshComponent& Mesh, FName BoneName) const;

	/** ボーンに適用する判定半径 */
	float GetRadiusForBone(FName BoneName) const;

	/** Overlap モード用 Sphere を生成 (ルートが無ければ false) */
	bool EnsureImpactSphere();

	/** Sphere を無効化して隠す */
	void HideImpactSphere();

	/** Sphere を移動して有効化 */
	void UpdateImpactSphere(const FVector& Location, float Radius);

	/** Overlap モード: Sphere に重なっているコンポーネントを処理 */
	void ProcessSphereOverlaps(const FMontageImpactBoneSample& Bone, const TArray<AActor*>& AttachedActors);

	/** Sweep モード: 前フレーム位置 → 現在位置をスイープして処理 */
	void SweepBone(const FMontageImpactBoneSample& Bone, const TArray<AActor*>& AttachedActors);

	/** HitObjectTypes からスイープ用クエリを構築 */
	FCollisionObjectQueryParams BuildObjectQueryParams() const;

	/** Overlap モード用: コンポーネントが HitObjectTypes に含まれるか */
	bool PassesObjectFilter(const UPrimitiveComponent* Component) const;

	/** 無視すべきアクターか判定 */
	bool ShouldIgnoreActor(const AActor* Actor, const TArray<AActor*>& AttachedActors) const;

	/** クールダウン中 (または判定窓内で既にヒット済み) か */
	bool IsOnCooldown(AActor* Actor) const;

	/** ヒットを記録してイベントを発火 */
	void HandleHit(AActor* HitActor, const FHitResult& Hit, const FMontageImpactBoneSample& Bone);

	/** 無効なヒット履歴を掃除 */
	void PruneHitHistory();
};
