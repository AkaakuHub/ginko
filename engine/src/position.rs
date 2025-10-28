use std::fmt;

use crate::attacks;
use crate::bitboard::Bitboard;
use crate::board::{BOARD_FILES, BOARD_RANKS, BOARD_SQUARES, Square};
use crate::hand::{Hand, HandPieceKind};
use crate::moves::{Move, MoveList};
use crate::piece::{COLORS, Color, PIECE_KIND_COUNT, Piece, PieceKind};
use crate::zobrist;

pub const INITIAL_SFEN: &str = "rbsgk/4p/5/P4/KGSBR b - 1";

#[derive(Debug)]
pub enum PositionError {
    Format(&'static str),
    Message(String),
}

impl PositionError {
    pub(crate) fn message<T: Into<String>>(msg: T) -> Self {
        Self::Message(msg.into())
    }
}

impl fmt::Display for PositionError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Format(msg) => write!(f, "{}", msg),
            Self::Message(msg) => write!(f, "{}", msg),
        }
    }
}

impl std::error::Error for PositionError {}

#[derive(Clone)]
pub struct Position {
    board: [Option<Piece>; BOARD_SQUARES],
    bitboards: [[Bitboard; PIECE_KIND_COUNT]; 2],
    occupancy: [Bitboard; 2],
    hands: [Hand; 2],
    side_to_move: Color,
    ply: u32,
    hash: u64,
    history: Vec<u64>,
}

impl Position {
    pub fn empty() -> Self {
        Self {
            board: [None; BOARD_SQUARES],
            bitboards: [[Bitboard::EMPTY; PIECE_KIND_COUNT]; 2],
            occupancy: [Bitboard::EMPTY; 2],
            hands: [Hand::default(), Hand::default()],
            side_to_move: Color::Black,
            ply: 1,
            hash: 0,
            history: Vec::new(),
        }
    }

    pub fn initial() -> Result<Self, PositionError> {
        Self::from_sfen(INITIAL_SFEN)
    }

    pub fn side_to_move(&self) -> Color {
        self.side_to_move
    }

    pub fn set_side_to_move(&mut self, color: Color) {
        if color != self.side_to_move {
            self.hash ^= zobrist::side_to_move();
            self.side_to_move = color;
        }
    }

    pub fn ply(&self) -> u32 {
        self.ply
    }

    pub fn set_ply(&mut self, ply: u32) {
        self.ply = ply.max(1);
    }

    pub fn piece_at(&self, square: Square) -> Option<Piece> {
        self.board[square.index() as usize]
    }

    pub fn set_piece(&mut self, square: Square, piece: Piece) -> Result<(), PositionError> {
        if self.board[square.index() as usize].is_some() {
            return Err(PositionError::message(format!(
                "square {} is already occupied",
                square
            )));
        }
        self.board[square.index() as usize] = Some(piece);
        self.bitboards[piece.color.index()][piece.kind as usize].insert(square);
        self.occupancy[piece.color.index()].insert(square);
        self.hash ^= zobrist::piece_square(piece.color, piece.kind, square);
        Ok(())
    }

    pub fn remove_piece(&mut self, square: Square) -> Option<Piece> {
        if let Some(piece) = self.board[square.index() as usize] {
            self.hash ^= zobrist::piece_square(piece.color, piece.kind, square);
            self.board[square.index() as usize] = None;
            self.bitboards[piece.color.index()][piece.kind as usize].remove(square);
            self.occupancy[piece.color.index()].remove(square);
            Some(piece)
        } else {
            None
        }
    }

    pub fn pieces(&self, color: Color, kind: PieceKind) -> Bitboard {
        self.bitboards[color.index()][kind as usize]
    }

    pub fn occupancy(&self, color: Color) -> Bitboard {
        self.occupancy[color.index()]
    }

    pub fn occupancy_all(&self) -> Bitboard {
        self.occupancy[0] | self.occupancy[1]
    }

    pub fn king_square(&self, color: Color) -> Option<Square> {
        let mut kings = self.pieces(color, PieceKind::King);
        kings.pop()
    }

    pub fn hand(&self, color: Color) -> &Hand {
        &self.hands[color.index()]
    }

    pub fn hand_mut(&mut self, color: Color) -> &mut Hand {
        &mut self.hands[color.index()]
    }

