use crate::board::{all_squares, Square, BOARD_FILES, BOARD_RANKS};
use crate::hand::{Hand, HandPieceKind};
use crate::piece::{Color, Piece, PieceKind};
use crate::position::Position;

const PIECE_VALUES: [i32; 10] = [
    15_000, // King
    700,    // Gold
    600,    // Silver
    650,    // Promoted Silver
    900,    // Bishop
    1_100,  // Promoted Bishop (Horse)
    1_000,  // Rook
    1_200,  // Promoted Rook (Dragon)
    100,    // Pawn
    400,    // Tokin
];

fn piece_value(kind: PieceKind) -> i32 {
    PIECE_VALUES[kind as usize]
}

pub fn piece_material_value(kind: PieceKind) -> i32 {
    piece_value(kind)
}

fn hand_piece_value(kind: HandPieceKind) -> i32 {
    match kind {
        HandPieceKind::Gold => piece_value(PieceKind::Gold),
        HandPieceKind::Silver => piece_value(PieceKind::Silver),
        HandPieceKind::Bishop => piece_value(PieceKind::Bishop),
        HandPieceKind::Rook => piece_value(PieceKind::Rook),
        HandPieceKind::Pawn => piece_value(PieceKind::Pawn),
    }
}

fn score_hand(color: Color, hand: &Hand) -> i32 {
    let mut score = 0;
    for kind in HandPieceKind::all() {
        let count = hand.count(kind) as i32;
        if count == 0 {
            continue;
        }
        let value = hand_piece_value(kind) * count;
        score += match color {
            Color::Black => value,
            Color::White => -value,
        };
    }
    score
}

fn positional_bonus(piece: Piece, square: Square) -> i32 {
    let file = square.file() as i32;
    let rank = square.rank() as i32;
    let center_file = (BOARD_FILES as i32 - 1) / 2;
    let center_rank = (BOARD_RANKS as i32 - 1) / 2;
    let center_distance = (file - center_file).abs() + (rank - center_rank).abs();
    let center_bonus = (4 - center_distance).max(0) * 10;

    let advancement = match piece.color {
        Color::Black => BOARD_RANKS as i32 - 1 - rank,
        Color::White => rank,
    };

    match piece.kind {
        PieceKind::Pawn => advancement * 25 + center_bonus * 2,
        PieceKind::Tokin => advancement * 20 + center_bonus * 3 + 50,
        PieceKind::Silver => advancement * 15 + center_bonus * 3,
        PieceKind::PromotedSilver => advancement * 20 + center_bonus * 3 + 40,
        PieceKind::Bishop => center_bonus * 5,
        PieceKind::PromotedBishop => center_bonus * 6 + 40,
        PieceKind::Rook => advancement * 10 + center_bonus * 6,
        PieceKind::PromotedRook => advancement * 12 + center_bonus * 6 + 60,
        PieceKind::Gold => advancement * 12 + center_bonus * 3,
        PieceKind::King => (4 - (rank - center_rank).abs()) * 20 - advancement * 10,
    }
}

fn score_board(position: &Position) -> i32 {
    let mut score = 0;
    for square in all_squares() {
        if let Some(piece) = position.piece_at(square) {
            let material = piece_value(piece.kind);
            let positional = positional_bonus(piece, square);
            let value = material + positional;
            score += match piece.color {
                Color::Black => value,
                Color::White => -value,
            };
        }
    }
    score
}

pub fn evaluate(position: &Position) -> i32 {
    let mut score = score_board(position);
    for color in [Color::Black, Color::White] {
        score += score_hand(color, position.hand(color));
    }
    match position.side_to_move() {
        Color::Black => score,
        Color::White => -score,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn initial_position_is_equal() {
        let position = Position::initial().expect("initial");
        assert_eq!(evaluate(&position), 0);
    }
}
