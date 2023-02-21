/// # oozle
///
/// A Rust library for decompressing Kraken, Mermaid, Selkie, Leviathan,
/// LZNA and Bitknit compressed buffers.
mod decompress;

pub use decompress::decompress;

#[cxx::bridge]
mod ffi {
    // C++ types and signatures exposed to Rust.
    extern "C++" {
        include!("oozle/include/decompress.h");

        unsafe fn Oozle_Decompress(
            src: *const u8,
            src_len: usize,
            dst: *mut u8,
            dst_len: usize,
        ) -> i32;
    }
}
