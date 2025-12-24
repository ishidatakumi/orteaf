# ランタイム層アーキテクチャ計画

ランタイム層は **資源の所有と公開 API（getter/setter）に専念する管理レイヤ** として再構築する。従来 `bitsai` で提供されていた `ContextManager` や `StreamManager` などの「便利メソッド」は、`runtime manager` から切り離し、別レイヤ（仮称 _runtime ops_）で提供する。これにより、マネージャはライフサイクル管理のみに責任を持ち、上位レイヤからの利用が明確になる。

## 期待するディレクトリ構成

`execution/` は今後の実行時基盤を一元的に置くトップレベルディレクトリとし、以下のように用途別のサブディレクトリを切る。

```text
orteaf/
├── include/orteaf/internal/execution/
│   ├── manager/            # ランタイムマネージャ群（各 backend サブディレクトリ）
│   ├── allocator/          # ランタイム共通アロケータ（Backend 別特殊化をここに集約）
│   ├── context/            # Runtime Context / CurrentState 相当の公開インターフェース
│   └── ops/                # 便利機能（wait/signal 等）――マネージャ完成後に追加
└── src/internal/execution/
    ├── manager/
    ├── allocator/
    ├── context/
    └── ops/                # 実装/テスト完了後に配置
```

- `manager/` 直下には `cuda/`, `mps/`, `cpu/` などバックエンド名のサブディレクトリを作り、**そのバックエンドに必要なマネージャだけ** を実装する。
- `allocator/`・`context/` は runtime manager と同じレイヤの基盤機能として配置する。`ops/` から直接参照できる距離に置くことで、便利機能実装時に依存関係が循環しないようにする。

## ランタイムマネージャの設計原則

1. **役割は所有と公開に限定**  
   - デバイス/コンテキスト/ストリーム/イベントなど「リソースのライフサイクル管理」と「参照（getter）」のみ担当。
   - ドライバ API の呼び出し順序や同期プリミティブは ops 層へ移す。
2. **バックエンド専用クラスを明示**  
   - 例: `execution::cuda::DeviceManager`, `execution::cpu::DeviceManager` のように名前空間とファイルパスを一致させる。
3. **遅延初期化と明示的解放**  
   - `ensure_alive`/`release` を保持し、呼び出し側がタイミングを制御できるようにする。
4. **依存方向は manager → allocator/context のみ**  
   - ops や高レベル API から manager/allocator/context を参照するが、その逆は許可しない。

## バックエンドごとの必須マネージャ

| Backend | Device | Context | Stream | Event | 補足 |
| --- | --- | --- | --- | --- | --- |
| CPU  | ✅ 必須 | 省略可（現状は不要） | 省略可 | 省略可 | 単一デバイスでシリアル実行を想定。 |
| CUDA | ✅ 必須 | ✅ 必須 | ✅ 必須 | ✅ 必須 | 既存 `bitsai` の 4 マネージャを分割して配置。 |
| MPS  | ✅ 必須 | 省略（不要） | ✅ 必須 | ✅ 必須 | 現行設計ではコンテキスト切替が不要なため Device 管理のみで十分。 |

> 上表は「最小構成」を示す。将来 CPU に並列ストリームを導入した場合など、追加が必要になれば同じパス配下に増やす。

### 各マネージャの責務

- **DeviceManager**  
  - デバイス列挙と `DeviceState` の保持。`initialize_devices()` / `shutdown()` / `get_state()` を提供。
  - 各バックエンド固有のアーキ情報（SM 数や Metal Feature Set など）を保持する構造体もここに置く。
  - ID 系の誤用を防ぐため、`base/handle.h` で提供する `DeviceHandle`/`StreamHandle`/`ContextHandle` を API 引数／返り値に強制し、`uint8_t`/`uint32_t` をそのまま受け取らない。
- **ContextManager**  
  - CUDA などドライバが明示的なコンテキストを要求するバックエンド専用。  
  - `ContextState` に `StreamManager`/`EventManager`/`Allocator` のハンドルを内包し、遅延生成・破棄を管理する。
- **StreamManager / EventManager**  
  - 実リソースの確保・破棄とステート（世代/シリアル/プール統計）のみ。  
  - `wait_on` や `signal` といった同期操作は ops 層の `execution::stream::wait` に委譲する。

## Runtime Context / Allocator との関係

- Runtime Context（`execution/context/`）は **「現在のバックエンド・デバイス・マネージャへの参照」** を束ねる軽量オブジェクトとして再設計する。  
  - manager を直接触りたくない高レイヤ（extension/user）には Context 経由で提供する。
- Allocator は manager と同じレイヤに置き、Context や ops が参照する。  
  - 例えば CUDA のメモリプールは `execution/allocator/cuda/…`、CPU は `execution/allocator/cpu/…` のように分割する。

## 実装フェーズの推奨手順

1. **空ディレクトリとビルドターゲットの用意**  
   - `include/orteaf/internal/execution/{manager,allocator,context}` と `src/internal/execution/...` を先に作成し、CMake から参照できるようにする。
2. **マネージャ API の骨格定義**  
   - バックエンドごとにヘッダのみを配置し、`class DeviceManager final` などのインターフェースを宣言。  
   - 実装が未完成の関数には `TODO` コメントを残すか、`ORT_NOT_IMPLEMENTED()` のような仮実装を入れる。
3. **CUDA → MPS → CPU の順で実装**  
   - 依存数の多いバックエンドから仕上げることで、共通ユーティリティ（`DeviceState`, `ContextState` など）を先に整備できる。
4. **Runtime Context / Allocator の統合**  
   - 各マネージャが `Allocator` を所有するか、Context がワンストップで保持するかを決め、依存を解消する。
5. **ops レイヤ追加**  
   - manager の API が安定したら `execution/ops/stream_ops.*` などを追加し、`wait_on`, `signal`, `set_context` など便利メソッドを実装。
6. **テストと使用箇所の接続**  
   - `src/internal` や `extension` で manager API を呼び出すコードを順次置き換える。

## 今後の検討事項

- Manager 間で共有する小さなユーティリティ（`DeviceHandle`, `StreamHandle` など）を `execution/common/` のように切り出すかどうか。
- ops レイヤを `execution/ops` に置くか、`execution/manager/ops` としてサブディレクトリ化するか。  
  - 便利機能の利用頻度・API 表現を見ながら決める。
- Allocator を ops から直接呼ぶケース（例: 一時バッファ確保）に備えて、スレッドローカル Context から安全に取得できる仕組みを整える。

上記ドキュメントを基点に、実装中も構成や責務に変更が生じた場合は本書を更新し、`docs/README.md` からの導線を保守する。
