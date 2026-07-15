# Box Sort Planner — Design Doc

Status: **design / for review** (no code yet)
Scope: the *main* sort of Pokémon HOME boxes into dex order, **after** the dex layout has
been built (targets known). Replaces the current selection-sort in `sort_all_boxes`.

Design assumptions (per review):
- Starting distribution **varies** — the planner must be robust to "already roughly
  clustered" and "fully scattered" without being told which.
- Free scratch space is **tight**: typically **1–2 empty boxes** plus whatever empty slots
  are scattered inside partially-full boxes. Buffer/hole management is therefore central.

---

## 1. Guiding principle

The bottleneck is **physical movement**, not compute. So:

> Plan the entire move sequence offline against the in-memory `boxes` model, run as much
> search as we want, choose the cheapest complete plan, and only then execute on hardware.

Planning and execution are fully decoupled. The planner emits a **move script**
(ordered list of operations); a thin executor replays it with the existing
`swap_pokemon` / block-move primitives. We can generate several full plans and keep the
cheapest before touching the Switch.

---

## 2. Model

- **Grid**: `B` boxes × 30 slots (5 rows × 6 cols). Slot = `(box, row, col)`.
- Each Pokémon `p` has `current(p)` and known `target(p)`. Empty slots tracked explicitly.
- **Fixed points**: `current(p) == target(p)` → never moved, excluded from all work.
- **Free space** = union of all empty slots. Two flavors, both usable:
  - *whole empty boxes* (contiguous 30-slot regions) — 1–2 of them here, and
  - *scattered empties* inside occupied boxes — often the larger pool under tight buffers.
  Consolidating scattered empties into a usable congruent region is itself a planned action.

### Cost model (units ≈ physical time)

```
cost(single_swap)  = C_handle + α·dist(cursor→src) + α·dist(src→dst)
cost(block_move)   = C_handle + C_select(rect) + α·dist(cursor→src) + α·dist(src→dst)
dist(a,b)          = intra-box cursor steps + β·(#page flips between a.box and b.box)
```

`C_handle`, `C_select`, `α`, `β` are calibrated once from real timings.

**Block size has a sweet spot, not "bigger is always better."** A block move of `k` Pokémon
amortizes `C_handle` across all `k`, but `C_select` itself **grows with the selection** (you
drag the rectangle out cell-by-cell to size `m×n`, and a bigger rectangle is more likely to
need a farther-away or reshaped destination). So the true per-Pokémon cost is roughly:
```
per_pokemon(k) ≈ (C_handle + C_select(m,n) + travel) / k
```
which is U-shaped: tiny blocks waste the fixed handling cost, but very large blocks pay
rising `C_select` and destination-fit constraints. **The planner should prefer the
*most cost-effective* block, which is typically medium** — large enough to amortize handling,
small enough to keep selection cheap and destinations easy to satisfy. Whole-box moves are
just one point on this curve, and usually not the best one. The break-even minimum block
size `k*` and the effective upper sweet-spot come straight from calibration (§10 Q2).

---

## 3. Primitive algebra (why the plan is layered)

- **Block move preserves relative arrangement** — it *translates* an occupied shape into a
  congruent empty footprint. It can change which box/region Pokémon occupy; it **cannot
  reorder them** relative to each other.
- **Single swap reorders** two units but carries only one meaningful unit of work.

Consequence — the two primitives own different jobs:

| Job | Primitive | Why |
|---|---|---|
| Right *set* of Pokémon into the right *box* (coarse) | block move | translation, 30:1 cheap |
| Right *order* within a box (fine) | single swap | reordering, local/cheap travel |

The pipeline below is this split made explicit.

### 3.1 Block-move validity — mask compatibility (not "solid rectangle → empty rectangle")

A block move selects an `m×n` **rectangle** `S` in one box and drops it at a destination
anchor in some box. The rectangle carries an **occupancy mask** (which of its cells hold a
Pokémon; the rest are gaps). The move is valid iff:

> For every **occupied** cell of `S`, the corresponding destination cell is **empty**.

Destination cells that line up with **gaps** in `S` are **untouched** — whatever sits there
(Pokémon or empty) is left alone. Formally, with `occ(·)` the set of occupied offsets:
`occ(S) ∩ occ(Dest_footprint) = ∅` under the translation. No swap occurs; it is strictly
move-into-blanks, so occupied source cells need empty destination cells and nothing else is
constrained.

This is more permissive than a solid block, in two useful ways:

