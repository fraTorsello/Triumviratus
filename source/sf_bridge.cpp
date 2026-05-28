// Bridge implementation: forwards to the standalone Stockfish NNUE probe.
// This is the ONLY translation unit (besides the sfnnue/ sources) that sees
// Stockfish headers, so the rest of the engine stays isolated from them.
//
// Thread-safety: each search thread owns its own incremental handle (its own
// Position mirror + StateInfo stack). The networks are immutable and only read,
// so concurrent eval calls are safe without locking. The original stateless
// sf_eval() builds a fresh local Position + thread_local scratch per call.

#include "sf_bridge.h"
#include "sfnnue/probe.h"
#include "sfnnue/position.h"
#include "sfnnue/evaluate.h"
#include "sfnnue/types.h"

#include <vector>
#include <cassert>

using namespace Stockfish;

void sf_init(const char* big_net, const char* small_net) {
    Stockfish::Probe::init(big_net, small_net);
}

int sf_eval(int side_white, const int* pieces, const int* squares, int count, int rule50) {
    return Stockfish::Probe::eval(pieces, squares, count, side_white != 0, rule50);
}

// ---------------------------------------------------------------------------
// Incremental per-thread mirror
// ---------------------------------------------------------------------------
namespace {

// Max search ply we mirror. Triumviratus caps the main search at max_ply (64)
// and qsearch stays within ~max_ply+8, so 256 is a generous safety margin.
constexpr int SF_STACK = 256;

struct SfPos {
    Position              pos;
    std::vector<StateInfo> st;   // pre-sized => element pointers stay stable
    std::vector<SfMove>    undo;  // how to reverse each pushed move
    std::vector<bool>      isNull;
    int                    ply;

    SfPos() : st(SF_STACK), undo(SF_STACK), isNull(SF_STACK, false), ply(0) {}
};

inline Color flip(Color c) { return c == WHITE ? BLACK : WHITE; }

// Copy the per-move scalar state and mark both accumulators dirty.
inline void begin_state(StateInfo* ns, StateInfo* prev, int rule50) {
    ns->previous               = prev;
    ns->rule50                 = rule50;
    ns->nonPawnMaterial[WHITE] = prev->nonPawnMaterial[WHITE];
    ns->nonPawnMaterial[BLACK] = prev->nonPawnMaterial[BLACK];
    ns->accumulatorBig.computed[WHITE]   = ns->accumulatorBig.computed[BLACK]   = false;
    ns->accumulatorSmall.computed[WHITE] = ns->accumulatorSmall.computed[BLACK] = false;
}

}  // namespace

void* sf_pos_create(void) { return new SfPos(); }

void sf_pos_destroy(void* handle) { delete static_cast<SfPos*>(handle); }

void sf_pos_set(void* handle, int side_white, const int* pieces,
                const int* squares, int count, int rule50) {
    SfPos* p = static_cast<SfPos*>(handle);
    p->ply = 0;
    // set() memsets the Position and the root StateInfo (computed=false,
    // previous=nullptr), so the first evaluate() does a full refresh.
    p->pos.set(pieces, squares, count, side_white != 0, rule50, &p->st[0]);
}

void sf_pos_do(void* handle, const struct SfMove* m) {
    SfPos*     p    = static_cast<SfPos*>(handle);
    StateInfo* prev = p->pos.st;
    assert(p->ply + 1 < SF_STACK);
    StateInfo* ns = &p->st[++p->ply];

    begin_state(ns, prev, m->rule50);

    // Build the dirty-piece list. The moving piece MUST be entry 0 because
    // Stockfish detects a king move (forcing a perspective refresh) via
    // dirtyPiece.piece[0].
    DirtyPiece& dp = ns->dirtyPiece;
    int         n  = 0;
    dp.piece[n] = Piece(m->movedPiece);
    dp.from[n]  = Square(m->from);
    dp.to[n]    = m->promoPiece ? SQ_NONE : Square(m->to);  // pawn vanishes on promotion
    ++n;
    if (m->promoPiece) {
        dp.piece[n] = Piece(m->promoPiece);
        dp.from[n]  = SQ_NONE;
        dp.to[n]    = Square(m->to);
        ++n;
    }
    if (m->capturedPiece) {
        dp.piece[n] = Piece(m->capturedPiece);
        dp.from[n]  = Square(m->capturedSq);
        dp.to[n]    = SQ_NONE;
        ++n;
    }
    if (m->rookPiece) {
        dp.piece[n] = Piece(m->rookPiece);
        dp.from[n]  = Square(m->rookFrom);
        dp.to[n]    = Square(m->rookTo);
        ++n;
    }
    dp.dirty_num = n;

    p->pos.st         = ns;
    p->pos.sideToMove = flip(p->pos.sideToMove);

    // Apply the board change in a capture-safe order.
    if (m->capturedPiece)
        p->pos.remove_piece(Square(m->capturedSq));
    p->pos.remove_piece(Square(m->from));
    p->pos.put_piece(Piece(m->promoPiece ? m->promoPiece : m->movedPiece), Square(m->to));
    if (m->rookPiece) {
        p->pos.remove_piece(Square(m->rookFrom));
        p->pos.put_piece(Piece(m->rookPiece), Square(m->rookTo));
    }

    // Keep non-pawn material exact (eval uses it for net selection + scaling).
    if (m->capturedPiece) {
        PieceType pt = type_of(Piece(m->capturedPiece));
        if (pt != PAWN && pt != KING)
            ns->nonPawnMaterial[color_of(Piece(m->capturedPiece))] -= PieceValue[m->capturedPiece];
    }
    if (m->promoPiece)
        ns->nonPawnMaterial[color_of(Piece(m->promoPiece))] += PieceValue[m->promoPiece];

    p->undo[p->ply]   = *m;
    p->isNull[p->ply] = false;
}

void sf_pos_do_null(void* handle, int rule50) {
    SfPos*     p    = static_cast<SfPos*>(handle);
    StateInfo* prev = p->pos.st;
    assert(p->ply + 1 < SF_STACK);
    StateInfo* ns = &p->st[++p->ply];

    begin_state(ns, prev, rule50);
    ns->dirtyPiece.dirty_num = 0;
    ns->dirtyPiece.piece[0]  = NO_PIECE;  // not a king => no forced refresh

    p->pos.st         = ns;
    p->pos.sideToMove = flip(p->pos.sideToMove);
    p->isNull[p->ply] = true;
}

void sf_pos_undo(void* handle) {
    SfPos* p = static_cast<SfPos*>(handle);

    p->pos.sideToMove = flip(p->pos.sideToMove);

    if (!p->isNull[p->ply]) {
        const SfMove& m = p->undo[p->ply];
        if (m.rookPiece) {
            p->pos.remove_piece(Square(m.rookTo));
            p->pos.put_piece(Piece(m.rookPiece), Square(m.rookFrom));
        }
        p->pos.remove_piece(Square(m.to));                            // mover or promoted piece
        p->pos.put_piece(Piece(m.movedPiece), Square(m.from));        // restore the (un-promoted) mover
        if (m.capturedPiece)
            p->pos.put_piece(Piece(m.capturedPiece), Square(m.capturedSq));
    }

    --p->ply;
    p->pos.st = &p->st[p->ply];
}

int sf_pos_eval(void* handle) {
    SfPos* p = static_cast<SfPos*>(handle);
    return Stockfish::Eval::evaluate(p->pos);
}
