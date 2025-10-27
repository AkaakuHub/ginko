use std::sync::OnceLock;

use crate::board::{BOARD_SQUARES, Square};
use crate::hand::{HAND_MAX_COUNT, HAND_PIECE_KIND_COUNT, HandPieceKind};
use crate::piece::{Color, PIECE_KIND_COUNT, PieceKind};

const COLORS: usize = 2;

struct ZobristTables {
    piece_square: [[[u64; BOARD_SQUARES]; PIECE_KIND_COUNT]; COLORS],
    hand: [[[u64; HAND_MAX_COUNT]; HAND_PIECE_KIND_COUNT]; COLORS],
    side_to_move: u64,
}

static TABLES: OnceLock<ZobristTables> = OnceLock::new();

fn tables() -> &'static ZobristTables {
    TABLES.get_or_init(ZobristTables::generate)
}

impl ZobristTables {
    fn generate() -> Self {
        let mut state = 0x9E3779B97F4A7C15u64;
        let mut next = || -> u64 {
            state = splitmix64(state);
            state
        };

        let mut piece_square = [[[0u64; BOARD_SQUARES]; PIECE_KIND_COUNT]; COLORS];
        for color in 0..COLORS {
            for kind in 0..PIECE_KIND_COUNT {
                for square in 0..BOARD_SQUARES {
                    piece_square[color][kind][square] = next();
                }
            }
        }

        let mut hand = [[[0u64; HAND_MAX_COUNT]; HAND_PIECE_KIND_COUNT]; COLORS];
        for color in 0..COLORS {
            for kind in 0..HAND_PIECE_KIND_COUNT {
                for count in 0..HAND_MAX_COUNT {
                    hand[color][kind][count] = next();
                }
            }
        }

        let side_to_move = next();

        Self {
            piece_square,
            hand,
            side_to_move,
        }
    }
}

fn splitmix64(mut x: u64) -> u64 {
    x = x.wrapping_add(0x9E3779B97F4A7C15);
    let mut z = x;
    z = (z ^ (z >> 30)).wrapping_mul(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)).wrapping_mul(0x94D049BB133111EB);
    z ^ (z >> 31)
}

pub fn piece_square(color: Color, kind: PieceKind, square: Square) -> u64 {
    let tables = tables();
    tables.piece_square[color.index()][kind.index()][square.index() as usize]
}

pub fn hand(color: Color, kind: HandPieceKind, count: usize) -> u64 {
    let idx = count.min(HAND_MAX_COUNT - 1);
    let tables = tables();
    tables.hand[color.index()][kind.index()][idx]
}

pub fn side_to_move() -> u64 {
    tables().side_to_move
}
