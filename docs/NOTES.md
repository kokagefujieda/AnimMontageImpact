# 開発メモ（決定事項・没案・後で考えること）

## 決定事項

- 2026-10-01: 既存 BP 互換のため、既存プロパティ名と `OnMontageImpactHit` の3引数シグネチャは変更しない。
- 2026-10-01: 本プラグインの差別化点は「ボーン速度から自動判定・設定不要」。ソケット手動配置方式にはしない。
- 2026-10-01: 判定窓にはエンジン標準 `UAnimNotifyState` を使う（独自機構は作らない）。
- 2026-10-01: UE 環境がないためコンパイル検証不可。Phase ごとにユーザーが実ビルドで確認する。

## 没案・見送り

- ソケット間トレースへの全面移行 → 既存品（Melee Trace 等）と同質になるため見送り。
- GitHub Actions での UE ビルド CI → Epic のコンテナ/ライセンスが必要で現状は過剰。後日検討。

## 後で考えること

- Mover の `PlayMoverMontage` 対応は未検証（README の記載の裏取りが必要）。
- Automation Spec（自動テスト）の追加。UE 環境で実行できる時に。
- Fab 提出用の要件確認（Icon128.png、EngineVersion、対応プラットフォーム）。
- 床/壁ヒット（B4）が実際に起きるかの実機確認。
- レプリケーション対応の深さ（ヒット結果のマルチキャスト等）。

## 参考（既存の類似品）

- [Melee Trace (Fab)](https://www.fab.com/listings/62565d62-8067-4c34-a3b2-19f74ef6f857)
- [Stable Hit Detection (Fab)](https://www.fab.com/listings/fb3f7273-5f50-4660-9768-c666937d7f6f)
- [Accurate Animation-Based C++ Melee Tracing in UE5](https://www.remyjaspers.com/blog/melee_tracing_ue5/)
- [USkinnedMeshComponent::GetBoneTransform](https://docs.unrealengine.com/5.2/en-US/API/Runtime/Engine/Components/USkinnedMeshComponent/GetBoneTransform/2/)
