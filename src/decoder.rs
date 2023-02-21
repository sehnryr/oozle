use crate::ffi;

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
