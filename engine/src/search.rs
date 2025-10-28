use std::ops::Range;
use std::time::{SystemTime, UNIX_EPOCH};

use crate::evaluation;
use crate::moves::{Move, MoveList};
use crate::piece::{Color, PIECE_KIND_COUNT};
use crate::position::{Position, PositionError};
use crate::table::{self, Bound, TableEntry, TranspositionTable};

use crate::board::BOARD_SQUARES;

const MATE_VALUE: i32 = 30_000;
const MAX_PLY: usize = 64;

#[derive(Clone)]
struct SimpleRng(u64);

impl SimpleRng {
    fn new(seed: u64) -> Self {
        let mut s = seed;
        if s == 0 {
            s = 0x9E37_79B9_7F4A_7C15;
        }
        Self(s)
    }

    fn next_u64(&mut self) -> u64 {
        let mut x = self.0;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        self.0 = x;
        x.wrapping_mul(0x2545F4914F6CDD1D)
    }

    fn gen_range(&mut self, range: Range<usize>) -> usize {
        let span = range.end - range.start;
        if span == 0 {
            return range.start;
        }
        (self.next_u64() as usize % span) + range.start
    }
}

#[derive(Debug, Default, Clone)]
pub struct SearchResult {
    pub best_move: Option<Move>,
    pub score: i32,
    pub depth: usize,
    pub nodes: u64,
}

#[derive(Debug, Clone, Copy)]
pub struct SearchLimits {
    pub depth: usize,
    pub randomness: i32,
}

impl Default for SearchLimits {
    fn default() -> Self {
        Self {
            depth: 3,
            randomness: 0,
        }
    }
}

#[derive(Clone, Copy)]
struct RootEntry {
    mv: Move,
    score: i32,
}

pub struct Searcher {
    tt: TranspositionTable,
    nodes: u64,
    killers: [[Option<Move>; 2]; MAX_PLY],
    history: [[[i32; BOARD_SQUARES]; PIECE_KIND_COUNT]; 2],
    rng: SimpleRng,
    limits: SearchLimits,
    root_entries: Vec<RootEntry>,
}

impl Default for Searcher {
    fn default() -> Self {
        let seed = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| d.as_nanos() as u64)
            .unwrap_or(0x9E37_79B9_7F4A_7C15);
        Self {
            tt: TranspositionTable::new(),
            nodes: 0,
            killers: [[None; 2]; MAX_PLY],
            history: [[[0; BOARD_SQUARES]; PIECE_KIND_COUNT]; 2],
            rng: SimpleRng::new(seed),
            limits: SearchLimits::default(),
            root_entries: Vec::new(),
        }
    }
}

impl Searcher {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn search(
        &mut self,
        position: &Position,
        limits: SearchLimits,
    ) -> Result<SearchResult, PositionError> {
        self.limits = limits;
        let max_depth = limits.depth.max(1);
        self.nodes = 0;
        self.tt.clear();
        self.clear_heuristics();
        self.root_entries.clear();

        if position.generate_legal_moves()?.is_empty() {
            let score = terminal_score(position, 0)?;
            return Ok(SearchResult {
                best_move: None,
                score,
                depth: 0,
                nodes: self.nodes,
            });
        }

        let mut result = SearchResult::default();
        let mut last_score = 0;

        for depth in 1..=max_depth {
            let mut alpha = -MATE_VALUE;
            let mut beta = MATE_VALUE;

            if depth > 1 {
                let window = 50;
                alpha = (last_score - window).max(-MATE_VALUE);
                beta = (last_score + window).min(MATE_VALUE);
            }

            loop {
                let iteration = self.root_iteration(position, depth, alpha, beta)?;
                if iteration.best_move.is_none() {
                    break;
                }

                let score = iteration.score;
                last_score = score;
                result.best_move = iteration.best_move;
                result.score = score;
                result.depth = depth;
                result.nodes = self.nodes;
                self.print_info(depth, score, iteration.best_move, self.nodes);

                if score <= alpha {
                    alpha = -MATE_VALUE;
                    beta = score + 1;
                    continue;
                }
                if score >= beta {
                    beta = MATE_VALUE;
                    alpha = score - 1;
                    continue;
                }
                break;
            }
        }

        result.best_move = self.pick_root_move();
        Ok(result)
    }