    pub fn clear(&mut self) {
        self.board.fill(None);
        for bb in &mut self.bitboards {
            bb.fill(Bitboard::EMPTY);
        }
        self.occupancy = [Bitboard::EMPTY; 2];
        self.hands = [Hand::default(), Hand::default()];
        self.side_to_move = Color::Black;
        self.ply = 1;
        self.hash = 0;
        self.history.clear();
        self.history.push(self.hash);
    }

    fn switch_side(&mut self) {
        self.hash ^= zobrist::side_to_move();
        self.side_to_move = self.side_to_move.opponent();
    }

    fn update_hand_hash(&mut self, color: Color, kind: HandPieceKind, old: u8, new: u8) {
        self.hash ^= zobrist::hand(color, kind, old as usize);
        self.hash ^= zobrist::hand(color, kind, new as usize);
    }

    pub fn zobrist_key(&self) -> u64 {
        self.hash
    }

    pub fn current_repetition_count(&self) -> usize {
        match self.history.last() {
            Some(&last) => self.repetition_count(last),
            None => 0,
        }
    }

    pub fn repetition_count(&self, key: u64) -> usize {
        self.history.iter().filter(|&&k| k == key).count()
    }

    fn recompute_hash(&mut self) {
        self.hash = 0;
        for idx in 0..BOARD_SQUARES {
            if let Some(piece) = self.board[idx] {
                let square = Square::from_index(idx as u8);
                self.hash ^= zobrist::piece_square(piece.color, piece.kind, square);
            }
        }
        for color in COLORS {
            for hand_kind in HandPieceKind::all() {
                let count = self.hands[color.index()].count(hand_kind) as usize;
                self.hash ^= zobrist::hand(color, hand_kind, count);
            }
        }
        if self.side_to_move == Color::White {
            self.hash ^= zobrist::side_to_move();
        }
        self.history.clear();
        self.history.push(self.hash);
    }

    fn promotion_zone(color: Color, square: Square) -> bool {
        match color {
            Color::Black => square.rank() == 0,
            Color::White => square.rank() == (BOARD_RANKS as u8 - 1),
        }
    }

    fn can_promote(color: Color, kind: PieceKind, from: Square, to: Square) -> bool {
        if !kind.can_promote() {
            return false;
        }
        Self::promotion_zone(color, from) || Self::promotion_zone(color, to)
    }

    fn must_promote(color: Color, kind: PieceKind, to: Square) -> bool {
        matches!(kind, PieceKind::Pawn) && Self::promotion_zone(color, to)
    }

    fn piece_effect_contains(
        square: Square,
        color: Color,
        kind: PieceKind,
        target: Square,
        occ: Bitboard,
    ) -> bool {
        let attacks = match kind {
            PieceKind::King => attacks::king_attacks(square),
            PieceKind::Gold | PieceKind::PromotedSilver | PieceKind::Tokin => {
                attacks::gold_attacks(color, square)
            }
            PieceKind::Silver => attacks::silver_attacks(color, square),
            PieceKind::Bishop => attacks::bishop_attacks(square, occ),
            PieceKind::PromotedBishop => attacks::horse_attacks(square, occ),
            PieceKind::Rook => attacks::rook_attacks(square, occ),
            PieceKind::PromotedRook => attacks::dragon_attacks(square, occ),
            PieceKind::Pawn => attacks::pawn_attacks(color, square),
        };
        attacks.contains(target)
    }

    fn is_square_attacked(&self, square: Square, by: Color) -> bool {
        let occ = self.occupancy_all();
        for kind in PieceKind::all() {
            let mut pieces = self.pieces(by, kind);
            while let Some(src) = pieces.pop() {
                if Self::piece_effect_contains(src, by, kind, square, occ) {
                    return true;
                }
            }
        }
        false
    }

    pub fn is_in_check(&self, color: Color) -> bool {
        if let Some(king_sq) = self.king_square(color) {
            self.is_square_attacked(king_sq, color.opponent())
        } else {
            false
        }
    }

