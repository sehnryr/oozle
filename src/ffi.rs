#[cxx::bridge]
mod ffi {
    // C++ types and signatures exposed to Rust.
    unsafe extern "C++" {
        include!("oozle/include/kraken.h");

        unsafe fn Kraken_Decompress(
            src: *const u8,
            src_len: usize,
            dst: *mut u8,
            dst_len: usize,
        ) -> i32;
    }
}

pub use ffi::*;
