# Anim Montage Impact

**Anim Montage Impact** は Unreal Engine 5 向けのボーン速度ベース攻撃判定プラグインです。
AnimMontage 再生中にボーンの速度を計測し、閾値を超えたボーンで当たり判定を行ってヒットイベントを発火します。

**MIT ライセンスで無料配布しています。**

## 特徴

- AnimMontage 再生を自動検出し、判定を ON/OFF
- 最速ボーンの位置で **Sphere 判定を自動実行**（ソケットの手動配置は不要）
- **Sweep 判定**（前フレーム→現在位置）で高速な攻撃でも素通りしない
- アクターの移動・回転を除いた**メッシュ基準の速度**で計測（走るだけでは誤ヒットしない）
- 複数ボーン同時判定、ボーンの Include/Exclude、ボーン別半径
- `Montage Impact Window`（AnimNotifyState）で攻撃区間だけ判定（任意）
- `OnMontageImpactHit` デリゲートでヒットイベントを取得（`HitResult` に衝突点・法線を設定）
- PlayMontage / Mover の PlayMoverMontage 両対応（Mover は未検証）

## 動作環境

- Unreal Engine 5.5 / 5.6 / 5.7
- プラットフォーム: Win64

## インストール

1. このページの [Releases](https://github.com/kokagefujieda/AnimMontageImpact/releases) から UE バージョンに合った zip をダウンロード
2. `AnimMontageImpact` フォルダをプロジェクトの `Plugins/` にコピー
3. UE エディタを起動し、プラグインを有効化
4. キャラクター BP に **Montage Impact** コンポーネントを追加
5. `OnMontageImpactHit` イベントをバインド

## クイックスタート

```
1. Add Component → Montage Impact
2. VelocityThreshold / CollisionRadius を調整
3. bDebugDraw = true で可視化確認
4. OnMontageImpactHit をバインドしてヒット処理を実装
```

## 主な設定

| プロパティ | 説明 |
|---|---|
| `DetectionMode` | `Sweep`（推奨）/ `Overlap`（v1.0 互換。Sphere コンポーネントを生成） |
| `VelocityThreshold` | 判定を発生させるボーン速度 (cm/s) |
| `bIgnoreOwnerMovement` | ON: メッシュ基準で計測 / OFF: ワールド空間で計測（v1.0 の挙動） |
| `CollisionRadius` | 判定 Sphere の半径 |
| `HitCooldown` | 同一アクターの再ヒット間隔 (秒)。0 = 判定が終わるまで再ヒットしない |
| `HitObjectTypes` | ヒット対象のオブジェクトタイプ。空 = 全タイプ。デフォルトは WorldStatic を除く |
| `MaxSimultaneousBones` | 同時に判定するボーン数（Sweep のみ） |
| `IncludeBones` / `ExcludeBones` | 対象ボーンの絞り込み（子孫ボーンにも適用） |
| `BoneRadiusOverrides` | ボーン別の判定半径 |
| `bRequireNotifyWindow` | ON: `Montage Impact Window` 区間でのみ判定 |

## 判定窓を使う（任意）

1. モンタージュの Notifies トラックを右クリック → **Add Notify State → Montage Impact Window**
2. 攻撃が当たる区間に長さを合わせる
3. コンポーネントの `bRequireNotifyWindow` を ON にする

## 注意

- **専用サーバー**ではメッシュが非表示などの条件でボーンが更新されない場合があります。サーバーでも判定する場合は、メッシュの `Visibility Based Anim Tick Option` を `Always Tick Pose and Refresh Bones` にしてください。
- ヒットイベントは、アニメーションが評価されている全てのマシンで発火します（サーバー限定の制御はありません）。
- v1.0 から更新する場合の挙動の変更は [CHANGELOG](CHANGELOG.md) を参照してください。

## ライセンス

[MIT License](LICENSE)
