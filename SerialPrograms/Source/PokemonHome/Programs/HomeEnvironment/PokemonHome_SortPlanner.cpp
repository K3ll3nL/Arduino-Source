/*  Pokemon HOME — Box Sort Planner (pure). See PokemonHome_SortPlanner.h / SortPlanner_Design.md. */

#include "PokemonHome_SortPlanner.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <tuple>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonHome{

SortPlanner::SortPlanner(std::vector<std::vector<int>> current,
                         std::vector<std::vector<int>> desired,
                         PlannerConfig config)
    : m_cfg(config)
    , m_cur(std::move(current))
    , m_des(std::move(desired))
{
    m_boxes = (int)m_cur.size();

    // Find the largest token id so we can size the per-token lookup tables.
    for (const auto& box : m_cur)
        for (int t : box) m_max_token = std::max(m_max_token, t);
    for (const auto& box : m_des)
        for (int t : box) m_max_token = std::max(m_max_token, t);

    m_desired_slot.assign(m_max_token + 1, -1);
    m_current_slot.assign(m_max_token + 1, -1);

    for (int b = 0; b < m_boxes; b++){
        for (int r = 0; r < ROWS; r++){
            for (int c = 0; c < COLS; c++){
                int cur = at(b, r, c);
                if (cur != EMPTY) m_current_slot[cur] = flat(b, r, c);
                int des = m_des[b][r*COLS + c];
                if (des != EMPTY) m_desired_slot[des] = flat(b, r, c);
            }
        }
    }
}

// --- cost model ------------------------------------------------------------------------

double SortPlanner::swap_cost(const PlanSlot& a, const PlanSlot& b) const{
    // grab at a, travel a->b, drop; plus page flips for box changes.
    double travel = m_cfg.alpha * (std::abs(a.row - b.row) + std::abs(a.col - b.col))
                  + m_cfg.beta  * std::abs(a.box - b.box);
    return m_cfg.c_handle + travel;
}

double SortPlanner::block_cost(const PlanOp& op) const{
    double select = m_cfg.c_select_base + m_cfg.c_select_per_cell * (op.height * op.width);
    double travel = m_cfg.alpha * (std::abs(op.src.row - op.dst.row) + std::abs(op.src.col - op.dst.col))
                  + m_cfg.beta  * std::abs(op.src.box - op.dst.box);
    return m_cfg.c_handle + select + travel;
}

// --- state mutation --------------------------------------------------------------------

void SortPlanner::apply_swap(const PlanSlot& a, const PlanSlot& b, PlanResult& out){
    int ta = at(a.box, a.row, a.col);
    int tb = at(b.box, b.row, b.col);
    set(a.box, a.row, a.col, tb);
    set(b.box, b.row, b.col, ta);
    if (ta != EMPTY) m_current_slot[ta] = flat(b.box, b.row, b.col);
    if (tb != EMPTY) m_current_slot[tb] = flat(a.box, a.row, a.col);

    PlanOp op; op.type = PlanOpType::SWAP; op.a = a; op.b = b;
    out.ops.push_back(op);
    out.swap_count++;
    out.estimated_cost += swap_cost(a, b);
}

void SortPlanner::apply_block(const PlanOp& op, PlanResult& out){
    // Move every occupied source cell to the translated destination cell; source cells clear.
    int dbox = op.dst.box - op.src.box;
    int drow = op.dst.row - op.src.row;
    int dcol = op.dst.col - op.src.col;

    // Gather first (so we don't stomp cells within the same box before reading them).
    struct Move{ int fb, fr, fc, tb, tr, tc, tok; };
    std::vector<Move> moves;
    for (int r = 0; r < op.height; r++){
        for (int c = 0; c < op.width; c++){
            int sr = op.src.row + r, sc = op.src.col + c;
            int tok = at(op.src.box, sr, sc);
            if (tok == EMPTY) continue;                    // gap — carried as empty, no-op
            moves.push_back({op.src.box, sr, sc,
                             op.src.box + dbox, sr + drow, sc + dcol, tok});
        }
    }
    for (const Move& m : moves) set(m.fb, m.fr, m.fc, EMPTY);
    for (const Move& m : moves){
        set(m.tb, m.tr, m.tc, m.tok);
        m_current_slot[m.tok] = flat(m.tb, m.tr, m.tc);
    }

    out.ops.push_back(op);
    out.block_count++;
    out.tokens_placed_by_block += (int)moves.size();
    out.estimated_cost += block_cost(op);
}

