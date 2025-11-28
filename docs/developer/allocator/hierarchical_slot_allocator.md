# Hierarchical Slot Allocator 概要

バックエンド非依存で「連続した BufferView」を返す階層アロケータの設計メモ。Segregated Pool の下層で利用し、chunk 単位の高速確保（Fast）と、複数スロットを束ねる密な確保（Dense）を使い分ける。

## 役割分担

- **Storage**: Slot/Layer の状態、free/span-free リスト、levels/threshold 検証、`computeRequestSlots`。
- **SingleOps**: 単一スロットの確保/解放（Fast）。`acquireSpecificSlot` で指定スロットを直接確保可能。
- **DenseOps**: 複数スロットの連続確保/解放（Dense）。plan に従って決定的に InUse + map する。
- **Allocator ファサード**: 上記をまとめた API（Fast/Dense を切り替え）。将来テンプレ戦略で静的切り替え、動的選択ラッパも視野。
- **HeapOps (backend)**: reserve/map/unmap を提供。将来的に「連続範囲をまとめて map/unmap」できる拡張を想定。

## 2つの戦略

- **FastSingle**: size に合うレイヤを 1 スロットで確保。最頻ケースを超高速に処理。
- **Dense**: 毎回 rs を計算し、連続スロットを束ねて 1 BufferView を返す。末尾連続（Trail）を優先、見つからなければフォールバック検索（Middle）→ 拡張 → 失敗。

## rs 計算（例）

levels = [256, 128, 64], size = 192  
最小スロット b=64, N=ceil(192/64)=3  
u=[4,2,1]  
rs=[0,1,1] → 128 + 64 = 192

## 確保フロー（Dense）

```
1) rs = computeRequestSlots(size)
2) Trail: 末尾から連続領域を探す（Splitに当たれば子へ潜る）
3) 見つからなければ Middle: root から連続Freeを探す
4) plan(end_layer/end_slot) 確定 → plan通りのスロットを InUse + map（再ピックしない）
5) 連続が取れなければ VA 拡張 → 再試行 → 失敗
```

視覚メモ（Trail）:

```
Layer0: [Free][Free][Split][InUse]...[Free][Free]  ← 末尾から遡って連続を探す
               |--> Layer1 children ...
```

## 解放フロー

- Fast: 1 スロットを unmap → free → merge。
- Dense: 連続確保を前提に base+offset でスロットを特定し、unmap → free → merge。より安全にするには確保時のスロット情報をメタとして保持する案もあり。

## 設計上の注意

- plan と execute をズラさない（特定スロットを直接確保する）。
- levels は単調減少かつ割り切り。threshold は 2 の冪乗、最小レベルも冪乗。
- Dense は連続性必須。取れなければ VA 拡張、それでも無理なら失敗。
- Fragmentation を抑えるには冪乗 levels + Dense で 50% 未満を狙う。
- 将来: HeapOps 拡張で連続範囲の一括 map/unmap をサポートする。

<!-- 
適当なアルゴリズムを書いてみる。

```python
plan trailContiguousSearch(layer_idx, slot_idx, rs, count, is_found):
    slot = layer[layer_idx].slots[slot_idx]
    if !is_found:
        idx = 0
        free_count = 0
        while slot.state == Free && idx < count:
            ++free_count
            ++idx
            slot = layer[layer_idx].slots[slot_idx - idx]
        
        if free_count > rs[layer_idx]:
            if slot.state == split:
                child_count = levels[layer_idx] / levels[layer_idx + 1]
                return trailContiguousSearch(layer_idx + 1, slot.child + child_count, rs, child_count, true)
            else:
                plan.end_layer = layer_idx
                plan.end_slot = slot_idx - idx + 1
                plan.found = true
                return plan
        else if free_count == rs[layer_idx]:
            if slot.state == split:
                child_count = levels[layer_idx] / levels[layer_idx + 1]
                return trailContiguousSearch(layer_idx + 1, slot.child + child_count, rs, child_count, false)
        else:
            plan.fount=false
            return
    else:
        idx = 
        while slot.state == Free && idx < count:
            ++idx
            slot = layer[layer_idx].slots[slot_idx - idx]

        if slot.state == split:
            if slot.state == split:
                child_count = levels[layer_idx] / levels[layer_idx + 1]
                return trailContiguousSearch(layer_idx + 1, slot.child + child_count, rs, child_count, true)

        else:
            plan.end_layer = layer_idx
            plan.end_slot = slot_idx - idx + 1
            plan.fount = true
            return plan

``` -->