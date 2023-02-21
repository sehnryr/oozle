use crate::ffi;

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
