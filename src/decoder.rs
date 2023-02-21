use crate::ffi;

impl Default for ffi::OozleDecoder {
    fn default() -> Self {
        Self {
            input_read: 0,
            output_written: 0,
            scratch: [0; 0x6C000],
            scratch_size: 0x6C000,
            header: ffi::OozleHeader::default(),
        }
    }
}

/* Rust types and signatures exposed to C++ */
pub fn default_oozle_decoder() -> ffi::OozleDecoder {
    ffi::OozleDecoder::default()
}