// --- candidate: block-move-home --------------------------------------------------------
//
// Find a group of misplaced tokens sharing one source box and one translation vector, whose
// desired slots are all currently empty, and whose source bounding rectangle contains no
// other occupied cells (so the mask is a clean translation). Emitting it places the whole
// group home in a single op. Progress-monotone: it only fills empty desired cells, so it
// never displaces a correctly-placed token.

bool SortPlanner::best_block_move(PlanOp& out, int& placed, double& value) const{
    if (!m_cfg.use_block_moves) return false;

    double best_value = 0.0;
    bool found = false;

    // Search EVERY sub-rectangle of EVERY source box (the grid is only 5x6, so this is cheap).
    // A rectangle is a valid block move iff every occupied cell in it shares one translation
    // vector and lands on an empty destination (multi-move only fills blanks — no multi-swap).
    // Unlike a single bounding-box test, this finds smaller valid sections when the full group
    // is broken up by a stray/placed cell, instead of throwing the whole block away.
    for (int sb = 0; sb < m_boxes; sb++){
        for (int r0 = 0; r0 < ROWS; r0++){
        for (int r1 = r0; r1 < ROWS; r1++){
        for (int c0 = 0; c0 < COLS; c0++){
        for (int c1 = c0; c1 < COLS; c1++){
            int height = r1 - r0 + 1, width = c1 - c0 + 1;
            if (height * width < m_cfg.min_block) continue;   // too small to bother

            bool valid = true;
            int movers = 0;
            int vbox = 0, vrow = 0, vcol = 0;
            bool have_v = false;
            for (int r = r0; r <= r1 && valid; r++){
                for (int c = c0; c <= c1 && valid; c++){
                    int tok = at(sb, r, c);
                    if (tok == EMPTY) continue;                 // gap — carried empty, fine
                    if (is_placed(tok)) { valid = false; break; } // would un-place a correct token
                    int db, dr, dc; unflat(m_desired_slot[tok], db, dr, dc);
                    if (at(db, dr, dc) != EMPTY) { valid = false; break; } // dest occupied
                    int tb = db - sb, tr = dr - r, tc = dc - c;
                    if (!have_v){ vbox = tb; vrow = tr; vcol = tc; have_v = true; }
                    else if (tb != vbox || tr != vrow || tc != vcol){ valid = false; break; }
                    movers++;
                }
            }
            if (!valid || movers < m_cfg.min_block) continue;

            // The whole rectangle (including gaps) must translate on-grid and to a real box.
            if (sb + vbox < 0 || sb + vbox >= m_boxes) continue;
            if (r0 + vrow < 0 || r1 + vrow >= ROWS) continue;
            if (c0 + vcol < 0 || c1 + vcol >= COLS) continue;

            PlanOp op;
            op.type   = PlanOpType::BLOCK_MOVE;
            op.src    = {sb, r0, c0};
            op.dst    = {sb + vbox, r0 + vrow, c0 + vcol};
            op.height = height;
            op.width  = width;

            double approach = m_cfg.alpha * (std::abs(m_cursor_row - r0) + std::abs(m_cursor_col - c0))
                            + m_cfg.beta  *  std::abs(m_cursor_box - sb);
            double v = (double)movers / (approach + block_cost(op));
            if (v > best_value){
                best_value = v;
                out = op;
                placed = movers;
                found = true;
            }
        }}}}
    }

    value = best_value;
    return found;
}

// --- candidate: cycle-advancing swap ---------------------------------------------------
//
// Pick a misplaced token and swap it directly onto its desired slot. The token currently in
// that desired slot is necessarily misplaced (a placed token would already occupy its own
// desired slot, which is unique), so this never regresses. This is cycle sort and is always
// available while the arrangement is unsorted — the guaranteed base case.