    fn apply_move_internal(&mut self, mv: &Move) -> Result<(), PositionError> {
        let color = self.side_to_move;

        if mv.is_drop() {
            let hand_kind = HandPieceKind::from_piece_kind(mv.piece)
                .ok_or_else(|| PositionError::message("cannot drop this piece"))?;
            if self.hands[color.index()].count(hand_kind) == 0 {
                return Err(PositionError::message("no piece in hand for drop"));
            }
            if self.piece_at(mv.to).is_some() {
                return Err(PositionError::message("drop target not empty"));
            }
            let old = self.hands[color.index()].count(hand_kind);
            let new = {
                let hand = &mut self.hands[color.index()];
                hand.remove(hand_kind, 1)
            };
            self.update_hand_hash(color, hand_kind, old, new);
            self.set_piece(mv.to, Piece::new(color, mv.piece))?;
        } else {
            let from = mv
                .from
                .ok_or_else(|| PositionError::message("missing from square"))?;
            let moving_piece = self
                .piece_at(from)
                .ok_or_else(|| PositionError::message("no piece on from square"))?;
            if moving_piece.color != color {
                return Err(PositionError::message("moving opponent piece"));
            }

            let mut resulting_kind = moving_piece.kind;
            if mv.promote {
                resulting_kind = resulting_kind
                    .promote()
                    .ok_or_else(|| PositionError::message("piece cannot promote"))?;
            }

            if let Some(target_piece) = self.piece_at(mv.to) {
                if target_piece.color == color {
                    return Err(PositionError::message("cannot capture own piece"));
                }
                self.remove_piece(mv.to);
                if let Some(hand_kind) = HandPieceKind::from_piece_kind(target_piece.kind.base()) {
                    let old = self.hands[color.index()].count(hand_kind);
                    let new = {
                        let hand = &mut self.hands[color.index()];
                        hand.add(hand_kind, 1)
                    };
                    self.update_hand_hash(color, hand_kind, old, new);
                }
            }

            self.remove_piece(from)
                .ok_or_else(|| PositionError::message("piece missing"))?;
            self.set_piece(mv.to, Piece::new(color, resulting_kind))?;
        }

        self.switch_side();
        self.ply += 1;
        self.history.push(self.hash);
        Ok(())
    }

    pub fn play_move(&self, mv: &Move) -> Result<Self, PositionError> {
        let mut next = self.clone();
        next.apply_move_internal(mv)?;
        Ok(next)
    }

    pub fn play_move_mut(&mut self, mv: &Move) -> Result<(), PositionError> {
        self.apply_move_internal(mv)
    }

    fn is_move_legal_internal(
        &self,
        mv: &Move,
        enforce_drop_rule: bool,
    ) -> Result<bool, PositionError> {
        let mover = self.side_to_move;
        let next = self.play_move(mv)?;
        if next.is_in_check(mover) {
            return Ok(false);
        }

        if enforce_drop_rule
            && mv.is_drop()
            && mv.piece == PieceKind::Pawn
            && next.is_in_check(mover.opponent())
        {
            if !next.has_any_legal_move_internal(true)? {
                return Ok(false);
            }
        }

        Ok(true)
    }

    fn has_any_legal_move_internal(&self, enforce_drop_rule: bool) -> Result<bool, PositionError> {
        for mv in self.generate_pseudo_legal_moves() {
            if self.is_move_legal_internal(&mv, enforce_drop_rule)? {
                return Ok(true);
            }
        }
        Ok(false)
    }

    pub fn generate_legal_moves(&self) -> Result<MoveList, PositionError> {
        let mut result = MoveList::new();
        for mv in self.generate_pseudo_legal_moves() {
            if self.is_move_legal_internal(&mv, true)? {
                result.push(mv);
            }
        }
        Ok(result)
    }

    fn generate_piece_moves(
        &self,
        color: Color,
        kind: PieceKind,
        mut pieces: Bitboard,
        moves: &mut MoveList,
    ) {
        if pieces.is_empty() {
            return;
        }
        let our_occ = self.occupancy(color);
        let all_occ = self.occupancy_all();

        while let Some(from) = pieces.pop() {
            let attacks = match kind {
                PieceKind::King => attacks::king_attacks(from),
                PieceKind::Gold | PieceKind::PromotedSilver | PieceKind::Tokin => {
                    attacks::gold_attacks(color, from)
                }
                PieceKind::Silver => attacks::silver_attacks(color, from),
                PieceKind::Bishop => attacks::bishop_attacks(from, all_occ),
                PieceKind::PromotedBishop => attacks::horse_attacks(from, all_occ),
                PieceKind::Rook => attacks::rook_attacks(from, all_occ),
                PieceKind::PromotedRook => attacks::dragon_attacks(from, all_occ),
                PieceKind::Pawn => attacks::pawn_attacks(color, from),
            };

            let mut targets = attacks & !our_occ;
            while let Some(to) = targets.pop() {
                let promote_forced = Self::must_promote(color, kind, to);
                let can_promote = Self::can_promote(color, kind, from, to);
                if promote_forced {
                    moves.push(Move::normal(from, to, kind, true));
                } else {
                    moves.push(Move::normal(from, to, kind, false));
                    if can_promote {
                        moves.push(Move::normal(from, to, kind, true));
                    }
                }
            }
        }
    }

