# 開発メモ（決定事項・没案・後で考えること）

## 決定事項

- 2026-10-01: 既存 BP 互換のため、既存プロパティ名と `OnMontageImpactHit` の3引数シグネチャは変更しない。
- 2026-10-01: 本プラグインの差別化点は「ボーン速度から自動判定・設定不要」。ソケット手動配置方式にはしない。
- 2026-10-01: 判定窓にはエンジン標準 `UAnimNotifyState` を使う（独自機構は作らない）。
- 2026-10-01: UE 環境がないためコンパイル検証不可。ユーザーが実ビルドで確認する。
- 2026-10-01 ユーザー回答:
  - 検出方式: **スイープ追加・旧スフィア方式も残す**（`DetectionMode`）。デフォルトは Sweep。
  - 相対速度モード: **デフォルト ON**（`bIgnoreOwnerMovement`）。
  - プラットフォーム: **Win64 のまま維持**。
  - 追加機能: **複数ボーン同時判定＋ボーンフィルタ**、**AnimNotifyState の判定窓** を実装。
- Overlap モードは 1 ボーンのみ（複数ボーンは Sweep 限定）。Sphere を複数生成するプール化は見送り。
- Overlap モードも毎フレーム `GetOverlappingComponents` をポーリング。BeginOverlap デリゲートは使わない（クールダウンを正しく効かせるため）。
- ヒット対象は `HitObjectTypes`（デフォルトで WorldStatic を除外）で絞る。床・壁ヒット(B4)対策。
- 速度計測は `GetBoneTransform(index, Identity)`（メッシュ空間）で取得し、相対モードはメッシュ空間の変位をスケール込みで換算。
- `HitCooldown = 0` の意味: 判定が終わる（モンタージュ終了 / 判定窓が閉じる）まで再ヒットしない。
- 連続して別モンタージュへ切り替わった場合（`GetCurrentActiveMontage` が変化）は新しい攻撃としてヒット履歴をリセット。

## 没案・見送り

- ソケット間トレースへの全面移行 → 既存品（Melee Trace 等）と同質になるため見送り。
- GitHub Actions での UE ビルド CI → Epic のコンテナ/ライセンスが必要で現状は過剰。
- B15（`GetActiveCollision` 等を `BlueprintPure` 化）→ 既存 BP の実行ピンが外れて壊れるため据え置き。
- 接触点の精密化に `GetClosestPointOnCollision` を使う案 → トライメッシュで警告ログが出る恐れがあり見送り。Overlap モードは従来通りボーン位置を ImpactPoint とする。
- ユーザーが今回選ばなかった機能（実装していない）:
  - モンタージュ許可リスト（B9 の一部）
  - サーバー権限のみ発火 `bServerOnly`（B10）
  - BP API 拡張（`SetImpactEnabled` / `ClearHitHistory` / `OnImpactStarted/Ended`）
  - ヒットフィルタのうち、無視クラス・タグ（オブジェクトタイプのみ実装）

## 後で考えること

- **実ビルド確認が必要な API**（UE 5.5〜5.7 で差異があり得る）:
  - `USkinnedMeshComponent::GetBoneTransform(int32, const FTransform&)` の公開範囲
  - `USkinnedMeshComponent::BoneIsChildOf`
  - `UAnimNotifyState::NotifyBegin/NotifyEnd`（`FAnimNotifyEventReference` 付き）の署名
  - `FCollisionObjectQueryParams::AllObjects`
- Mover の `PlayMoverMontage` 対応は未検証（README に「未検証」と明記）。
- Automation Spec（自動テスト）の追加。UE 環境で実行できる時に。
- Fab 提出用の要件確認（`Resources/Icon128.png`、`EngineVersion`、対応プラットフォーム）。
- README の英語版。
- レプリケーション対応の深さ（ヒット結果のマルチキャスト等）。
- 複数モンタージュ同時再生時に `GetCurrentActiveMontage` が入れ替わるとヒット履歴がリセットされる可能性（スロット違いの併用）。問題が出たら montage 判定を見直す。

## 参考（既存の類似品）

- [Melee Trace (Fab)](https://www.fab.com/listings/62565d62-8067-4c34-a3b2-19f74ef6f857)
- [Stable Hit Detection (Fab)](https://www.fab.com/listings/fb3f7273-5f50-4660-9768-c666937d7f6f)
- [Accurate Animation-Based C++ Melee Tracing in UE5](https://www.remyjaspers.com/blog/melee_tracing_ue5/)
- [USkinnedMeshComponent::GetBoneTransform](https://docs.unrealengine.com/5.2/en-US/API/Runtime/Engine/Components/USkinnedMeshComponent/GetBoneTransform/2/)
