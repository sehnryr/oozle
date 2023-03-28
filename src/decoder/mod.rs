mod common;
mod decoder;
mod header;
mod quantum_header;

pub use decoder::Decoder;

#[derive(Debug, PartialEq, Eq)]
pub(super) enum DecoderType {
    LZNA = 5,
    Kraken = 6,
    Mermaid = 10,
    Bitknit = 11,
    Leviathan = 12,
    Unknown = 0,
}

impl From<u8> for DecoderType {
    fn from(value: u8) -> Self {
        match value {
            5 => DecoderType::LZNA,
            6 => DecoderType::Kraken,
            10 => DecoderType::Mermaid,
            11 => DecoderType::Bitknit,
            12 => DecoderType::Leviathan,
            _ => DecoderType::Unknown,
        }
    }
}
