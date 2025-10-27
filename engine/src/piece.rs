use core::fmt;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Color {
    Black = 0,
    White = 1,
}

impl Color {
    pub const fn index(self) -> usize {
        self as usize
    }

    pub const fn opponent(self) -> Self {
        match self {
            Self::Black => Self::White,
            Self::White => Self::Black,
        }
    }
}

pub const COLORS: [Color; 2] = [Color::Black, Color::White];

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PieceKind {
    King = 0,
    Gold = 1,
    Silver = 2,
    PromotedSilver = 3,
    Bishop = 4,
    PromotedBishop = 5,
    Rook = 6,
    PromotedRook = 7,
    Pawn = 8,
    Tokin = 9,
}

pub const PIECE_KIND_COUNT: usize = 10;

impl PieceKind {
    pub const fn index(self) -> usize {
        self as usize
    }

    pub const fn all() -> [Self; 10] {
        [
            Self::King,
            Self::Gold,
            Self::Silver,
            Self::PromotedSilver,
            Self::Bishop,
            Self::PromotedBishop,
            Self::Rook,
            Self::PromotedRook,
            Self::Pawn,
            Self::Tokin,
        ]
    }

    pub const fn is_promoted(self) -> bool {
        matches!(
            self,
            Self::PromotedSilver | Self::PromotedBishop | Self::PromotedRook | Self::Tokin
        )
    }

    pub const fn can_promote(self) -> bool {
        matches!(self, Self::Silver | Self::Bishop | Self::Rook | Self::Pawn)
    }

    pub const fn promote(self) -> Option<Self> {
        match self {
            Self::Silver => Some(Self::PromotedSilver),
            Self::Bishop => Some(Self::PromotedBishop),
            Self::Rook => Some(Self::PromotedRook),
            Self::Pawn => Some(Self::Tokin),
            _ => None,
        }
    }

    pub const fn demote(self) -> Option<Self> {
        match self {
            Self::PromotedSilver => Some(Self::Silver),
            Self::PromotedBishop => Some(Self::Bishop),
            Self::PromotedRook => Some(Self::Rook),
            Self::Tokin => Some(Self::Pawn),
            _ => None,
        }
    }

    pub const fn base(self) -> Self {
        match self {
            Self::PromotedSilver => Self::Silver,
            Self::PromotedBishop => Self::Bishop,
            Self::PromotedRook => Self::Rook,
            Self::Tokin => Self::Pawn,
            other => other,
        }
    }

    pub fn sfen_letter(self) -> char {
        match self.base() {
            Self::King => 'k',
            Self::Gold => 'g',
            Self::Silver => 's',
            Self::Bishop => 'b',
            Self::Rook => 'r',
            Self::Pawn => 'p',
            _ => unreachable!("base() guarantees unpromoted piece"),
        }
    }

    pub fn from_sfen_letter(ch: char, promoted: bool) -> Option<Self> {
        let base = match ch.to_ascii_lowercase() {
            'k' => Self::King,
            'g' => Self::Gold,
            's' => Self::Silver,
            'b' => Self::Bishop,
            'r' => Self::Rook,
            'p' => Self::Pawn,
            _ => return None,
        };
        if promoted { base.promote() } else { Some(base) }
    }

    pub fn drop_char(self) -> Option<char> {
        match self {
            Self::Gold => Some('G'),
            Self::Silver => Some('S'),
            Self::Bishop => Some('B'),
            Self::Rook => Some('R'),
            Self::Pawn => Some('P'),
            _ => None,
        }
    }

    pub fn from_drop_char(ch: char) -> Option<Self> {
        match ch.to_ascii_uppercase() {
            'G' => Some(Self::Gold),
            'S' => Some(Self::Silver),
            'B' => Some(Self::Bishop),
            'R' => Some(Self::Rook),
            'P' => Some(Self::Pawn),
            _ => None,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Piece {
    pub color: Color,
    pub kind: PieceKind,
}

impl Piece {
    pub const fn new(color: Color, kind: PieceKind) -> Self {
        Self { color, kind }
    }

    pub fn to_sfen(self) -> String {
        let mut txt = String::new();
        if self.kind.is_promoted() {
            txt.push('+');
        }
        let mut ch = self.kind.sfen_letter();
        if matches!(self.color, Color::Black) {
            ch = ch.to_ascii_uppercase();
        }
        txt.push(ch);
        txt
    }
}

impl fmt::Display for Piece {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.to_sfen())
    }
}
