use crate::board::all_squares;
use crate::hand::{Hand, HandPieceKind};
use crate::piece::{Color, PieceKind};
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

fn score_board(position: &Position) -> i32 {
    let mut score = 0;
    for square in all_squares() {
        if let Some(piece) = position.piece_at(square) {
            let value = piece_value(piece.kind) as i32;
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
