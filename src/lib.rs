/// # oozle
///
/// A Rust library for decompressing Kraken, Mermaid, Selkie, Leviathan,
/// LZNA and Bitknit compressed buffers.
mod decoder;
mod decompress;
mod header;

use decoder::default_oozle_decoder;

pub use decompress::decompress;

#[cxx::bridge]
mod ffi {
    struct OozleHeader {
        pub decoder_type: u32,
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
    }

    // Rust types and signatures exposed to C++.
    extern "Rust" {
        fn default_oozle_decoder() -> OozleDecoder;
    }

    // C++ types and signatures exposed to Rust.
    extern "C++" {
        include!("oozle/include/decompress.h");

        unsafe fn Oozle_Decompress(input: &[u8], output: &mut [u8]) -> i32;
    }
}
