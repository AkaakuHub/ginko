use crate::evaluation;
use crate::moves::{Move, MoveList};
use crate::piece::Color;
use crate::position::{Position, PositionError};
use crate::table::{self, Bound, TableEntry, TranspositionTable};

const MATE_VALUE: i32 = 30_000;

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
}

impl Default for SearchLimits {
    fn default() -> Self {
        Self { depth: 3 }
    }
}

pub struct Searcher {
    tt: TranspositionTable,
    nodes: u64,
}

impl Default for Searcher {
    fn default() -> Self {
        Self {
            tt: TranspositionTable::new(),
            nodes: 0,
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
        let max_depth = limits.depth.max(1);
        self.nodes = 0;
        self.tt.clear();

        let mut result = SearchResult::default();
        let mut best_move: Option<Move> = None;

        for depth in 1..=max_depth {
            let (score, mv) = self.root_search(position, depth)?;
            if let Some(m) = mv {
                best_move = Some(m);
            }
            result.best_move = best_move;
            result.score = score;
            result.depth = depth;
            result.nodes = self.nodes;
            self.print_info(depth, score, best_move, self.nodes);
        }

        Ok(result)
    }

    fn root_search(
        &mut self,
        position: &Position,
        depth: usize,
    ) -> Result<(i32, Option<Move>), PositionError> {
        self.nodes += 1;
        let hash = table::compute_hash(position);
        let tt_move = self.tt.probe(hash).and_then(|entry| entry.best_move);

        let moves = self.order_moves(position.generate_legal_moves()?, tt_move);
        if moves.is_empty() {
            return Ok((terminal_score(position, 0)?, None));
        }

        let mut best_score = -MATE_VALUE;
        let mut best_move = None;
        let mut alpha = -MATE_VALUE;
        let beta = MATE_VALUE;

        for mv in moves {
            let mover = position.side_to_move();
            let next = position.play_move(&mv)?;
            if let Some(score) =
                repetition_terminal_value(mover, next.current_repetition_count(), 1)
            {
                if score > best_score {
                    best_score = score;
                    best_move = Some(mv);
                }
                if score > alpha {
                    alpha = score;
                }
                continue;
            }
            let score = -self.negamax(&next, depth - 1, -beta, -alpha, 1)?;
            if score > best_score {
                best_score = score;
                best_move = Some(mv);
            }
            if score > alpha {
                alpha = score;
            }
        }

        Ok((best_score, best_move))
    }

    fn negamax(
        &mut self,
        position: &Position,
        depth: usize,
        mut alpha: i32,
        mut beta: i32,
        ply: usize,
    ) -> Result<i32, PositionError> {
        self.nodes += 1;

        if depth == 0 {
            return Ok(evaluation::evaluate(position));
        }

        let hash = table::compute_hash(position);
        let mut tt_move = None;
        let alpha_orig = alpha;
        let beta_orig = beta;

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
            tt_move = entry.best_move;
        }

        let moves = self.order_moves(position.generate_legal_moves()?, tt_move);
        if moves.is_empty() {
            return terminal_score(position, ply);
        }

        let mut best_value = -MATE_VALUE;
        let mut best_move = None;

        for mv in moves {
            let mover = position.side_to_move();
            let next = position.play_move(&mv)?;
            if let Some(term) =
                repetition_terminal_value(mover, next.current_repetition_count(), ply + 1)
            {
                if term > best_value {
                    best_value = term;
                    best_move = Some(mv);
                }
                if term > alpha {
                    alpha = term;
                }
                if alpha >= beta {
                    break;
                }
                continue;
            }
            let score = -self.negamax(&next, depth - 1, -beta, -alpha, ply + 1)?;
            if score > best_value {
                best_value = score;
                best_move = Some(mv);
            }
            if score > alpha {
                alpha = score;
            }
            if alpha >= beta {
                break;
            }
        }

        let bound = if best_value <= alpha_orig {
            Bound::Upper
        } else if best_value >= beta_orig {
            Bound::Lower
        } else {
            Bound::Exact
        };

        self.tt.store(
            hash,
            TableEntry {
                depth,
                score: best_value,
                bound,
                best_move,
            },
        );

        Ok(best_value)
    }

    fn order_moves(&self, mut moves: MoveList, tt_move: Option<Move>) -> MoveList {
        if let Some(target) = tt_move {
            if let Some(pos) = moves.iter().position(|m| *m == target) {
                moves.swap(0, pos);
            }
        }
        moves
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
    if position.is_in_check(position.side_to_move()) {
        Ok(-MATE_VALUE + ply as i32)
    } else {
        Ok(0)
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