- **Select with holes, no reshaping needed.** A group of Pokémon that isn't a solid
  rectangle can still move in one op — just select the bounding rectangle *with its gaps*.
- **Deliberately expand `m×n` to straddle destination Pokémon.** You may grow the selection
  beyond the tight bounding box precisely *because* the extra cells are gaps that pass
  harmlessly over occupied destination cells.

**Worked example (yours).** Source = a 4×3 selection whose cell `(1,1)` is empty (11
Pokémon + 1 gap). Destination = a 4×3 region that is empty everywhere **except** a Pokémon
at `(1,1)`. Valid: the 11 source Pokémon land on the 11 empty destination cells, and the
source gap at `(1,1)` passes over the destination Pokémon at `(1,1)`, which stays put. One
operation, no reshaping.

**Planner consequence.** Candidate generation must reason over **masks**, not bounding
boxes: for a set of Pokémon we want to relocate, search over candidate selection rectangles
`S` (varying `m`, `n`, and anchor) and destination anchors, and accept any `(S, dest)` whose
masks are compatible per the rule above. Prefer the `(S, dest)` with the best
value/cost — which is often a *medium* rectangle-with-holes rather than a reshaped solid one
(see §6, and the size preference in §2).

---

## 4. Pipeline

```
Phase 0  Prep            targets, permutation, fixed points, free-space census, buffers
                         + compute the global cycle-swap baseline (§7.1a) as the floor
Phase 1  Coarse pass     block moves + buffer routing → each box holds its target SET
                         (each step must beat the cycle-swap base case, else swap)
Phase 2  Fine pass       per-box cycle-sort with single swaps → exact ORDER
Phase 3  Checkpoints     save + JSON export at safe box boundaries
```

The whole coarse pass is bounded below by the cycle-swap baseline: if block moves never
help (no/too little free space), Phase 1 degenerates to plain cycle-sort swaps and Phase 2
is empty — i.e. the planner reduces to optimal single-swap sorting in the worst case, and
only *adds* block moves where they strictly win.

### Phase 0 — Prep
1. Build `target(p)` for every Pokémon and the box-membership map
   `target_box(p) = target(p).box`.
2. Mark fixed points.
3. Census free space; reserve `R` scratch buffers (here `R ∈ {1,2}`). Record scattered
   empties per box.
4. Compute the coarse goal: for each box `T`, the multiset `want(T)` of Pokémon whose
   `target_box == T`, and the current multiset `have(T)`. `need(T) = want(T) \ have(T)`.

### Phase 1 — Coarse pass (block moves + buffers)
Goal state: `have(T) == want(T)` for every box (order ignored).
Operation menu:
- **Mask-compatible block translation** (§3.1) of a group into its target box or a staging
  region — typically a *medium* rectangle-with-holes (§2), not necessarily a whole box.
- **Adjacent-box exchange** (§7): fill a box from its `T±1` neighbor at one page-flip cost.
- **Evict-to-buffer**: a target box occupied by wrong Pokémon → block-move its wrong
  contents to a scratch box/region, then block-move the correct group in.
- **Reshape** (§6), fallback only, when no mask-compatible destination exists cheaply.

Under tight buffers this pass behaves like **cycle routing on boxes** (§5).

### Phase 2 — Fine pass (single swaps)
Each box now holds the right set, wrong order. Cycle-sort each box independently:
`min swaps for a box = occupied − cycles`. Optimal for the swap primitive; every swap is
intra-box so travel is minimal. A scattered empty in the box additionally permits rotate-
through-hole if ever cheaper (usually not; swaps win).

### Phase 3 — Checkpoints
Reuse the earlier success-only save/export logic: after a box reaches its final sorted
state, `pause_to_save()`; on success `export_boxes()`, on failure `revert_boxes()`.

---

## 5. Buffer/hole management under tight space (the crux)

With only 1–2 empty boxes, we cannot freely evacuate boxes. Treat blanks like the scarce
blank tile of a 15-puzzle: **maneuverability is bounded by total free space**, and
progress often requires *temporarily* worsening `Φ` (potential) to route a group.

Strategy:
1. **Prefer moves that consume no net free space**: direct block-move-home (into existing
   congruent blanks) and 2-for-1 transposition swaps. These never reduce maneuverability.
2. **Box cycle routing** when a set of boxes need to rotate contents (A→B→C→A): use one
   buffer as the rotation hole. Cost ≈ (cycle length + 1) block moves. Pick the box
   decomposition that minimizes total blocks, biasing to cycles where large sub-groups are
   already together (so each block move is big).
