/// # oozle
/// 
/// A Rust library for decompressing Kraken, Mermaid, Selkie, Leviathan,
/// LZNA and Bitknit compressed buffers.

mod ffi;
mod decompress;

pub use decompress::decompress;
