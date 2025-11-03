# Extension Guide

開発者が新しい演算やテンソル表現を追加するときの導線をまとめる予定の文書です。実装が固まり次第、`Kernel` / `Ops` / `TensorImpl` / `ModuleImpl` の実例とベストプラクティスを追記してください。

- `include/orteaf/extension/kernel/` : カーネルの抽象クラスとバックエンド固有実装を配置
- `include/orteaf/extension/ops/` : 演算登録とディスパッチャとの接合点
- `include/orteaf/extension/tensor/` : `TensorImpl`・`Parameter` 実装
- `include/orteaf/extension/module/` : `ModuleImpl` や標準レイヤ群

> **メモ**: 実際の API が固まるまではドキュメントを軽めに保ち、改定しやすい状態を維持してください。
