use crate::board::Square;
use crate::piece::PieceKind;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Move {
    pub from: Option<Square>,
    pub to: Square,
    pub piece: PieceKind,
    pub promote: bool,
}

impl Move {
    pub fn normal(from: Square, to: Square, piece: PieceKind, promote: bool) -> Self {
        Self {
            from: Some(from),
            to,
            piece,
            promote,
        }
    }

    pub fn drop(to: Square, piece: PieceKind) -> Self {
        Self {
            from: None,
            to,
            piece,
            promote: false,
        }
    }

    pub fn is_drop(self) -> bool {
        self.from.is_none()
    }

    pub fn to_usi(&self) -> String {
        if let Some(from) = self.from {
            let mut s = String::with_capacity(5);
            s.push_str(&from.to_coord());
            s.push_str(&self.to.to_coord());
            if self.promote {
                s.push('+');
            }
            s
        } else {
            let piece_char = self
                .piece
                .drop_char()
                .expect("drop should only contain droppable pieces");
            format!("{}*{}", piece_char, self.to.to_coord())
        }
    }
}

pub type MoveList = Vec<Move>;

#[cfg(test)]
mod tests {
    use super::*;
    use crate::board::Square;
    use crate::piece::PieceKind;

    #[test]
    fn to_usi_normal_move() {
        let from = Square::from_coord("5e").unwrap();
        let to = Square::from_coord("5d").unwrap();
        let mv = Move::normal(from, to, PieceKind::Pawn, false);
        assert_eq!(mv.to_usi(), "5e5d");
    }

    #[test]
    fn to_usi_drop_move() {
        let to = Square::from_coord("3c").unwrap();
        let mv = Move::drop(to, PieceKind::Gold);
        assert_eq!(mv.to_usi(), "G*3c");
    }
}
