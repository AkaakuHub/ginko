fn main() {
    if let Err(err) = engine::usi::run() {
        eprintln!("error: {err}");
    }
}
