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

impl ffi::OozleQuantumHeader {
    pub fn parse(&mut self, input: &[u8], use_checksum: bool) -> Result<usize, io::Error> {
        let mut v: u32 = u32::from_be_bytes([0, input[0], input[1], input[2]]);
        let size: usize = (v & 0xFFFF) as usize;

        if size != 0xFFFF {
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
