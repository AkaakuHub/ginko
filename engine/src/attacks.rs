use crate::bitboard::Bitboard;
use crate::board::{BOARD_FILES, BOARD_RANKS, Square};
use crate::piece::Color;

const DIR_ROOK: &[(i8, i8)] = &[(0, 1), (0, -1), (-1, 0), (1, 0)];
const DIR_BISHOP: &[(i8, i8)] = &[(1, 1), (1, -1), (-1, 1), (-1, -1)];

fn ray_attacks(square: Square, occupancy: Bitboard, directions: &[(i8, i8)]) -> Bitboard {
    let mut attacks = Bitboard::EMPTY;
    for &(df, dr) in directions {
        let mut current = square;
        while let Some(next) = current.offset(df, dr) {
            attacks.insert(next);
            if occupancy.contains(next) {
                break;
            }
            current = next;
        }
    }
    attacks
}

fn step_attacks(square: Square, offsets: &[(i8, i8)]) -> Bitboard {
    let mut attacks = Bitboard::EMPTY;
    for &(df, dr) in offsets {
        if let Some(sq) = square.offset(df, dr) {
            attacks.insert(sq);
        }
    }
    attacks
}

pub fn pawn_attacks(color: Color, square: Square) -> Bitboard {
    match color {
        Color::Black => step_attacks(square, &[(0, -1)]),
        Color::White => step_attacks(square, &[(0, 1)]),
    }
}

pub fn silver_attacks(color: Color, square: Square) -> Bitboard {
    match color {
        Color::Black => step_attacks(square, &[(-1, -1), (0, -1), (1, -1), (-1, 1), (1, 1)]),
        Color::White => step_attacks(square, &[(-1, -1), (1, -1), (-1, 1), (0, 1), (1, 1)]),
    }
}

pub fn gold_attacks(color: Color, square: Square) -> Bitboard {
    match color {
        Color::Black => step_attacks(
            square,
            &[(-1, -1), (0, -1), (1, -1), (-1, 0), (0, 1), (1, 0)],
        ),
        Color::White => step_attacks(square, &[(-1, 0), (0, -1), (1, 0), (-1, 1), (0, 1), (1, 1)]),
    }
}

pub fn king_attacks(square: Square) -> Bitboard {
    step_attacks(
        square,
        &[
            (-1, -1),
            (0, -1),
            (1, -1),
            (-1, 0),
            (1, 0),
            (-1, 1),
            (0, 1),
            (1, 1),
        ],
    )
}

pub fn bishop_attacks(square: Square, occupancy: Bitboard) -> Bitboard {
    ray_attacks(square, occupancy, DIR_BISHOP)
}

pub fn rook_attacks(square: Square, occupancy: Bitboard) -> Bitboard {
    ray_attacks(square, occupancy, DIR_ROOK)
}

pub fn horse_attacks(square: Square, occupancy: Bitboard) -> Bitboard {
    bishop_attacks(square, occupancy) | king_attacks(square)
}

pub fn dragon_attacks(square: Square, occupancy: Bitboard) -> Bitboard {
    rook_attacks(square, occupancy) | king_attacks(square)
}

pub fn pawn_attack_bitboard(color: Color, occupancy: Bitboard) -> Bitboard {
    let mut result = Bitboard::EMPTY;
    for rank in 0..BOARD_RANKS {
        for file in 0..BOARD_FILES {
            let square = Square::from_file_rank(file as u8, rank as u8);
            if occupancy.contains(square) {
                result |= pawn_attacks(color, square);
            }
        }
    }
    result
}
