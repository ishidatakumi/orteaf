大きな目的として、shared ptr, unique ptrのcontrol blockを一箇所にまとめることで、再利用をすることができ、メモリを効率的に使用でき、かつ、キャッシュ効率を上げることができる。しかし、メモリの安全性を保つことも目的とする。
そのため、shutdownというメソッドには大きな制約を与えることで、メモリの安全性を保つことができる。

## Shutdownの制約 (Shutdown Constraints)

Poolの破棄(`shutdown()`)は、リソース管理における最も危険な操作の一つである。以下のルールを設けることを提案する：

## Shutdownの制約 (Shutdown Constraints)

Poolの破棄(`shutdown()`)は、リソース管理における最も危険な操作の一つである。以下のルールを設けることを提案する：

1.  **Safety Checkの分離**:
    *   **`canTeardown()`**: リソース(Payload)を破棄して良いか判定する。
        *   基準: **Strong Reference Count == 0** (Unique/Strong)
        *   用途: `release()` や `releaseAndDestroy()` 時の判定。
    *   **`canShutdown()`**: マネージャ(Pool)自体を破棄して良いか判定する。
        *   基準: **Total Reference Count (Strong + Weak) == 0**
        *   用途: `shutdown()` 時の判定。Weak Leaseが残っている場合もShutdownを拒否する。

2.  **Raw Control Blockの扱い**:
    *   Raw Control Block (Countを持たない単純なスロット) も、Managerの安全なShutdownのためには「使用中かどうか」以上の情報が必要になる可能性がある。
    *   提案: Raw Control Blockにも最小限の参照カウント(またはLease発行数)を持たせ、`canShutdown()` の判定に参加させる。

3.  **Shutdown挙動**:
    *   `shutdown()` 呼び出し時に `!canShutdown()` なControl Blockが一つでもあれば、**即座にAbort/例外**とする。
    *   これにより、ダングリングポインタ（Strong/Weak問わず）の発生を未然に防ぐ。

## Freelistへの返却ポリシー (Freelist Policy)

安全性と整合性を高めるため、以下のポリシーを採用する：

*   **完全な解放 (Full Release)**:
    *   SlotをFreelistに戻す（再利用可能にする）のは、**`canShutdown()` が true (Strong=0 AND Weak=0) になった時点**とする。
    *   つまり、**Weak Leaseが残っている間は、そのSlotは「墓標 (Tombstone)」として確保され続け、他の用途に再利用されない**。
    *   これにより、Weak Lease保有者は、自分の参照しているSlotが（Generationチェック以前に）別のリソースに使われることがないことを保証される。

## Control Blockの責務

*   `release()` (Strong Release):
    *   Strong Countを減らす（0になればPayload破棄）。
    *   もし Weak Count == 0 ならば、`canShutdown()` 状態になり、ここで初めてFreelistに戻る。
    *   Weak Count > 0 ならば、戻らない（Zombie状態）。
*   `releaseWeak()` (Weak Release):
    *   Weak Countを減らす。
    *   もし Strong Count == 0 (既にPayload破棄済み) かつ Weak Count == 0 になれば、ここでFreelistに戻る。

## Strong Lockable Resource Model (CommandQueue)

CommandQueueのように「複数の場所で参照を持ちたい(Strong)が、操作時には排他制御したい(Lock)」ケースのために、**LockableSharedControlBlock** を導入する。

### 構造案 (LockableSharedControlBlock)

```cpp
class LockableSharedControlBlock : public SharedControlBlock<SlotT> {
public:
  // 追加: 排他ロック用フラグ
  std::atomic<bool> locked_{false};

  // Try to acquire exclusive lock
  bool tryLock() noexcept {
    bool expected = false;
    return locked_.compare_exchange_strong(expected, true, 
                                           std::memory_order_acquire, 
                                           std::memory_order_relaxed);
  }

  // Release lock
  void unlock() noexcept {
    locked_.store(false, std::memory_order_release);
  }

  // Shutdown Check overwrite
  bool canShutdown() const noexcept {
    // 参照が0 かつ ロックされていないこと
    return this->count() == 0 && !locked_.load(std::memory_order_acquire);
  }
};
```

### 安全なアクセス設計 (LockableSharedLease)

「ロックしていないのにリソースを使ってしまう」事故を防ぐため、専用のLeaseタイプ `LockableSharedLease` を導入し、アクセスの明示性を高める。

1.  **LockableSharedLease**:
    *   通常の `SharedLease` と異なり、**`->` 演算子による暗黙のアクセスを提供しない**。
    *   リソースに触れるには、以下のいずれかのメソッドを明示的に呼ぶ必要がある。

2.  **アクセスパターンの分離**:

    *   **A. 排他アクセス (Exclusive / Locked)**:
        *   `lock()` / `tryLock()` を使用。
        *   `LockableSharedControlBlock` のロックフラグを立てる。
        *   戻り値: `ScopedLock` (Payloadへのアクセサを持つ RAII オブジェクト)。
        *   用途: CommandBufferの送信など、競合してはいけない操作。

    *   **B. 同時アクセス (Concurrent / Unsafe)**:
        *   `getValues()` / `accessConcurrent()` などを提供（名前は要検討）。
        *   ロックを取得せず、直接 Payload への参照を返す。
        *   **明示的なメソッド呼び出し**を要求することで、「うっかりロック忘れ」を防ぐ。
        *   用途: スレッドセーフなメソッドの呼び出し、読み取り専用アクセスなど。

```cpp
auto lease = manager.acquire(); // LockableSharedLease

// NG: 暗黙アクセスは禁止
// lease->send(); // Compile Error!

// OK: 排他ロック
if (auto locked = lease.lock()) { // Blocks or Spin
    locked->send_command(); 
}

// OK: 同時書き込み許可 (明示的)
// ユーザーがスレッドセーフであることを理解して使う
lease.accessConcurrent()->thread_safe_method();
```

これなら、「ロックするべき箇所」と「しなくていい箇所」をコード上で明確に区別できる。

