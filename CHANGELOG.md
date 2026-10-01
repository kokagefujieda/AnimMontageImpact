# Changelog

## 1.1

### 挙動の変更（既存プロジェクトは要確認）

- `DetectionMode` を追加。デフォルトは **Sweep**（v1.0 と同じ挙動にするには `Overlap` を選択）
- `bIgnoreOwnerMovement` を追加。デフォルト **ON**（アクターの移動・回転・テレポートを速度から除外）。v1.0 と同じ挙動にするには OFF
- `HitObjectTypes` を追加。デフォルトは Pawn / PhysicsBody / WorldDynamic / Destructible（床・壁の WorldStatic は対象外）
- `HitCooldown = 0` を仕様通り「判定が終わるまで再ヒットしない」に修正（v1.0 は無制限にヒットしていた）
- 再生が途切れず別のモンタージュに切り替わった場合、新しい攻撃として扱いヒット履歴をリセット
- コンポーネントの Tick を `TG_PostPhysics` に変更し、メッシュを前提 Tick に登録（1フレーム遅れのポーズを読まないように）

### 追加

- Sweep 判定（前フレーム位置→現在位置）。高速でも素通りせず、`HitResult` に衝突点・法線を設定
- 複数ボーン同時判定 `MaxSimultaneousBones`（Sweep のみ）
- ボーンフィルタ `IncludeBones` / `ExcludeBones` / `bFilterAppliesToChildBones`、ボーン別半径 `BoneRadiusOverrides`
- `UAnimNotifyState_MontageImpact`（Montage Impact Window）と `bRequireNotifyWindow`
- ログカテゴリ `LogMontageImpact`

### 修正

- ルートコンポーネントが無いアクターで Overlap モードがクラッシュする問題
- ボーン計測の負荷（ボーン名配列の毎フレーム確保・ソケット名検索）をインデックスキャッシュに変更
- `GEngine` の null 参照
- コンポーネント破棄時に生成 Sphere が残る問題
- `bDebugDraw` を実行中に変更しても Sphere 表示が更新されない問題
- クールダウン掃除が閾値超過時にしか行われない問題（掃除自体を不要な設計に変更）

### プラグイン設定

- `.uplugin` から `"Installed": true` を削除
- Build.cs: `CoreUObject` / `Engine` を Public 依存に変更
- `.gitignore` を追加

## 1.0

- 初回リリース
