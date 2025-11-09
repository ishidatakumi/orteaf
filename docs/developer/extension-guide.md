# Extension Guide

開発者が新しい演算やテンソル表現を追加するときの導線をまとめる予定の文書です。実装が固まり次第、`Kernel` / `Ops` / `TensorImpl` / `ModuleImpl` の実例とベストプラクティスを追記してください。

- `include/orteaf/extension/kernel/` : カーネルの抽象クラス
- `src/extension/kernel/<backend>/impl/` : CUDA/MPS などバックエンド固有のカーネル実装。CMake が自動で検出し、埋め込みバイナリを生成する。
- `include/orteaf/extension/tensor/` : `TensorImpl`・`Parameter` 実装
- `include/orteaf/extension/module/` : `ModuleImpl` や標準レイヤ群

> **メモ**: 実際の API が固まるまではドキュメントを軽めに保ち、改定しやすい状態を維持してください。
