/// # oozle
///
/// A Rust library for decompressing Kraken, Mermaid, Selkie, Leviathan,
/// LZNA and Bitknit compressed buffers.
mod decoder;
mod decompress;
mod header;

use header::{parse_header, parse_quantum_header};

pub use decompress::decompress;

#[cxx::bridge]
mod ffi {
    enum OozleDecoderType {
        LZNA = 5,
        Kraken = 6,
        Mermaid = 10,
        Bitknit = 11,
        Leviathan = 12,
        Unknown = 0,
    }

    struct OozleHeader {
        pub decoder_type: OozleDecoderType,
        pub restart_decoder: bool,
        pub uncompressed: bool,
        pub use_checksums: bool,
    }

    struct OozleQuantumHeader {
        pub compressed_size: u32,
        pub checksum: u32,
        pub flag1: u8,
        pub flag2: u8,
        pub whole_match_distance: u32,
    }

    struct OozleDecoder {
        pub input_read: u32,
        pub output_written: u32,
        pub scratch: [u8; 0x6C000],
        pub header: OozleHeader,
        pub quantum_header: OozleQuantumHeader,
    }

    // Rust types and signatures exposed to C++.
    extern "Rust" {
        fn parse_header(decoder: &mut OozleDecoder, input: &[u8]) -> Result<usize>;
        fn parse_quantum_header(decoder: &mut OozleDecoder, input: &[u8]) -> Result<usize>;
    }

    // C++ types and signatures exposed to Rust.
    extern "C++" {
        include!("oozle/include/decompress.h");

        unsafe fn Oozle_DecodeStep(
            decoder: &mut OozleDecoder,
            dst_start: *mut u8,
            offset: i32,
            dst_bytes_left_in: usize,
            src: *const u8,
            src_bytes_left: usize,
        ) -> bool;
    }
}
