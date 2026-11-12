// G2B AURA v6.0 — Tauri native entrypoint (Windows/macOS/Linux)
fn main() {
    tauri::Builder::default()
        .run(tauri::generate_context!())
        .expect("error while running G2B AURA v6.0");
}
