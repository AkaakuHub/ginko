use std::error::Error;
use std::io::{self, BufRead, Write};

use crate::moves::Move;
use crate::position::{Position, PositionError};
use crate::search::{SearchLimits, Searcher};

pub struct UsiEngine {
    position: Position,
    searcher: Searcher,
    default_limits: SearchLimits,
}

impl UsiEngine {
    pub fn new() -> Result<Self, PositionError> {
        Ok(Self {
            position: Position::initial()?,
            searcher: Searcher::new(),
            default_limits: SearchLimits::default(),
        })
    }

    fn reset(&mut self) -> Result<(), PositionError> {
        self.position = Position::initial()?;
        Ok(())
    }

    fn parse_position(&mut self, tokens: &[&str]) -> Result<(), PositionError> {
        if tokens.is_empty() {
            return Err(PositionError::Format("position requires arguments"));
        }
        let mut idx = 0;
        match tokens[idx] {
            "startpos" => {
                self.reset()?;
                idx += 1;
            }
            "sfen" => {
                if tokens.len() < idx + 5 {
                    return Err(PositionError::Format("invalid sfen command"));
                }
                let sfen = tokens[idx + 1..idx + 5].join(" ");
                self.position = Position::from_sfen(&sfen)?;
                idx += 5;
            }
            _ => return Err(PositionError::Format("unknown position command")),
        }

        if idx < tokens.len() && tokens[idx] == "moves" {
            idx += 1;
            while idx < tokens.len() {
                let mv = self.find_legal_move(tokens[idx])?;
                self.position.play_move_mut(&mv)?;
                idx += 1;
            }
        }
        Ok(())
    }

    fn find_legal_move(&self, token: &str) -> Result<Move, PositionError> {
        let legal_moves = self.position.generate_legal_moves()?;
        legal_moves
            .into_iter()
            .find(|mv| mv.to_usi() == token)
            .ok_or_else(|| PositionError::message(format!("illegal move: {token}")))
    }

    fn parse_go_limits(&self, args: &[&str]) -> SearchLimits {
        let mut depth = None;
        let mut randomness = None;
        let mut iter = args.iter();
        while let Some(&token) = iter.next() {
            if token.eq_ignore_ascii_case("depth") {
                if let Some(&value) = iter.next() {
                    if let Ok(parsed) = value.parse::<usize>() {
                        depth = Some(parsed.max(1));
                    }
                }
            } else if token.eq_ignore_ascii_case("random") {
                if let Some(&value) = iter.next() {
                    if let Ok(parsed) = value.parse::<i32>() {
                        randomness = Some(parsed.max(0));
                    }
                }
            }
        }
        SearchLimits {
            depth: depth.unwrap_or(self.default_limits.depth),
            randomness: randomness.unwrap_or(self.default_limits.randomness),
        }
    }

    fn legal_moves(&self) -> Result<(Vec<String>, bool), PositionError> {
        let moves = self.position.generate_legal_moves()?;
        let move_strings = moves.into_iter().map(|mv| mv.to_usi()).collect();
        let in_check = self.position.is_in_check(self.position.side_to_move());
        Ok((move_strings, in_check))
    }

    fn go(&mut self, args: &[&str]) -> Result<String, PositionError> {
        let limits = self.parse_go_limits(args);
        let result = self.searcher.search(&self.position, limits)?;
        if let Some(best) = result.best_move {
            let move_txt = best.to_usi();
            self.position.play_move_mut(&best)?;
            Ok(move_txt)
        } else {
            Ok("resign".to_string())
        }
    }
}

pub fn run() -> Result<(), Box<dyn Error>> {
    let stdin = io::stdin();
    let mut engine = UsiEngine::new()?;
    let mut last_bestmove: Option<String> = None;

    for line in stdin.lock().lines() {
        let line = line?;
        let trimmed = line.trim();
        if trimmed.is_empty() {
            continue;
        }
        let mut parts = trimmed.split_whitespace();
        let command = parts.next().unwrap();
        let args: Vec<&str> = parts.collect();

        match command {
            "usi" => {
                println!("id name Ginko5x5");
                println!("id author AkaakuHub");
                println!("usiok");
            }
            "isready" => {
                println!("readyok");
            }
            "usinewgame" => {
                engine.reset()?;
            }
            "position" => {
                if let Err(err) = engine.parse_position(&args) {
                    println!("info string position error: {err}");
                }
            }
            "legalmoves" => match engine.legal_moves() {
                Ok((moves, in_check)) => {
                    if moves.is_empty() {
                        println!("legalmoves");
                    } else {
                        println!("legalmoves {}", moves.join(" "));
                    }
                    println!("checkstate {}", if in_check { "true" } else { "false" });
                }
                Err(err) => {
                    println!("info string legalmoves error: {err}");
                }
            },
            "go" => match engine.go(&args) {
                Ok(best) => {
                    println!("bestmove {best}");
                    last_bestmove = Some(best);
                }
                Err(err) => {
                    println!("info string go error: {err}");
                    println!("bestmove resign");
                    last_bestmove = Some("resign".to_string());
                }
            },
            "stop" => {
                if let Some(best) = last_bestmove.as_deref() {
                    println!("bestmove {best}");
                } else {
                    println!("bestmove resign");
                }
            }
            "setoption" => {
                // Options are not implemented yet.
            }
            "quit" => break,
            _ => {
                println!("info string unknown command: {command}");
            }
        }
        io::stdout().flush().ok();
    }

    Ok(())
}
