use crate::piece::PieceKind;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HandPieceKind {
    Gold = 0,
    Silver = 1,
    Bishop = 2,
    Rook = 3,
    Pawn = 4,
}

pub const HAND_PIECE_KIND_COUNT: usize = 5;
pub const HAND_MAX_COUNT: usize = 6;

impl HandPieceKind {
    pub const fn all() -> [Self; 5] {
        [
            Self::Gold,
            Self::Silver,
            Self::Bishop,
            Self::Rook,
            Self::Pawn,
        ]
    }

    pub const fn index(self) -> usize {
        self as usize
    }

    pub fn from_piece_kind(kind: PieceKind) -> Option<Self> {
        match kind {
            PieceKind::Gold => Some(Self::Gold),
            PieceKind::Silver => Some(Self::Silver),
            PieceKind::Bishop => Some(Self::Bishop),
            PieceKind::Rook => Some(Self::Rook),
            PieceKind::Pawn => Some(Self::Pawn),
            _ => None,
        }
    }

    pub fn to_char(self) -> char {
        match self {
            Self::Gold => 'G',
            Self::Silver => 'S',
            Self::Bishop => 'B',
            Self::Rook => 'R',
            Self::Pawn => 'P',
        }
    }
}

/// 持ち駒の枚数管理。
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct Hand {
    counts: [u8; HAND_PIECE_KIND_COUNT],
}

impl Hand {
    pub fn add(&mut self, kind: HandPieceKind, amount: u8) -> u8 {
        self.counts[kind as usize] = self.counts[kind as usize].saturating_add(amount);
        self.counts[kind as usize]
    }

    pub fn remove(&mut self, kind: HandPieceKind, amount: u8) -> u8 {
        self.counts[kind as usize] = self.counts[kind as usize].saturating_sub(amount);
        self.counts[kind as usize]
    }

    pub fn set(&mut self, kind: HandPieceKind, amount: u8) {
        self.counts[kind as usize] = amount;
    }

    pub fn count(&self, kind: HandPieceKind) -> u8 {
        self.counts[kind as usize]
    }

    pub fn is_empty(&self) -> bool {
        self.counts.iter().all(|&c| c == 0)
    }

    pub fn to_sfen(&self, lower: bool) -> String {
        let mut buf = String::new();
        for kind in HandPieceKind::all() {
            let count = self.count(kind);
            if count == 0 {
                continue;
            }
            if count > 1 {
                buf.push_str(&count.to_string());
            }
            let mut ch = kind.to_char();
            if lower {
                ch = ch.to_ascii_lowercase();
            }
            buf.push(ch);
        }
        buf
    }
}
