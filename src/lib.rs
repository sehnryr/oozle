mod decoder;
mod decompress;

pub use decompress::decompress;

#[cxx::bridge]
mod ffi {
    // C++ types and signatures exposed to Rust.
    extern "C++" {
        include!("oozle/include/decompress.h");

        type LznaState;
        type BitknitState;

        unsafe fn Kraken_DecodeQuantum(
            dst: *mut u8,
            dst_end: *mut u8,
            dst_start: *mut u8,
            src: *const u8,
            src_end: *const u8,
            scratch: *mut u8,
            scratch_end: *mut u8,
        ) -> i32;
        unsafe fn Mermaid_DecodeQuantum(
            dst: *mut u8,
            dst_end: *mut u8,
            dst_start: *mut u8,
            src: *const u8,
            src_end: *const u8,
            temp: *mut u8,
            temp_end: *mut u8,
        ) -> i32;
        unsafe fn Leviathan_DecodeQuantum(
            dst: *mut u8,
            dst_end: *mut u8,
            dst_start: *mut u8,
            src: *const u8,
            src_end: *const u8,
            scratch: *mut u8,
            scratch_end: *mut u8,
        ) -> i32;

        unsafe fn LZNA_InitLookup(lut: *mut LznaState);
        unsafe fn LZNA_DecodeQuantum(
            dst: *mut u8,
            dst_end: *mut u8,
            dst_start: *mut u8,
            src: *const u8,
            src_end: *const u8,
            lut: *mut LznaState,
        ) -> i32;

        unsafe fn BitknitState_Init(bk: *mut BitknitState);
        unsafe fn Bitknit_Decode(
            src: *const u8,
            src_end: *const u8,
            dst: *mut u8,
            dst_end: *mut u8,
            dst_start: *mut u8,
            bk: *mut BitknitState,
        ) -> usize;
    }
}
