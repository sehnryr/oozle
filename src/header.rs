use crate::ffi;
use std::io;

impl ffi::OozleHeader {
    pub fn parse(&mut self, input: &[u8]) -> Result<usize, io::Error> {
        let mut header_byte: u8 = input[0];

        if header_byte & 0xF != 0xC || (header_byte >> 4) & 3 != 0 {
            return Err(io::Error::from(io::ErrorKind::InvalidData));
        }

        self.restart_decoder = (header_byte >> 7) & 1 != 0;
        self.uncompressed = (header_byte >> 6) & 1 != 0;

        header_byte = input[1];

        self.decoder_type = ffi::OozleDecoderType::from(header_byte & 0xF);
        self.use_checksums = (header_byte >> 7) & 1 != 0;

        if self.decoder_type == ffi::OozleDecoderType::Unknown {
            return Err(io::Error::from(io::ErrorKind::InvalidData));
        }

        Ok(2)
    }
}

impl Default for ffi::OozleHeader {
    fn default() -> Self {
        Self {
            decoder_type: ffi::OozleDecoderType::Unknown,
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

/* Rust functions exposed to C++ */
pub fn parse_header(decoder: &mut ffi::OozleDecoder, input: &[u8]) -> Result<usize, io::Error> {
    decoder.header.parse(input)
}