    fn root_iteration(
        &mut self,
        position: &Position,
        depth: usize,
        mut alpha: i32,
        beta: i32,
    ) -> Result<SearchResult, PositionError> {
        self.nodes += 1;
        let hash = table::compute_hash(position);
        let tt_move = self.tt.probe(hash).and_then(|entry| entry.best_move);

        let mut moves = position.generate_legal_moves()?;
        if moves.is_empty() {
            let score = terminal_score(position, 0)?;
            return Ok(SearchResult {
                best_move: None,
                score,
                depth: 0,
                nodes: self.nodes,
            });
        }

        self.order_moves(position, &mut moves, tt_move, 0);

        let mut best_move = None;
        let mut best_score = -MATE_VALUE;
        let mut local_entries: Vec<RootEntry> = Vec::with_capacity(moves.len());

        for mv in moves {
            let mover = position.side_to_move();
            let next = position.play_move(&mv)?;

            if let Some(score) =
                repetition_terminal_value(mover, next.current_repetition_count(), 1)
            {
                local_entries.push(RootEntry { mv, score });
                if score > best_score {
                    best_score = score;
                    best_move = Some(mv);
                }
                if score > alpha {
                    alpha = score;
                }
                continue;
            }

            let mut child_depth = depth - 1;
            if next.is_in_check(next.side_to_move()) {
                child_depth += 1;
            }
            let score = -self.alpha_beta(&next, child_depth, -beta, -alpha, 1)?;
            local_entries.push(RootEntry { mv, score });

            if score > best_score {
                best_score = score;
                best_move = Some(mv);
            }
            if score > alpha {
                alpha = score;
            }
        }

        local_entries.sort_by(|a, b| b.score.cmp(&a.score));
        self.root_entries = local_entries;

        if let Some(best) = best_move {
            self.tt.store(
                hash,
                TableEntry {
                    depth,
                    score: best_score,
                    bound: Bound::Exact,
                    best_move: Some(best),
                },
            );
        }

        Ok(SearchResult {
            best_move,
            score: best_score,
            depth,
            nodes: self.nodes,
        })
    }

    fn alpha_beta(
        &mut self,
        position: &Position,
        depth: usize,
        mut alpha: i32,
        mut beta: i32,
        ply: usize,
    ) -> Result<i32, PositionError> {
        self.nodes += 1;

        if let Some(score) = repetition_terminal_value(
            position.side_to_move(),
            position.current_repetition_count(),
            ply,
        ) {
            return Ok(score);
        }

        if depth == 0 {
            return self.quiescence(position, alpha, beta, ply);
        }

        let hash = table::compute_hash(position);
        if let Some(entry) = self.tt.probe(hash) {
            if entry.depth >= depth {
                match entry.bound {
                    Bound::Exact => return Ok(entry.score),
                    Bound::Lower => alpha = alpha.max(entry.score),
                    Bound::Upper => beta = beta.min(entry.score),
                }
                if alpha >= beta {
                    return Ok(entry.score);
                }
            }
        }

        let mut moves = position.generate_legal_moves()?;
        if moves.is_empty() {
            return terminal_score(position, ply);
        }

        let tt_move = self.tt.probe(hash).and_then(|entry| entry.best_move);
        self.order_moves(position, &mut moves, tt_move, ply);

        let mut best_value = -MATE_VALUE;
        let mut best_move = None;
        let mut searched_any = false;

        for mv in moves {
            let mover = position.side_to_move();
            let next = position.play_move(&mv)?;

            if let Some(score) = repetition_terminal_value(mover, next.current_repetition_count(), ply + 1) {
                if score > best_value {
                    best_value = score;
                    best_move = Some(mv);
                }
                if score > alpha {
                    alpha = score;
                }
                if alpha >= beta {
                    self.register_cutoff(position, mv, ply);
                    break;
                }
                continue;
            }

            let mut child_depth = depth - 1;
            if next.is_in_check(next.side_to_move()) {
                child_depth += 1;
            }

            let score = -self.alpha_beta(&next, child_depth, -beta, -alpha, ply + 1)?;
            searched_any = true;

            if score > best_value {
                best_value = score;
                best_move = Some(mv);
            }
            if score > alpha {
                alpha = score;
            }
            if alpha >= beta {
                self.register_cutoff(position, mv, ply);
                break;
            }
        }

        let bound = if best_value <= alpha { Bound::Upper } else if best_value >= beta { Bound::Lower } else { Bound::Exact };

        if searched_any {
            self.tt.store(
                hash,
                TableEntry {
                    depth,
                    score: best_value,
                    bound,
                    best_move,
                },
            );
        }

        Ok(best_value)
    }