bool SortPlanner::next_placing_swap(PlanSlot& a, PlanSlot& b, double& value) const{
    // Choose the *most efficient* placing swap, not just the first one. Value = Pokémon placed
    // home per unit physical cost, so this prefers (1) 2-cycles/transpositions that land BOTH
    // endpoints home in one swap, and (2) near swaps (same box, then adjacent boxes) over
    // far ones. Picking "the first misplaced token by id" instead made it fixate on one cell as
    // a pivot and turned every swap into a long cross-box round trip.
    bool found = false;
    double best_value = -1.0;
    for (int tok = 0; tok <= m_max_token; tok++){
        if (m_current_slot[tok] < 0) continue;
        if (is_placed(tok)) continue;
        int cf = m_current_slot[tok];
        int df = m_desired_slot[tok];
        int cb, cr, cc; unflat(cf, cb, cr, cc);
        int db, dr, dc; unflat(df, db, dr, dc);

        // A swap places `tok` at its home. If the Pokémon currently sitting on that home wants
        // tok's slot in return, the single swap lands both — a 2-cycle worth double.
        int occ = at(db, dr, dc);
        double placements = (occ != EMPTY && m_desired_slot[occ] == cf) ? 2.0 : 1.0;

        PlanSlot sa{cb, cr, cc}, sb{db, dr, dc};
        // Cost includes travel from the cursor's current position to the swap, so nearby swaps
        // win and the cursor sweeps a region instead of teleporting across boxes each step.
        double approach = m_cfg.alpha * (std::abs(m_cursor_row - cr) + std::abs(m_cursor_col - cc))
                        + m_cfg.beta  *  std::abs(m_cursor_box - cb);
        double cand_value = placements / (approach + swap_cost(sa, sb));
        if (!found || cand_value > best_value){
            found = true;
            best_value = cand_value;
            a = sa;
            b = sb;
        }
    }
    value = found ? best_value : 0.0;
    return found;
}

// --- driver ----------------------------------------------------------------------------

bool SortPlanner::next_op_greedy(PlanOp& out) const{
    // Both candidate types are progress-monotone; pick whichever has the higher value (Pokémon
    // placed per unit cost, cursor-approach included), so a small nearby block that beats a
    // swap is actually chosen instead of always defaulting to swaps.
    PlanOp block; int placed = 0; double block_value = 0.0;
    bool have_block = best_block_move(block, placed, block_value);

    PlanSlot a, b; double swap_value = 0.0;
    bool have_swap = next_placing_swap(a, b, swap_value);

    if (!have_block && !have_swap) return false;   // sorted

    if (have_block && (!have_swap || block_value >= swap_value)){
        out = block;
        return true;
    }

    out = PlanOp{};
    out.type = PlanOpType::SWAP;
    out.a = a;
    out.b = b;
    return true;   // a/b guaranteed valid here (misplaced tokens exist)
}

int SortPlanner::count_misplaced() const{
    int m = 0;
    for (int tok = 0; tok <= m_max_token; tok++){
        if (m_current_slot[tok] < 0) continue;
        if (m_current_slot[tok] != m_desired_slot[tok]) m++;
    }
    return m;
}

void SortPlanner::apply_op_tracking(const PlanOp& op, double& total_cost){
    PlanResult dummy;   // discarded; we only need the state mutation + cost
    if (op.type == PlanOpType::SWAP){
        double approach = m_cfg.alpha * (std::abs(m_cursor_row - op.a.row) + std::abs(m_cursor_col - op.a.col))
                        + m_cfg.beta  *  std::abs(m_cursor_box - op.a.box);
        total_cost += approach + swap_cost(op.a, op.b);
        apply_swap(op.a, op.b, dummy);
        m_cursor_box = op.b.box; m_cursor_row = op.b.row; m_cursor_col = op.b.col;
    } else {
        double approach = m_cfg.alpha * (std::abs(m_cursor_row - op.src.row) + std::abs(m_cursor_col - op.src.col))
                        + m_cfg.beta  *  std::abs(m_cursor_box - op.src.box);
        total_cost += approach + block_cost(op);
        apply_block(op, dummy);
        m_cursor_box = op.dst.box; m_cursor_row = op.dst.row; m_cursor_col = op.dst.col;
    }
}

