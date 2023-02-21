use crate::ffi;
use std::io;

impl From<u8> for ffi::OozleDecoderType {
    fn from(value: u8) -> Self {
        match value {
            5 => ffi::OozleDecoderType::LZNA,
            6 => ffi::OozleDecoderType::Kraken,
            10 => ffi::OozleDecoderType::Mermaid,
            11 => ffi::OozleDecoderType::Bitknit,
            12 => ffi::OozleDecoderType::Leviathan,
            _ => ffi::OozleDecoderType::Unknown,
        }
    }
}

impl ffi::OozleDecoder {
    pub fn parse_header(&mut self, input: &[u8]) -> Result<usize, io::Error> {
        self.header.parse(input)
    }

    pub fn parse_quantum_header(&mut self, input: &[u8]) -> Result<usize, io::Error> {
        self.quantum_header.parse(input, self.header.use_checksums)
    }
}

impl Default for ffi::OozleDecoder {
    fn default() -> Self {
        Self {
            input_read: 0,
            output_written: 0,
            scratch: [0; 0x6C000],
            header: ffi::OozleHeader::default(),
            quantum_header: ffi::OozleQuantumHeader::default(),
        }
    }
}

/* Rust functions exposed to C++ */
pub fn parse_header(decoder: &mut ffi::OozleDecoder, input: &[u8]) -> Result<usize, io::Error> {
    decoder.parse_header(input)
}

pub fn parse_quantum_header(
    decoder: &mut ffi::OozleDecoder,
    input: &[u8],
) -> Result<usize, io::Error> {
    decoder.parse_quantum_header(input)
}