    fn quiescence(
        &mut self,
        position: &Position,
        mut alpha: i32,
        beta: i32,
        ply: usize,
    ) -> Result<i32, PositionError> {
        self.nodes += 1;

        if let Some(score) = repetition_terminal_value(
            position.side_to_move(),
            position.current_repetition_count(),
            ply,
        ) {
            return Ok(score);
        }

        let stand_pat = evaluation::evaluate(position);
        if stand_pat >= beta {
            return Ok(beta);
        }
        let mut value = stand_pat;
        if value > alpha {
            alpha = value;
        }

        let mut moves = self.generate_tactical_moves(position)?;
        if moves.is_empty() {
            return Ok(value);
        }

        moves.sort_by(|a, b| self.capture_order_score(position, b).cmp(&self.capture_order_score(position, a)));

        for mv in moves {
            let mover = position.side_to_move();
            let next = position.play_move(&mv)?;

            if let Some(score) = repetition_terminal_value(mover, next.current_repetition_count(), ply + 1) {
                if score > value {
                    value = score;
                }
                if value >= beta {
                    return Ok(beta);
                }
                if value > alpha {
                    alpha = value;
                }
                continue;
            }

            let score = -self.quiescence(&next, -beta, -alpha, ply + 1)?;
            if score >= beta {
                return Ok(beta);
            }
            if score > value {
                value = score;
            }
            if score > alpha {
                alpha = score;
            }
        }

        Ok(value)
    }

    fn generate_tactical_moves(&self, position: &Position) -> Result<MoveList, PositionError> {
        let mut result = MoveList::new();
        for mv in position.generate_legal_moves()? {
            if mv.is_drop() {
                continue;
            }
            if position.piece_at(mv.to).is_some() || mv.promote {
                result.push(mv);
            }
        }
        Ok(result)
    }

    fn order_moves(
        &mut self,
        position: &Position,
        moves: &mut MoveList,
        tt_move: Option<Move>,
        ply: usize,
    ) {
        moves.sort_by(|a, b| {
            let sb = self.move_score(position, *b, tt_move, ply);
            let sa = self.move_score(position, *a, tt_move, ply);
            sb.cmp(&sa)
        });
    }

    fn move_score(
        &self,
        position: &Position,
        mv: Move,
        tt_move: Option<Move>,
        ply: usize,
    ) -> i32 {
        if Some(mv) == tt_move {
            return 1_000_000;
        }

        let mut score = 0;
        if let Some(killers) = self.killers.get(ply) {
            if killers[0] == Some(mv) {
                score += 900_000;
            } else if killers[1] == Some(mv) {
                score += 800_000;
            }
        }

        if let Some(captured) = position.piece_at(mv.to) {
            let capture_value = evaluation::piece_material_value(captured.kind);
            let mover_value = evaluation::piece_material_value(mv.piece);
            score += 500_000 + (capture_value - mover_value);
        } else if mv.promote {
            score += 400_000;
        }

        let color_idx = position.side_to_move().index();
        let to_idx = mv.to.index() as usize;
        score += self.history[color_idx][mv.piece.index()][to_idx];

        score
    }

