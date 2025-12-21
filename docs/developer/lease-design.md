# Lease 設計（ドラフト）

このドキュメントは、リースの生成・利用・解放の設計方針をまとめる。
manager は lease の入出力のみを担当し、lease は control block に直接
アクセスできることを前提とする。

## 目的

- lease を毎回生成する（軽量ハンドルとして扱う）。
- manager を経由せず、lease から control block に直接アクセスする。
- control block の handle を lease に保持させ、lock/解放の可否判断に使う。
- strong/weak の2種を基礎に、weak から strong を発行できるようにする。

## 基本関係

- manager は acquire/release の入口であり、lease の生成と返却を行う。
- lease は control block の pointer と handle に加えて control block pool pointer を保持する。
- control block は参照カウントと payload 解放の責務を持つ。
- control block は payload の handle / pointer / pool pointer を保持する。

## Handle 構成

- `ControlBlockHandle` を新設する。
- lease は `ControlBlockHandle` と `ControlBlock*` を保持する。
- payload handle は lease に保持しない。
- handle は stale 判定（generation）に使う。

## Lease 種別

- **StrongLease**
  - control block の strong count を保持する。
  - 破棄時に strong count を減算する。
- **WeakLease**
  - control block の weak count を保持する。
  - `lock()` により StrongLease の発行を試みる。

## Acquire フロー

1. 初回 acquire は manager が control block を pool から取得する。
2. 必要に応じて payload も確保し、control block に bind する。
3. strong lease を生成して返却する。
4. 2回目以降の取得は、原則として既存 lease から行う:
   - weak lock による strong lease の発行
   - strong copy による参照の複製
   - handle を用いた再取得（必要な manager API を用意）

## Release フロー

1. strong/weak lease が破棄される。
2. control block の release を呼び、該当カウントを減算する。
3. release が true を返した場合、所有権の破棄が完了したと見なす。
4. control block の `canShutdown()` が true なら、control block pool に返却する。

補足:
- control block の再利用は manager ではなく pool が行う。
- lease は control block pool pointer を保持し、返却先に直接アクセスする。

## Weak -> Strong (Lock)

- weak lease は `lock()` を持つ。
- `lock()` は control block の strong count を増加できる場合のみ成功する。
- 成功時は StrongLease を返し、失敗時は無効 lease を返す。

## エラーハンドリング方針

- `try*` 系は `bool` を返す。
- `*` ラッパは `try*` を呼び、失敗時に `throw` する。
- `release` は二重解放を検出しても例外は投げず、`bool` で失敗を返す。

## テスト指針（先に書くもの）

- acquire で strong lease が作られる。
- strong lease のコピー/破棄で strong count が正しく変化する。
- weak lease から lock できる。
- stale handle の weak lock は失敗する。
- strong/weak の両方が 0 になったときに control block が pool に戻る。
