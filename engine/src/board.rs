use core::fmt;

pub const BOARD_FILES: usize = 5;
pub const BOARD_RANKS: usize = 5;
pub const BOARD_SQUARES: usize = BOARD_FILES * BOARD_RANKS;

/// 5x5将棋盤のマス（筋・段で表現）。
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Square(u8);

impl Square {
    pub const fn from_file_rank(file: u8, rank: u8) -> Self {
        assert!(file < BOARD_FILES as u8, "file out of range");
        assert!(rank < BOARD_RANKS as u8, "rank out of range");
        Self(rank * BOARD_FILES as u8 + file)
    }

    pub const fn from_index(index: u8) -> Self {
        assert!(index < BOARD_SQUARES as u8, "index out of range");
        Self(index)
    }

    pub const fn index(self) -> u32 {
        self.0 as u32
    }

    pub const fn file(self) -> u8 {
        self.0 % BOARD_FILES as u8
    }

    pub const fn rank(self) -> u8 {
        self.0 / BOARD_FILES as u8
    }

    #[inline]
    pub fn offset(self, df: i8, dr: i8) -> Option<Self> {
        let file = self.file() as i8 + df;
        let rank = self.rank() as i8 + dr;
        if (0..BOARD_FILES as i8).contains(&file) && (0..BOARD_RANKS as i8).contains(&rank) {
            Some(Self::from_file_rank(file as u8, rank as u8))
        } else {
            None
        }
    }

    pub fn from_coord(coord: &str) -> Option<Self> {
        if coord.len() != 2 {
            return None;
        }
        let mut chars = coord.chars();
        let file_char = chars.next()?;
        let rank_char = chars.next()?;
        let file_digit = file_char.to_digit(10)?;
        if file_digit == 0 || file_digit as usize > BOARD_FILES {
            return None;
        }
        let rank_index = (rank_char as u8).wrapping_sub(b'a');
        if rank_index as usize >= BOARD_RANKS {
            return None;
        }
        Some(Self::from_file_rank((file_digit - 1) as u8, rank_index))
    }

    /// 表記 (例: 1e) を返す。
    pub fn to_coord(self) -> String {
        let file = self.file() + 1;
        let rank = (b'a' + self.rank()) as char;
        format!("{}{}", file, rank)
    }
}

impl fmt::Display for Square {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.to_coord())
    }
}

/// 左上（5a）から右下（1e）までの全マスを返す。
pub const fn all_squares() -> [Square; BOARD_SQUARES] {
    let mut squares = [Square(0); BOARD_SQUARES];
    let mut idx = 0;
    while idx < BOARD_SQUARES {
        squares[idx] = Square(idx as u8);
        idx += 1;
    }
    squares
}