    fn capture_order_score(&self, position: &Position, mv: &Move) -> i32 {
        position
            .piece_at(mv.to)
            .map(|pc| evaluation::piece_material_value(pc.kind))
            .unwrap_or(0)
            - evaluation::piece_material_value(mv.piece)
    }

    fn register_cutoff(&mut self, position: &Position, mv: Move, ply: usize) {
        let idx = ply.min(MAX_PLY - 1);
        if !mv.is_drop() && position.piece_at(mv.to).is_none() {
            let killers = &mut self.killers[idx];
            if killers[0] != Some(mv) {
                killers[1] = killers[0];
                killers[0] = Some(mv);
            }
        }

        let color_idx = position.side_to_move().index();
        let to_idx = mv.to.index() as usize;
        let history = &mut self.history[color_idx][mv.piece.index()][to_idx];
        *history += (ply as i32 + 1) * (ply as i32 + 1);
        if *history > 200_000 {
            *history /= 2;
        }
    }

    fn pick_root_move(&mut self) -> Option<Move> {
        if self.root_entries.is_empty() {
            return None;
        }

        let best_score = self.root_entries[0].score;
        if self.limits.randomness <= 0 {
            return Some(self.root_entries[0].mv);
        }

        let threshold = best_score - self.limits.randomness;
        let mut candidates: Vec<Move> = Vec::new();
        for entry in &self.root_entries {
            if entry.score >= threshold {
                candidates.push(entry.mv);
            } else {
                break;
            }
        }
        if candidates.is_empty() {
            candidates.push(self.root_entries[0].mv);
        }
        let idx = self.rng.gen_range(0..candidates.len());
        Some(candidates[idx])
    }

    fn clear_heuristics(&mut self) {
        self.killers = [[None; 2]; MAX_PLY];
        for color in &mut self.history {
            for piece in color.iter_mut() {
                piece.fill(0);
            }
        }
    }

    fn print_info(&self, depth: usize, score: i32, best: Option<Move>, nodes: u64) {
        let (score_tag, score_value) = if score.abs() >= MATE_VALUE - 100 {
            let mate = if score > 0 {
                (MATE_VALUE - score + 1) / 2
            } else {
                -((MATE_VALUE + score + 1) / 2)
            };
            ("mate", mate.to_string())
        } else {
            ("cp", score.to_string())
        };

        if let Some(mv) = best {
            println!(
                "info depth {} score {} {} nodes {} pv {}",
                depth,
                score_tag,
                score_value,
                nodes,
                mv.to_usi()
            );
        } else {
            println!(
                "info depth {} score {} {} nodes {}",
                depth, score_tag, score_value, nodes
            );
        }
    }
}

fn terminal_score(position: &Position, ply: usize) -> Result<i32, PositionError> {
    let mate_score = -MATE_VALUE + ply as i32;
    if position.is_in_check(position.side_to_move()) {
        Ok(mate_score)
    } else {
        Ok(mate_score)
    }
}

fn repetition_terminal_value(
    mover: Color,
    repeat_count: usize,
    ply_from_root: usize,
) -> Option<i32> {
    if repeat_count >= 4 {
        let mate_score = (MATE_VALUE - ply_from_root as i32).max(1);
        let value = match mover {
            Color::Black => -mate_score,
            Color::White => mate_score,
        };
        return Some(value);
    }

    if repeat_count == 3 {
        let penalty = (MATE_VALUE / 4).max(1);
        return Some(match mover {
            Color::Black => -penalty,
            Color::White => penalty,
        });
    }

    if repeat_count == 2 {
        const SOFT_PENALTY: i32 = 500;
        return Some(match mover {
            Color::Black => -SOFT_PENALTY,
            Color::White => SOFT_PENALTY,
        });
    }

    None
}
