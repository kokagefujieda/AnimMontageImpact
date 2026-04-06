# Anim Montage Impact

**Anim Montage Impact** は Unreal Engine 5 向けのボーン速度ベース攻撃判定プラグインです。
AnimMontage 再生中にボーンの速度を計測し、閾値を超えた最速ボーンの位置に Sphere コリジョンを自動生成してヒットイベントを発火します。

**MIT ライセンスで無料配布しています。**

## 特徴

- AnimMontage 再生を自動検出し、判定を ON/OFF
- 最速ボーンの位置に **Sphere コリジョンを動的生成**
- `OnMontageImpactHit` デリゲートでヒットイベントを取得
- PlayMontage / Mover の PlayMoverMontage 両対応
- コリジョン設定不要 &mdash; アタッチするだけで動作

## 動作環境

- Unreal Engine 5.5 / 5.6 / 5.7

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

## ライセンス

[MIT License](LICENSE)
