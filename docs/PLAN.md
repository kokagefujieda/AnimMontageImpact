# AnimMontageImpact 全体デバッグ・機能追加 計画書

日付: 2026-10-01 / 対象: v1.0 (`08cc7dd`) / ブランチ: `claude/quirky-franklin-53uz96`

## 0. 前提・制約

- この環境には UE も UE ヘッダーもない。**コンパイル・実行検証は不可**。
- 方針: 変更は小さなコミットに分け、各フェーズ終了時にユーザー側で UE エディタ/ビルドして確認する。
- 互換性: 既存プロパティ名・`OnMontageImpactHit` の3引数シグネチャは維持（既存 BP を壊さない）。

## 1. 不具合・問題点（調査結果）

行番号は `MontageImpactComponent.cpp` / `.h` のもの。重大度: 高 / 中 / 低。

| # | 重大度 | 内容 | 場所 |
|---|---|---|---|
| B1 | 高 | ボーン速度がワールド空間。キャラが走る/テレポートするだけで全ボーンが閾値超え → 誤ヒット | cpp:70-76 |
| B2 | 高 | `HitCooldown=0` が「モンタージュ終了まで再ヒットなし」の仕様なのに、0 だと履歴を記録せず無制限にヒットする（ドキュメントと逆） | cpp:226-229, h:50 |
| B3 | 高 | ルートコンポーネントが null のアクターで `AttachToComponent(nullptr)` → クラッシュ | cpp:156 |
| B4 | 高 | 床・壁など WorldStatic にもヒットする可能性（`OverlapAllDynamic`、フィルタなし）※実機確認要 | cpp:148-150 |
| B5 | 中 | 毎フレーム `GetBoneNames`(配列確保) + `GetSocketLocation(FName)`(ソケット線形探索→ボーン検索) + `TMap` 更新。ボーン数が多いと重い | cpp:61-84 |
| B6 | 中 | 判定がスフィア移動後の BeginOverlap のみ。高速時に素通り(トンネリング)。滞留中のアクターはクールダウン後も再ヒットしない（クールダウンが意味をなさない） | cpp:195-204, 209-247 |
| B7 | 中 | `HitResult` が不正確（ImpactPoint=ボーン位置、Normal/BoneName 未設定） | cpp:231-237 |
| B8 | 中 | コンポーネントの Tick 順が未指定 → 1フレーム遅れのポーズを読む可能性。専用サーバーはメッシュ非表示時などにボーンが更新されない場合がある | cpp:10-14 |
| B9 | 中 | `IsAnyMontagePlaying` のみで判定 → 無関係なモンタージュ(リアクション等)でも発動。同一フレームでの別モンタージュ切替は検知不能 | cpp:41-54 |
| B10 | 中 | サーバー/クライアント両方でヒットイベントが発火（権限制御なし） | 全体 |
| B11 | 低 | `FindOwnerSkeletalMesh` を毎フレーム実行（キャッシュなし） | cpp:36 |
| B12 | 低 | `GEngine` の null チェックなし（5箇所） | cpp:98,111,170,188,243 |
| B13 | 低 | `EndPlay`/コンポーネント破棄時に `ImpactSphere` を破棄せず、デリゲートも解除しない | cpp:21-25 |
| B14 | 低 | `CleanupExpiredCooldowns` が閾値超え時のみ実行 | cpp:106 |
| B15 | 低 | `GetActiveCollision`/`IsImpactActive` が `BlueprintCallable`（const getter は `BlueprintPure` が適切） | h:70-75 |
| B16 | 低 | `FastestBoneLocation.IsZero()` で「未検出」を判定（原点のボーンで誤動作） | cpp:87 |
| B17 | 低 | `bDebugDraw` を途中変更すると `SetHiddenInGame` が更新されない | cpp:153 |
| P1 | 中 | `.uplugin` に `"Installed": true`（ソース配布では付けない値） | uplugin:17 |
| P2 | 中 | `PlatformAllowList` が Win64 のみ。README に記載なし | uplugin:23-25 |
| P3 | 低 | Build.cs: 公開ヘッダーが Engine/CoreUObject 型を使うのに Private 依存 | Build.cs:9-22 |
| P4 | 低 | `.gitignore`・`Resources/Icon128.png`・CHANGELOG なし | — |
| P5 | 低 | README の「Mover の PlayMoverMontage 両対応」は未検証（`IsAnyMontagePlaying` が同じ AnimInstance 上で動く前提） | README |

## 2. 既存の類似品（車輪の再発明チェック）

- 既存: Fab の Melee Trace / Stable Hit Detection、AnimNotifyState + ソケット間トレース方式（多数の解説あり）。
- いずれも「ソケットを手動配置して判定窓を指定」する方式。**本プラグインの差別化 =「ボーン速度から自動で判定、設定不要」**。この特徴は維持する。
- 判定窓には新規の独自機構を作らず、エンジン標準の `UAnimNotifyState` を継承する（採用する場合）。
- トレース/オーバーラップはエンジン標準の `UWorld::Sweep*` / `Overlap*` を使う（独自の衝突処理は書かない）。

## 3. 実施フェーズ

### Phase 1: バグ修正（互換維持）
B2, B3, B5, B8, B11, B12, B13, B14, B15, B16, B17, P1, P3, P4 を修正。
- ボーンインデックスのキャッシュ + `TArray` 化 + `GetBoneTransform(int32)` 利用
- メッシュ参照キャッシュ、Tick を `TG_PostPhysics` + メッシュを前提 Tick に
- 破棄処理、null ガード、`UE_LOG` 用カテゴリ `LogMontageImpact` 追加

### Phase 2: 判定ロジックの修正（挙動が変わる）
B1, B4, B6, B7, B9, B10。
- 相対速度モード（オーナー移動分を除外）: `bIgnoreOwnerMovement`
- スイープ判定（前フレーム位置→現在位置）と正確な `HitResult`
- ヒットフィルタ（オブジェクトタイプ / 無視クラス / タグ）
- モンタージュのフィルタ（許可リスト）
- サーバー権限オプション `bServerOnly`

### Phase 3: 機能追加
- 複数ボーン同時判定（上位N本）＋ボーン Include/Exclude
- `UAnimNotifyState` による判定窓（任意）
- BP API: `SetImpactEnabled`, `ClearHitHistory`, `OnImpactStarted/Ended`
- ボーン別半径、デバッグ表示改善

### Phase 4: 仕上げ
README 更新（日英）、CHANGELOG、`Icon128.png`、uplugin 整理、バージョン 1.1。

## 4. 未決事項（ユーザー確認待ち）

1. 検出方式: 旧スフィア方式を残すか、スイープに置き換えるか
2. 対応プラットフォーム（Win64 限定の意図）
3. Phase 3 で実装する機能の範囲
4. 相対速度モードをデフォルト ON にしてよいか（既存利用者の挙動が変わる）

## 5. リスク

- コンパイル未検証のまま大量変更すると、エラーが一度に出る → Phase ごとにユーザー確認。
- UE 5.5/5.6/5.7 の API 差異（`GetBoneTransform` 等）は実ビルドで要確認。
