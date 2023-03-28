use std::io;

use super::decoder_type::DecoderType;

pub struct Header {
    pub decoder_type: DecoderType,
    pub restart_decoder: bool,
    pub uncompressed: bool,
    pub use_checksums: bool,
}

impl Header {
    pub fn parse(&mut self, input: &[u8]) -> Result<usize, io::Error> {
        let mut header_byte: u8 = input[0];

        if header_byte & 0xF != 0xC || (header_byte >> 4) & 3 != 0 {
            return Err(io::Error::from(io::ErrorKind::InvalidData));
        }

        self.restart_decoder = (header_byte >> 7) & 1 != 0;
        self.uncompressed = (header_byte >> 6) & 1 != 0;

        header_byte = input[1];

        self.decoder_type = DecoderType::from(header_byte & 0xF);
        self.use_checksums = (header_byte >> 7) & 1 != 0;

        if self.decoder_type == DecoderType::Unknown {
            return Err(io::Error::from(io::ErrorKind::InvalidData));
        }

        Ok(2)
    }
}

impl Default for Header {
    fn default() -> Self {
        Self {
            decoder_type: DecoderType::Unknown,
            restart_decoder: false,
            uncompressed: false,
            use_checksums: false,
        }
    }
}
