use crate::board::Square;

/// 5x5将棋盤用の25ビットビットボード。
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct Bitboard(u32);

impl Bitboard {
    pub const EMPTY: Self = Self(0);
    pub const FULL: Self = Self((1u32 << 25) - 1);

    #[inline]
    pub const fn from_bits(bits: u32) -> Self {
        Self(bits & Self::FULL.0)
    }

    #[inline]
    pub const fn to_bits(self) -> u32 {
        self.0
    }

    #[inline]
    pub const fn from_square(square: Square) -> Self {
        Self(1u32 << square.index())
    }

    #[inline]
    pub fn contains(self, square: Square) -> bool {
        (self.0 & (1u32 << square.index())) != 0
    }

    #[inline]
    pub fn insert(&mut self, square: Square) {
        self.0 |= 1u32 << square.index();
    }

    #[inline]
    pub fn remove(&mut self, square: Square) {
        self.0 &= !(1u32 << square.index());
    }

    #[inline]
    pub fn is_empty(self) -> bool {
        self.0 == 0
    }

    #[inline]
    pub fn iter(self) -> BitboardIter {
        BitboardIter(self.0)
    }

    #[inline]
    pub fn pop(&mut self) -> Option<Square> {
        if self.0 == 0 {
            None
        } else {
            let lsb = self.0.trailing_zeros();
            self.0 &= self.0 - 1;
            Some(Square::from_index(lsb as u8))
        }
    }
}

impl core::ops::BitOr for Bitboard {
    type Output = Self;

    fn bitor(self, rhs: Self) -> Self::Output {
        Self(self.0 | rhs.0)
    }
}

impl core::ops::BitOrAssign for Bitboard {
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0;
    }
}

impl core::ops::BitAnd for Bitboard {
    type Output = Self;

    fn bitand(self, rhs: Self) -> Self::Output {
        Self(self.0 & rhs.0)
    }
}

impl core::ops::BitAndAssign for Bitboard {
    fn bitand_assign(&mut self, rhs: Self) {
        self.0 &= rhs.0;
    }
}

impl core::ops::BitXor for Bitboard {
    type Output = Self;

    fn bitxor(self, rhs: Self) -> Self::Output {
        Self(self.0 ^ rhs.0)
    }
}

impl core::ops::BitXorAssign for Bitboard {
    fn bitxor_assign(&mut self, rhs: Self) {
        self.0 ^= rhs.0;
    }
}

impl core::ops::Not for Bitboard {
    type Output = Self;

    fn not(self) -> Self::Output {
        Self(!self.0 & Self::FULL.0)
    }
}

pub struct BitboardIter(u32);

impl Iterator for BitboardIter {
    type Item = Square;

    fn next(&mut self) -> Option<Self::Item> {
        if self.0 == 0 {
            return None;
        }
        let lsb = self.0.trailing_zeros();
        self.0 &= self.0 - 1;
        Some(Square::from_index(lsb as u8))
    }
}
