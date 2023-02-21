/// # oozle
///
/// A Rust library for decompressing Kraken, Mermaid, Selkie, Leviathan,
/// LZNA and Bitknit compressed buffers.
mod decompress;

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
        pub src_used: u32,
        pub dst_used: u32,

        // pub scratch: &mut [u8; 0x6C000],
        pub scratch: *mut u8,
        pub scratch_size: usize,

        pub hdr: OozleHeader,
    }

    // Rust types and signatures exposed to C++.
    extern "Rust" {
        fn default_oozle_decoder() -> OozleDecoder;
    }

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

impl Default for ffi::OozleHeader {
    fn default() -> Self {
        Self {
            decoder_type: 0,
            restart_decoder: false,
            uncompressed: false,
            use_checksums: false,
        }
    }
}

impl Default for ffi::OozleQuantumHeader {
    fn default() -> Self {
        Self {
            compressed_size: 0,
            checksum: 0,
            flag1: 0,
            flag2: 0,
            whole_match_distance: 0,
        }
    }
}

impl Default for ffi::OozleDecoder {
    fn default() -> Self {
        Self {
            src_used: 0,
            dst_used: 0,
            // TODO: Change this to a slice.
            scratch: std::ptr::null_mut(),
            scratch_size: 0x6C000,
            hdr: ffi::OozleHeader::default(),
        }
    }
}

fn default_oozle_decoder() -> ffi::OozleDecoder {
    ffi::OozleDecoder::default()
}
