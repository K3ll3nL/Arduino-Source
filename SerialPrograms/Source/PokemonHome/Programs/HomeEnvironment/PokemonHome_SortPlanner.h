/*  Pokemon HOME — Box Sort Planner (pure, hardware-independent)
 *
 *  Plans the minimal-movement transformation of the current box arrangement into a desired
 *  (dex-sorted) arrangement, emitting an ordered list of physical operations: rectangular
 *  BLOCK moves (mask-compatible translations of a group into empty target cells) and single
 *  SWAPs. See SortPlanner_Design.md for the full design.
 *
 *  This module has NO dependency on the game, controller, or video pipeline. It operates on
 *  an abstract grid of opaque token ids so it can be unit-tested and cost-measured offline.
 *  A separate executor replays PlanOps with the real primitives.
 *
 *  v1 scope: only "progress-monotone" moves are emitted — every op places at least one token
 *  on its final slot and never displaces an already-correct token. This makes the plan
 *  thrash-free by construction (no evict-to-buffer staging that could move a token away only
 *  to bring it back), optimal single-swap (cycle-sort) in the worst case, and augmented with
 *  block moves wherever a group is already in the correct relative order. Buffer-staging,
 *  reshaping, adjacent-box exchange, and beam/rollout search are deferred to later layers.
 */

#ifndef PokemonAutomation_PokemonHome_SortPlanner_H
#define PokemonAutomation_PokemonHome_SortPlanner_H

#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

struct PlanSlot{
    int box = 0;
    int row = 0;
    int col = 0;
};

enum class PlanOpType{
    SWAP,
    BLOCK_MOVE,
};

struct PlanOp{
    PlanOpType type;

    // SWAP: exchange the contents of slot a and slot b (either may be empty).
    PlanSlot a;
    PlanSlot b;

    // BLOCK_MOVE: select the rectangle whose top-left is `src` with the given height/width in
    // the source box (src.box), carrying its occupancy mask, and drop it anchored at `dst`
    // (top-left, dst.box). Every occupied source cell lands on an empty destination cell;
    // cells under source-gaps are untouched (mask compatibility).
    PlanSlot src;
    PlanSlot dst;
    int height = 0;
    int width  = 0;
};

struct PlannerConfig{
    // Cost model — calibrate from real timings. Units are arbitrary but must be consistent.
    double c_handle         = 4.0;   // fixed grab+drop cost per operation
    double c_select_base    = 2.0;   // fixed cost to enter multi-select
    double c_select_per_cell= 0.15;  // dragging the selection rectangle out (per cell of m*n)
    double alpha            = 1.0;   // per intra-box cursor step
    double beta             = 3.0;   // per page flip (box change)

    int    min_block        = 2;     // k*: a block move must carry at least this many tokens
    bool   use_block_moves  = true;

    // Pre-sort exploration (rollout). Each candidate first-op is scored by greedily playing
    // `lookahead_ply` follow-up ops and measuring Pokémon-placed per unit cost — so a swap that
    // *unlocks* a big block scores well and gets chosen. Follow-ups are ordinary placing/block
    // moves (all progress-monotone), so no staging and no thrash regardless of depth.
    //   lookahead_ply     = 0 -> pure greedy (no lookahead)
    //                       1 -> the 1-ply swap->block lookahead (default)
    //                       2+ -> deeper; compute scales with breadth * ply
    int    lookahead_ply     = 1;
    int    lookahead_breadth = 6;    // how many candidate first-ops to roll out each step
};

struct PlanResult{
    std::vector<PlanOp> ops;
    double estimated_cost = 0.0;
    int    swap_count     = 0;
    int    block_count    = 0;
    int    tokens_placed_by_block = 0;   // diagnostic: how many tokens the block moves handled
};

// Grid geometry (a HOME box is 5 rows x 6 cols).
class SortPlanner{
public:
    static constexpr int ROWS  = 5;
    static constexpr int COLS  = 6;
    static constexpr int SLOTS = ROWS * COLS;
    static constexpr int EMPTY = -1;

