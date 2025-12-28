# PoolManager テスト計画

対象: `orteaf/include/orteaf/internal/base/manager/pool_manager.h`

## 進め方
- まずは副作用が少ない単体チェックから着手する
- 依存が増える順に段階的に拡張する
- 失敗系（例外）を先に固定してから成功系を増やす

## 優先順（段階）
1. **設定状態**
   - `isConfigured()` の初期値
   - `ensureConfigured()` の未設定時例外
2. **Config バリデーション**
   - `control_block_block_size == 0`
   - `control_block_growth_chunk_size == 0`
   - `payload_growth_chunk_size == 0`
   - `payload_capacity != 0 && payload_block_size == 0`
3. **Block size setter のガード**
   - `setControlBlockBlockSize(0)` 例外
   - `setPayloadBlockSize(0)` 例外
   - サイズ変更時の `canShutdown` / `canTeardown` チェック
4. **Growth chunk size setter のガード**
   - `setControlBlockGrowthChunkSize(0)` 例外
   - `setPayloadGrowthChunkSize(0)` 例外
5. **Payload 系の軽い操作**
   - `isAlive()` の判定
   - `reserveUncreatedPayloadOrGrow()`
   - `acquirePayloadOrGrowAndCreate()`
6. **Shutdown 系**
   - `shutdown()` が active lease を検知して例外
7. **Lease 取得（統合）**
   - `acquireWeakLease()` / `acquireStrongLease()` の再利用・バインド
   - invalid handle / 未作成 payload / nullptr payload の例外

## 事前準備（必要なら）
- テスト用の最小 `Traits` / `PayloadPool` / `ControlBlock` を用意
- `#if ORTEAF_ENABLE_TEST` の `*ForTest()` を活用可能か確認