std::vector<PlanOp> SortPlanner::gather_candidates(int breadth) const{
    std::vector<PlanOp> cands;

    PlanOp block; int placed = 0; double bv = 0.0;
    if (best_block_move(block, placed, bv)) cands.push_back(block);

    // Collect every placing swap with its immediate value, then keep the top `breadth`.
    std::vector<std::pair<double, PlanOp>> swaps;
    for (int tok = 0; tok <= m_max_token; tok++){
        if (m_current_slot[tok] < 0 || is_placed(tok)) continue;
        int cf = m_current_slot[tok], df = m_desired_slot[tok];
        int cb, cr, cc; unflat(cf, cb, cr, cc);
        int db, dr, dc; unflat(df, db, dr, dc);
        int occ = at(db, dr, dc);
        double placements = (occ != EMPTY && m_desired_slot[occ] == cf) ? 2.0 : 1.0;
        PlanSlot sa{cb, cr, cc}, sb{db, dr, dc};
        double approach = m_cfg.alpha * (std::abs(m_cursor_row - cr) + std::abs(m_cursor_col - cc))
                        + m_cfg.beta  *  std::abs(m_cursor_box - cb);
        double val = placements / (approach + swap_cost(sa, sb));
        PlanOp op; op.type = PlanOpType::SWAP; op.a = sa; op.b = sb;
        swaps.emplace_back(val, op);
    }

    int n = std::min<int>((int)swaps.size(), std::max(1, breadth));
    if (n > 0){
        std::partial_sort(swaps.begin(), swaps.begin() + n, swaps.end(),
            [](const std::pair<double, PlanOp>& x, const std::pair<double, PlanOp>& y){ return x.first > y.first; });
        for (int i = 0; i < n; i++) cands.push_back(swaps[i].second);
    }
    return cands;
}

double SortPlanner::rollout_score(const PlanOp& first, int ply) const{
    SortPlanner sim(*this);                 // copy; playout mutates the copy only
    int before = sim.count_misplaced();
    double total_cost = 0.0;

    sim.apply_op_tracking(first, total_cost);
    for (int i = 0; i < ply; i++){
        PlanOp nxt;
        if (!sim.next_op_greedy(nxt)) break;   // fully sorted mid-playout
        sim.apply_op_tracking(nxt, total_cost);
    }

    int placed = before - sim.count_misplaced();   // >=1: every op is progress-monotone
    if (total_cost <= 0.0) return 0.0;
    return (double)placed / total_cost;
}

bool SortPlanner::next_op(PlanOp& out) const{
    // No lookahead -> immediate greedy choice.
    if (m_cfg.lookahead_ply <= 0)
        return next_op_greedy(out);

    // Rollout: score each candidate first-op by a short greedy playout and take the best.
    std::vector<PlanOp> cands = gather_candidates(m_cfg.lookahead_breadth);
    if (cands.empty()) return false;   // sorted

    double best_score = -1.0;
    bool found = false;
    for (const PlanOp& cand : cands){
        double s = rollout_score(cand, m_cfg.lookahead_ply);
        if (s > best_score){
            best_score = s;
            out = cand;
            found = true;
        }
    }
    return found;
}

PlanResult SortPlanner::plan(){
    PlanResult out;
    PlanOp op;
    while (next_op(op)){
        if (op.type == PlanOpType::SWAP) apply_swap(op.a, op.b, out);
        else                             apply_block(op, out);
    }
    return out;
}

int SortPlanner::lower_bound_ops() const{
    // max( ceil(misplaced / SLOTS), swaps needed by cycle decomposition )
    int misplaced = 0;
    for (int tok = 0; tok <= m_max_token; tok++){
        if (m_current_slot[tok] < 0) continue;
        if (m_current_slot[tok] != m_desired_slot[tok]) misplaced++;
    }

    // cycle decomposition of the permutation over occupied tokens.
    std::vector<char> seen(m_max_token + 1, 0);
    int cycles = 0, in_cycles = 0;
    for (int tok = 0; tok <= m_max_token; tok++){
        if (m_current_slot[tok] < 0 || seen[tok]) continue;
        if (m_current_slot[tok] == m_desired_slot[tok]){ seen[tok] = 1; continue; }
        // walk the cycle: from a token, the token that must move into its current slot
        // (i.e. the token whose desired slot == this token's current slot).
        // Build reverse map lazily via scan is O(n^2); acceptable for planning sizes.
        int len = 0;
        int t = tok;
        while (!seen[t]){
            seen[t] = 1;
            in_cycles++;
            len++;
            // find token u whose desired slot == current slot of t
            int target_slot = m_current_slot[t];
            int next = -1;
            for (int u = 0; u <= m_max_token; u++){
                if (m_current_slot[u] < 0) continue;
                if (m_desired_slot[u] == target_slot){ next = u; break; }
            }
            if (next < 0 || seen[next]) break;
            t = next;
        }
        if (len > 0) cycles++;
    }
    int cycle_swaps = in_cycles - cycles;

    int block_bound = (misplaced + SLOTS - 1) / SLOTS;
    return std::max(block_bound, cycle_swaps);
}

}
}
}