    // `current` and `desired` are per-box grids indexed [box][row*COLS + col]; each entry is a
    // token id (>= 0) or EMPTY. The two grids must have the same number of boxes and contain
    // the same multiset of token ids. Token ids are opaque and only used for identity.
    SortPlanner(std::vector<std::vector<int>> current,
                std::vector<std::vector<int>> desired,
                PlannerConfig config = {});

    // Produce the ordered operation list against the internal model. Deterministic.
    // Frontloaded — useful for a cost/quality estimate. For execution prefer per-op recompute
    // via next_op() so physical divergence is absorbed each step.
    PlanResult plan();

    // Compute the single best next operation for the current grids WITHOUT applying it.
    // Returns false when already sorted. Intended for per-op (receding-horizon) execution:
    // the caller rebuilds a fresh SortPlanner from the live board after each physical move.
    bool next_op(PlanOp& out) const;

    // Tell the planner where the cursor currently is (planner box coords) so it prefers ops
    // near the cursor and sweeps locally instead of teleporting across the region each step.
    void set_cursor(int box, int row, int col){ m_cursor_box = box; m_cursor_row = row; m_cursor_col = col; }

    // Lower bound on operation count (see design §8), for measuring plan quality.
    int lower_bound_ops() const;

private:
    int m_boxes;
    PlannerConfig m_cfg;

    std::vector<std::vector<int>> m_cur;      // mutated during planning
    const std::vector<std::vector<int>> m_des;

    // desired flat-slot for each token id, and current flat-slot, kept in sync with m_cur.
    // flat slot = box*SLOTS + row*COLS + col.
    std::vector<int> m_desired_slot;          // indexed by token id
    std::vector<int> m_current_slot;          // indexed by token id
    int m_max_token = -1;

    // Current cursor position (planner box coords), updated each step by the executor.
    int m_cursor_box = 0;
    int m_cursor_row = 0;
    int m_cursor_col = 0;

    // helpers
    static int flat(int box, int row, int col){ return box*SLOTS + row*COLS + col; }
    static void unflat(int f, int& box, int& row, int& col){
        box = f / SLOTS; int r = f % SLOTS; row = r / COLS; col = r % COLS;
    }
    int  at(int box, int row, int col) const { return m_cur[box][row*COLS + col]; }
    void set(int box, int row, int col, int tok){ m_cur[box][row*COLS + col] = tok; }

    bool is_placed(int tok) const { return m_current_slot[tok] == m_desired_slot[tok]; }

    double swap_cost(const PlanSlot& a, const PlanSlot& b) const;
    double block_cost(const PlanOp& op) const;

    void apply_swap(const PlanSlot& a, const PlanSlot& b, PlanResult& out);
    void apply_block(const PlanOp& op, PlanResult& out);

    // Find the best progress-monotone block move currently available (or return false).
    // Also reports its value (Pokémon placed per unit cost, cursor-approach included).
    bool best_block_move(PlanOp& out, int& placed, double& value) const;
    // A cycle-advancing swap that places a token home (always available while unsorted).
    // Also reports its value on the same scale as best_block_move.
    bool next_placing_swap(PlanSlot& a, PlanSlot& b, double& value) const;

    // Immediate greedy choice (best block vs best swap by value) — used both directly when
    // lookahead is off and as the playout policy inside rollouts.
    bool next_op_greedy(PlanOp& out) const;
    // Candidate first-ops to roll out: the best block plus the top-`breadth` placing swaps.
    std::vector<PlanOp> gather_candidates(int breadth) const;
    // Greedy playout score for a candidate first-op: Pokémon placed per unit cost over the
    // op plus `ply` follow-ups. Runs on a copy; does not mutate this planner.
    double rollout_score(const PlanOp& first, int ply) const;
    // Apply an op to THIS planner (used on rollout copies), accruing its cursor-aware cost and
    // advancing the cursor to where the op ends.
    void apply_op_tracking(const PlanOp& op, double& total_cost);
    // Number of occupied tokens not on their desired slot.
    int count_misplaced() const;
};

}
}
}
#endif