    fn has_pawn_on_file(&self, color: Color, file: u8) -> bool {
        let mut pawns = self.pieces(color, PieceKind::Pawn);
        while let Some(square) = pawns.pop() {
            if square.file() == file {
                return true;
            }
        }
        false
    }

    fn generate_drop_moves(&self, color: Color, moves: &mut MoveList) {
        let mut empty = !self.occupancy_all();
        while let Some(to) = empty.pop() {
            for hand_kind in HandPieceKind::all() {
                let count = self.hand(color).count(hand_kind);
                if count == 0 {
                    continue;
                }
                let piece_kind = match hand_kind {
                    HandPieceKind::Gold => PieceKind::Gold,
                    HandPieceKind::Silver => PieceKind::Silver,
                    HandPieceKind::Bishop => PieceKind::Bishop,
                    HandPieceKind::Rook => PieceKind::Rook,
                    HandPieceKind::Pawn => PieceKind::Pawn,
                };

                if piece_kind == PieceKind::Pawn {
                    if Self::promotion_zone(color, to) {
                        continue;
                    }
                    if self.has_pawn_on_file(color, to.file()) {
                        continue;
                    }
                }

                moves.push(Move::drop(to, piece_kind));
            }
        }
    }

    pub fn generate_pseudo_legal_moves(&self) -> MoveList {
        let mut moves = MoveList::new();
        let color = self.side_to_move;

        for kind in PieceKind::all() {
            let pieces = self.pieces(color, kind);
            if pieces.is_empty() {
                continue;
            }
            self.generate_piece_moves(color, kind, pieces, &mut moves);
        }

        self.generate_drop_moves(color, &mut moves);
        moves
    }

    pub fn to_sfen(&self) -> String {
        let mut ranks = Vec::with_capacity(BOARD_RANKS);
        for rank in 0..BOARD_RANKS {
            let mut empties = 0;
            let mut row = String::new();
            for file in (0..BOARD_FILES).rev() {
                let square = Square::from_file_rank(file as u8, rank as u8);
                if let Some(piece) = self.piece_at(square) {
                    if empties > 0 {
                        row.push_str(&empties.to_string());
                        empties = 0;
                    }
                    row.push_str(&piece.to_sfen());
                } else {
                    empties += 1;
                }
            }
            if empties > 0 {
                row.push_str(&empties.to_string());
            }
            ranks.push(row);
        }

        let hand_str = {
            let upper = self.hands[Color::Black.index()].to_sfen(false);
            let lower = self.hands[Color::White.index()].to_sfen(true);
            if upper.is_empty() && lower.is_empty() {
                "-".to_string()
            } else {
                format!("{}{}", upper, lower)
            }
        };

        format!(
            "{} {} {} {}",
            ranks.join("/"),
            match self.side_to_move {
                Color::Black => "b",
                Color::White => "w",
            },
            hand_str,
            self.ply
        )
    }

