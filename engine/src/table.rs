use std::collections::HashMap;
use std::collections::hash_map::Entry;

use crate::moves::Move;
use crate::position::Position;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Bound {
    Exact,
    Lower,
    Upper,
}

#[derive(Clone, Copy, Debug)]
pub struct TableEntry {
    pub depth: usize,
    pub score: i32,
    pub bound: Bound,
    pub best_move: Option<Move>,
}

#[derive(Default)]
pub struct TranspositionTable {
    map: HashMap<u64, TableEntry>,
}

impl TranspositionTable {
    pub fn new() -> Self {
        Self {
            map: HashMap::new(),
        }
    }

    pub fn clear(&mut self) {
        self.map.clear();
    }

    pub fn store(&mut self, hash: u64, entry: TableEntry) {
        match self.map.entry(hash) {
            Entry::Occupied(mut occ) => {
                if entry.depth >= occ.get().depth {
                    occ.insert(entry);
                }
            }
            Entry::Vacant(vac) => {
                vac.insert(entry);
            }
        }
    }

    pub fn probe(&self, hash: u64) -> Option<&TableEntry> {
        self.map.get(&hash)
    }
}

pub fn compute_hash(position: &Position) -> u64 {
    position.zobrist_key()
}
