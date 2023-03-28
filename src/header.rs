use std::io;

use crate::decoder::DecoderType;

pub struct Header {
    pub decoder_type: DecoderType,
    pub restart_decoder: bool,
    pub uncompressed: bool,
    pub use_checksums: bool,
}

pub struct QuantumHeader {
    pub compressed_size: u32,
    pub checksum: u32,
    pub flag1: u8,
    pub flag2: u8,
    pub whole_match_distance: u32,
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

impl QuantumHeader {
    pub fn parse(&mut self, input: &[u8], use_checksum: bool) -> Result<usize, io::Error> {
        let mut v: u32 = u32::from_be_bytes([0, input[0], input[1], input[2]]);
        let size: usize = (v & 0x3FFFF) as usize;

        if size != 0x3FFFF {
            self.compressed_size = size as u32 + 1;
            self.flag1 = ((v >> 18) & 1) as u8;
            self.flag2 = ((v >> 19) & 1) as u8;

            if use_checksum {
                self.checksum = u32::from_be_bytes([0, input[3], input[4], input[5]]);
                return Ok(6);
            } else {
                return Ok(3);
            }
        }

        v >>= 18;

        if v == 1 {
            self.checksum = input[3] as u32;
            self.compressed_size = 0;
            self.whole_match_distance = 0;
            return Ok(4);
        }

        Err(io::Error::from(io::ErrorKind::InvalidData))
    }

    pub fn parse_lzna(
        &mut self,
        input: &[u8],
        use_checksum: bool,
        output_len: usize,
    ) -> Result<usize, io::Error> {
        let mut v: u32 = u32::from_be_bytes([0, 0, input[0], input[1]]);
        let size: usize = (v & 0x3FFF) as usize;

        if size != 0x3FFF {
            self.compressed_size = size as u32 + 1;
            self.flag1 = ((v >> 14) & 1) as u8;
            self.flag2 = ((v >> 15) & 1) as u8;
            if use_checksum {
                self.checksum = u32::from_be_bytes([0, input[2], input[3], input[4]]);
                return Ok(5);
            } else {
                return Ok(2);
            }
        }

        v >>= 14;

        if v == 0 {
            self.compressed_size = 0;
            v = u32::from_be_bytes([0, 0, input[2], input[3]]);

            if v < 0x8000 {
                let mut b: u8;
                let mut x: u32 = 0;
                let mut pos: usize = 0;
                let mut input_offset: usize = 2;
                loop {
                    b = input[input_offset];
                    input_offset += 1;

                    if b & 0x80 != 0 {
                        break;
                    }

                    x += (b as u32 + 0x80) << pos;
                    pos += 7;
                }
                x += (b as u32 - 128) << pos;
                self.whole_match_distance = 0x8000 + v + (x << 15) as u32 + 1;
                return Ok(input_offset);
            } else {
                self.whole_match_distance = v - 0x8000 + 1;
                return Ok(4);
            }
        }
        if v == 1 {
            self.checksum = input[2] as u32;
            self.compressed_size = 0;
            self.whole_match_distance = 0;
            return Ok(3);
        }
        if v == 2 {
            self.compressed_size = output_len as u32;
            return Ok(2);
        }

        Err(io::Error::from(io::ErrorKind::InvalidData))
    }
}

impl Default for QuantumHeader {
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