    pub fn from_sfen(s: &str) -> Result<Self, PositionError> {
        let mut parts = s.split_whitespace();
        let board_part = parts.next().ok_or(PositionError::Format("missing board"))?;
        let turn_part = parts.next().ok_or(PositionError::Format("missing turn"))?;
        let hand_part = parts.next().ok_or(PositionError::Format("missing hands"))?;
        let ply_part = parts.next().ok_or(PositionError::Format("missing ply"))?;

        if parts.next().is_some() {
            return Err(PositionError::Format("Too many fields in SFEN"));
        }

        let mut position = Self::empty();

        // board parsing
        let ranks: Vec<&str> = board_part.split('/').collect();
        if ranks.len() != BOARD_RANKS {
            return Err(PositionError::message(format!(
                "board has {} ranks, expected {}",
                ranks.len(),
                BOARD_RANKS
            )));
        }

        for (rank_idx, rank_str) in ranks.iter().enumerate() {
            let mut file: i32 = (BOARD_FILES as i32) - 1;
            let mut chars = rank_str.chars().peekable();
            while let Some(ch) = chars.next() {
                if ch.is_ascii_digit() {
                    let skip = ch
                        .to_digit(10)
                        .ok_or_else(|| PositionError::message("invalid digit"))?
                        as i32;
                    if skip <= 0 || skip > file + 1 {
                        return Err(PositionError::message(format!(
                            "invalid empty count {} in rank {}",
                            skip, rank_idx
                        )));
                    }
                    file -= skip;
                    continue;
                }

                let promoted = if ch == '+' {
                    let Some(next) = chars.next() else {
                        return Err(PositionError::Format("dangling promotion marker"));
                    };
                    let result = Self::place_board_piece(&mut position, next, true, rank_idx, file);
                    file -= 1;
                    result?;
                    continue;
                } else {
                    false
                };

                Self::place_board_piece(&mut position, ch, promoted, rank_idx, file)?;
                file -= 1;
            }

            if file != -1 {
                return Err(PositionError::message(format!(
                    "rank {} does not cover all files",
                    rank_idx
                )));
            }
        }

        position.side_to_move = match turn_part {
            "b" | "B" => Color::Black,
            "w" | "W" => Color::White,
            _ => return Err(PositionError::message("turn must be b or w")),
        };

        position.parse_hands(hand_part)?;

        position.ply = ply_part
            .parse()
            .map_err(|_| PositionError::message("invalid ply"))?;
        if position.ply == 0 {
            position.ply = 1;
        }

        position.recompute_hash();

        Ok(position)
    }

    fn place_board_piece(
        position: &mut Position,
        ch: char,
        promoted: bool,
        rank_idx: usize,
        file: i32,
    ) -> Result<(), PositionError> {
        if file < 0 {
            return Err(PositionError::message("too many squares in rank"));
        }
        let color = if ch.is_ascii_uppercase() {
            Color::Black
        } else {
            Color::White
        };
        let letter = ch.to_ascii_lowercase();
        let kind = PieceKind::from_sfen_letter(letter, promoted)
            .ok_or_else(|| PositionError::message(format!("invalid piece letter '{}'", ch)))?;
        let square = Square::from_file_rank(file as u8, rank_idx as u8);
        position.set_piece(square, Piece::new(color, kind))
    }

    fn parse_hands(&mut self, hand_part: &str) -> Result<(), PositionError> {
        if hand_part == "-" {
            return Ok(());
        }

        let mut count_buf = String::new();
        for ch in hand_part.chars() {
            if ch.is_ascii_digit() {
                count_buf.push(ch);
                continue;
            }

            let count: u8 = if count_buf.is_empty() {
                1
            } else {
                count_buf
                    .parse()
                    .map_err(|_| PositionError::message("invalid hand count"))?
            };
            count_buf.clear();

            let (color, uppercase) = if ch.is_ascii_uppercase() {
                (Color::Black, ch)
            } else {
                (Color::White, ch.to_ascii_uppercase())
            };

            let hand_kind = match uppercase {
                'G' => HandPieceKind::Gold,
                'S' => HandPieceKind::Silver,
                'B' => HandPieceKind::Bishop,
                'R' => HandPieceKind::Rook,
                'P' => HandPieceKind::Pawn,
                _ => return Err(PositionError::message("invalid hand piece")),
            };

            self.hands[color.index()].add(hand_kind, count);
        }

        if !count_buf.is_empty() {
            return Err(PositionError::message("dangling hand count"));
        }

        Ok(())
    }
}

impl Default for Position {
    fn default() -> Self {
        Self::empty()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::hand::HandPieceKind;

    #[test]
    fn initial_sfen_roundtrip() {
        let position = Position::initial().expect("initial position");
        assert_eq!(position.to_sfen(), INITIAL_SFEN);
    }

    #[test]
    fn parse_custom_sfen() {
        let sfen = "5/5/5/5/5 w Pp 42";
        let position = Position::from_sfen(sfen).expect("parse");
        assert_eq!(position.side_to_move(), Color::White);
        assert_eq!(position.hand(Color::Black).count(HandPieceKind::Pawn), 1);
        assert_eq!(position.hand(Color::White).count(HandPieceKind::Pawn), 1);
        assert_eq!(position.ply(), 42);
        assert_eq!(position.to_sfen(), sfen);
    }

    #[test]
    fn initial_position_has_moves() {
        let position = Position::initial().expect("initial");
        let moves = position.generate_legal_moves().expect("legal moves");
        assert_eq!(moves.len(), 14);
    }
}