3. **Consolidate scattered empties**: when no whole buffer box is free, reshape scattered
   empties within a box into a congruent region to serve as a temporary destination. Costs
   local single moves; only do it when it unlocks a block move that pays for it.
4. **Never strand the last hole**: keep an invariant that at least one congruent empty
   footprint large enough for the smallest pending block always remains reachable, else the
   plan deadlocks. The search enforces this as a hard constraint on candidate generation.

---

## 6. Reshaping subroutine (fallback, not a prerequisite)

Because block moves are validated by **mask compatibility** (§3.1), a group usually does
**not** need to be a solid rectangle first — select the bounding rectangle *with its gaps*
and find a mask-compatible destination. Reshaping is therefore a **fallback**, used only when
no mask-compatible `(S, dest)` exists for the group at acceptable cost (e.g. the group's mask
collides with occupied cells at every candidate destination). When that happens:

```
reshape_cost(pokemon_set S, desired_shape D):
    # cheapest local single-moves to move S onto the cells of D
    solve assignment: match each p in S to a distinct cell of D
        minimizing Σ dist(current(p), cell)     # small; Hungarian or greedy
    return matched moves + total cost
```

Fold `reshape_cost` into the candidate block move's cost. **Fire only if net value stays
positive** — i.e., reshape iff the block move it enables still pays for the reshaping.

---

## 7. Heuristic search engine (drives Phase 1, optionally the whole plan)

Greedy best-first with rollout; compute is free so we search widely and offline.

**Potentials**
```
Φ(state) = Σ_{misplaced p} d(current(p), target(p))      # physical-distance weighted
M(state) = # misplaced Pokémon
```

**Candidate value**
```
value(o) = (ΔΦ + μ·Δplaced) / cost(o)
    Δplaced = # Pokémon newly landed exactly on target      (μ large: placing home dominates)
    ΔΦ      = Φ(before) − Φ(after)
    cost(o) = §2 cost, including any reshape_cost
```

**Bounded candidate generation per step** (never enumerate the full move space):
```
gen_candidates(state):
    C = []
    C += place_home_block_moves(state)     # mask-compatible groups → target box (§3.1)
    C += adjacent_box_exchanges(state)     # cheap L/R-neighbor moves (see below)
    C += whole_box_translations(state)     # to a buffer, only if ΔΦ > 0
    C += transposition_swaps(state)        # single swaps that place 2 at once (2-cycles)
    if best_value(C) <= 0:                 # stuck: allow a temporary-worsening unlock
        C += buffer_eviction_moves(state)  # route via the scarce hole (may raise Φ)
    filter C by the "never strand the last hole" invariant (§5.4)
    return C
```

**Adjacent-box exchanges (your neighbor idea).** Boxes `T` and `T±1` are one page flip
apart (`β`), the cheapest possible cross-box travel. This is worth exploiting as its own
candidate class: when Pokémon destined for `T` sit in `T±1` (or vice-versa) near the current
selection region, move them across with a small mask-compatible block or a short single
move. Two concrete uses:

- **Migration by bubbling toward the target box.** A scattered Pokémon can be walked toward
  its target box through neighbor moves; each hop is only `β`, and hops can be batched as
  neighbor block moves so the per-Pokémon cost stays low.
- **Fill-from-neighbor.** When a target box has empty slots near its edge and the needed
  Pokémon are just over the boundary in `T±1`, a neighbor block move fills them in place
  without staging through a buffer — valuable precisely because buffers are scarce here.

Helpful? Yes, especially in the scattered / tight-buffer regime: it gives the engine a cheap
locality-driven move that doesn't consume a whole buffer box. The value function keeps it
honest — a neighbor *block* move rates highly (low travel, several placed), while a neighbor
*single* move rates only modestly, so the engine won't over-bubble one Pokémon at a time
when a better block exists.

**Loop (greedy + rollout to escape myopia)**
```
plan_phase1(state):
    plan = []
    while M(state) > 0 and coarse_goal_unmet(state):
        C = gen_candidates(state)
        # rollout: for each top-K candidate, greedily finish and score full remaining cost
        best = argmin_{o in topK(C by value)} rollout_cost(apply(state,o))
        state = apply(state, best); plan.append(best)
    return plan
```

Optional upgrade: keep a **beam** of the best `W` states instead of a single greedy path,
and/or generate several independent plans (varying tie-breaks/seed) and keep the cheapest.
All affordable because it's offline.

