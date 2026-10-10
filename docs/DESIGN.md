# Design and trade-offs

## Matching state

`orderbook.hpp` is the header-only matching engine. Bid price levels use a
`std::map` ordered high to low; asks use a low-to-high map. Each level holds a
`std::list<std::shared_ptr<Order>>` in arrival order. An incoming order repeatedly
visits the opposite map's first level and the list's first order while prices
cross. Each trade uses the resting price and the smaller remaining quantity.

Filled orders leave both their FIFO list and the ID index. Empty price levels
are erased. An unfilled GTC order joins the end of its own level; an IOC remainder
is discarded. The result vector contains executions from that command only.
There is no transaction log or sequencing layer.

An `unordered_map<OrderId, OrderEntry>` locates each live order and stores its
list iterator. List iterators remain valid across unrelated insertions and
erasures. The index entry is removed when that particular order leaves the
list. The engine retains a copy of submitted order values, so a caller cannot
mutate resting state through the original pointer. `GetOrder` and
`GetOrderInfos` return independent value snapshots.

Copying and moving `OrderBook` are disabled: blindly copying the index would
leave iterators pointing at the original lists. A future movable/copyable design
would need deliberate ownership and iterator rebuilding rules.

## Validation and modification

`Order` validates positive tick prices, positive quantities, and known enums.
Submission also rejects null pointers, duplicate live IDs, and previously filled
orders. IDs are unique while live; zero is valid, and departed IDs may be reused.
Depth sums individual uint32 quantities into uint64 with an overflow check.

`ModifyOrder` validates the replacement and its unchanged side before canceling
the old order, then submits the replacement with the old order type. This
cancel/replace policy resets arrival priority and filled history, even at the
same price. The specified quantity is a replacement total, not a delta. There
is no exchange-specific amend-in-place policy.

Validation failures leave the old order intact. That guarantee does not extend
to allocation failure during the subsequent cancel/replace submission.

## Complexity

Let `N` be live orders, `L` price levels on the relevant side, and `K` executions
produced by a command. Hash costs below assume ordinary distribution; allocation
costs and vector growth are amortized, and `log L` means `log(L + 1)`.

| Operation | Average / amortized time | Caveat |
| --- | --- | --- |
| Non-crossing GTC insertion | O(log L) | Map lookup/insertion plus list and hash insertion; hash worst case O(N) |
| Cancel by ID | O(log L) | Hash lookup and list erase average O(1), but the engine also finds the price in the map; hash worst case O(N) |
| Matching submission | O(K + log L) | O(K) FIFO fills and index removals, plus possible remainder insertion; pathological hashing can raise the bound to O(KN + N + log L) |
| Cancel/replace modification | O(K + log L) | Cancellation followed by normal submission; same hash caveats |
| Order lookup/snapshot | Average O(1), worst O(N) | Copies one order value after hash lookup |
| Full aggregated depth snapshot | O(N + L) | Traverses all queues and allocates O(L) snapshot storage |
| `Size()` | O(1) | Cached container size |

Map erasure by iterator has amortized constant cost in matching; cancellation
still performs a logarithmic map lookup, including when an empty level is
removed. Calling cancellation wholly constant-time would omit that lookup.
Total persistent engine storage is O(N + L); command result storage is O(K).
The shared pointers, node containers, and the value submission path introduce
allocation/copy overhead. No pool, flat map, or allocator optimization is claimed.

## Exception and concurrency boundaries

Recording each trade precedes applying that particular fill. Resting insertion
cleans up its new list entry/level if index insertion fails. These measures keep
local container operations consistent, but provide no full command rollback:
a multi-fill submission may have already changed earlier orders when a later
allocation throws, and a replacement may already have canceled its predecessor.
The caller may not receive earlier executions if the command throws. Recovery
from such failures would require a stronger transactional design.

All access is single-threaded. Synchronizing only writes would be insufficient;
readers and snapshots also need synchronization if accessed concurrently. The
project has no persistence, crash recovery, gateway validation, or risk engine.

## Replay and benchmark boundaries

`tools/replay.*` defines value commands, strict parsing, engine dispatch, and
stream replay. The CLI only selects file/stdin and handles help/errors. Full
numeric parses reject signs, overflow, and trailing junk. Replay counts physical
lines, including ignored comments/blanks, and wraps parser/engine errors with
that line. Output preserves earlier records when later commands fail, and emits
a final summary only after successful input consumption.

`benchmarks/benchmark.*` generates commands before measurement and calls the
same engine dispatcher without CSV parsing. Expected counts and final depth come
from cycle arithmetic, independently of the engine. An untimed warmup and two
separate passes check those outcomes; aggregate throughput avoids the latency
pass's per-operation clock calls. See [methodology and results](BENCHMARKS.md).