### 7.1 Cycle-swap base case + anti-thrash (never move far away just to come back)

Two safeguards, because a naive block-move engine will happily shove a Pokémon out to a far
buffer and haul it back later:

**(a) Global cycle-swap baseline — always computed, always the floor.**
Independent of the block engine, decompose the *entire* current→target permutation into
cycles and compute the single-swap plan directly: for each cycle of length `L`, `L−1` swaps
(a 2-cycle = one transposition that places **both** endpoints). This baseline:
- is the **guaranteed fallback** when free space is insufficient for any valid block
  destination (the "no/too-few spaces" case you called out — cycle swaps need *zero* free
  space),
- is a **base case checked every iteration**: if the best available block move's value is not
  strictly better than just resolving a cycle with swaps here, take the swap,
- bounds the whole plan: **`final_plan = min(block-augmented plan, pure cycle-swap plan)`**
  region-by-region, so the planner can never do *worse* than plain cycle sorting.

A pure transposition swap places two Pokémon in one op and moves no one out of the way, so it
is the natural antidote to round-tripping — the engine falls back to it whenever staging
doesn't clearly pay.

**(b) Anti-thrash weighting in `Φ` and in move admissibility.**
- **Protect near-home Pokémon.** Weight displacement superlinearly in remaining distance's
  *inverse closeness*: moving a Pokémon that is already at/near its target costs extra in the
  value function (e.g. add a penalty `P·[d_before small]` when a move increases a small
  `d(p)`). Correctly-placed Pokémon are hard-pinned (never disturbed unless a swap places the
  disturbing Pokémon home in the same op).
- **Monotone-distance preference.** Prefer moves under which every displaced Pokémon's
  remaining distance is non-increasing; charge moves that push any Pokémon *farther* from its
  target by the round-trip they imply (`+2·α·Δdistance`, the cost to undo).
- **No-net-progress guard.** Reject a staging/eviction move whose only effect is relocation
  with `Δplaced == 0` **unless** it strictly enables a later placement in the rollout that
  more than repays both legs of the trip. This is exactly the "moved out of the way just to
  move back" pattern, and this guard is what forbids it.

**Robustness to distribution (answers "varies"):** the value function auto-adapts. When
target-mates are already clustered, `place_home_block_moves` finds big cheap blocks and
dominates. When scattered, those candidates are weak, so the engine leans on
buffer-eviction/routing and reshaping. No mode flag needed.

---

## 8. Quality bound (to know how good a plan is)

Exact minimization is NP-hard (sorting-by-block-moves + token-swapping on a grid). Judge
plans against a lower bound instead of chasing optimum:
```
LB_ops ≥ max(
    ceil(M / 30),                                   # each op moves ≤ 30
    Σ_boxes (occupied_b − cycles_b)                 # unavoidable fine-pass swaps
)
```
Report `plan_ops / LB_ops` so we can see headroom and tune `μ`, beam width, and the
reshape threshold.

---

## 9. Integration points with existing code

- **New executor primitive needed**: a general `block_move(src_rect, dst_anchor)` wrapping
  the multi-select drag (the manual ZR/A/dpad sequence in `sort_into_correct_boxes`'s
  30-at-once special case is a hardcoded instance of this — generalize it).
- **Reuse**: `swap_pokemon` for the fine pass; `reconcile_box`/`scan_box` for verification;
  the success-only `export_boxes`/`revert_boxes` for checkpoints.
- **Replace**: the selection-sort body of `sort_all_boxes` becomes
  `plan = plan_phase1(state); plan += plan_fine(state); execute(plan)`.
- **Planner is pure/in-memory** over a copy of `boxes`, so it can be unit-tested and
  cost-measured without hardware.

---

## 10. Open questions / tuning knobs

1. Calibrate `C_handle`, `C_select`, `α`, `β` from real timings (esp. the multi-select
   entry/drag cost, which sets the block-vs-swap break-even `k`).
2. Break-even block size: below `k*` Pokémon a block move isn't worth its select overhead —
   derive `k*` from the calibration and let candidate generation prune sub-`k*` blocks.
3. Rollout depth / beam width `W` vs planning-time budget (generous, since offline).
4. Verification cadence: how often to `reconcile_box` mid-plan vs trusting the model
   (drift risk vs time cost).
5. Does the block-move UI allow non-rectangular multi-select or only rectangles? Confirms
   whether reshaping targets rectangles only (assumed here) or arbitrary masks.
