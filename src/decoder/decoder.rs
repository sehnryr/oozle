use std::io;

use anyhow::{Error, Result};
use log::{debug, trace, warn};

use crate::ffi;

use super::common::{copy_whole_match, get_crc};
use super::decoder_type::DecoderType;
use super::header::Header;
use super::quantum_header::QuantumHeader;

pub struct Decoder {
    pub input_read: u32,
    pub output_written: u32,
    pub scratch: [u8; 0x6C000],
    pub header: Header,
    pub quantum_header: QuantumHeader,
}

impl Decoder {
    pub fn parse_header(&mut self, input: &[u8]) -> Result<usize, io::Error> {
        self.header.parse(input)
    }

    pub fn parse_quantum_header(&mut self, input: &[u8]) -> Result<usize, io::Error> {
        self.quantum_header.parse(input, self.header.use_checksums)
    }

    pub fn parse_lzna_quantum_header(
        &mut self,
        input: &[u8],
        output_len: usize,
    ) -> Result<usize, io::Error> {
        self.quantum_header
            .parse_lzna(input, self.header.use_checksums, output_len)
    }

    pub unsafe fn decode_step(
        &mut self,
        output: &mut [u8],
        output_offset: usize,
        input: &[u8],
    ) -> Result<()> {
        let mut input_offset: usize = 0;

        // If the output buffer is less than 0x40000, parse the header
        if output_offset < 0x40000 {
            trace!("Parsing header");
            input_offset += self.parse_header(input)?;
        }

        // Get type of decoder
        let is_kraken_decoder = self.header.decoder_type == DecoderType::Kraken
            || self.header.decoder_type == DecoderType::Mermaid
            || self.header.decoder_type == DecoderType::Leviathan;

        // Calculate the output length
        let output_len = std::cmp::min(
            output.len() - output_offset,
            if is_kraken_decoder { 0x40000 } else { 0x4000 },
        );

        // Check if the data is compressed
        trace!("Checking if data is compressed");
        if self.header.uncompressed {
            // Check if the input buffer is large enough and return if not
            if input.len() - input_offset < output_len {
                self.input_read = 0;
                self.output_written = 0;
                return Ok(());
            }

            // Copy the data to the output buffer
            output[output_offset..output_offset + output_len]
                .copy_from_slice(&input[input_offset..input_offset + output_len]);
            self.input_read = (input_offset + output_len) as u32;
            self.output_written = output_len as u32;
            return Ok(());
        }

        // If the decoder is a Kraken decoder, parse the quantum header
        if is_kraken_decoder {
            trace!("Parsing quantum header");
            input_offset += self.parse_quantum_header(&input[input_offset..])?;
        }
        // Otherwise, parse the LZNA quantum header
        else {
            trace!("Parsing LZNA quantum header");
            input_offset += self.parse_lzna_quantum_header(&input[input_offset..], output_len)?;
        }

        // Check if the input buffer has enough data to decompress and return if not
        trace!("Checking if input buffer has enough data to decompress");
        if input.len() - input_offset < self.quantum_header.compressed_size as usize {
            self.input_read = 0;
            self.output_written = 0;
            return Ok(());
        }

        // Check if the output buffer is large enough for the decompressed data
        // and return an error if not
        trace!("Checking if output buffer is large enough for decompressed data");
        if output_len < self.quantum_header.compressed_size as usize {
            return Err(Error::msg("Output buffer is too small"));
        }

        // If the compressed size is 0, the data should either be parsed as a whole match
        // or filled with the checksum
        trace!("Checking if compressed size is 0");
        if self.quantum_header.compressed_size == 0 {
            if self.quantum_header.whole_match_distance != 0 {
                // Check if the whole match distance is too large
                if self.quantum_header.whole_match_distance > output_offset as u32 {
                    return Err(Error::msg("Whole match distance is too large"));
                }

                copy_whole_match(
                    output,
                    output_offset,
                    self.quantum_header.whole_match_distance as usize,
                );
            } else {
                // TODO: Verify that this is the correct analogy to
                // memset(output + output_offset, checksum, output_len);
                output[output_offset..output_offset + output_len]
                    .fill(self.quantum_header.checksum as u8);
            }

            self.input_read = input_offset as u32;
            self.output_written = output_len as u32;
            return Ok(());
        }

        // Get the CRC checksum of the compressed data
        trace!("Getting CRC checksum of compressed data");
        let crc = get_crc(
            &input[input_offset..input_offset + self.quantum_header.compressed_size as usize],
        );

        // Check if the checksum is correct
        trace!("Checking if checksum is correct");
        if self.header.use_checksums && (crc & 0xFFFFFF) != self.quantum_header.checksum {
            return Err(Error::msg("Checksum mismatch"));
        }

        // If the compressed size is the same as the output length, the data is not compressed.
        // Copy the data to the output buffer and return
        trace!("Checking if compressed size is the same as output length");
        if self.quantum_header.compressed_size == output_len as u32 {
            output[output_offset..output_offset + output_len]
                .copy_from_slice(&input[input_offset..input_offset + output_len]);

            self.input_read = (input_offset + output_len) as u32;
            self.output_written = output_len as u32;
            return Ok(());
        }

        // Decompress the data using the correct decoder
        trace!("Decompressing data using correct decoder");
        debug!(
            "Decompressing {} bytes to {} bytes using {:?}",
            self.quantum_header.compressed_size, output_len, self.header.decoder_type
        );
        debug!(
            "Input offset: {} Output offset: {}",
            input_offset, output_offset
        );
        let read_bytes: i32 = match self.header.decoder_type {
            DecoderType::Kraken => ffi::Kraken_DecodeQuantum(
                output.as_mut_ptr().add(output_offset),
                output.as_mut_ptr().add(output_offset + output_len),
                output.as_mut_ptr(),
                input.as_ptr().add(input_offset),
                input
                    .as_ptr()
                    .add(input_offset + self.quantum_header.compressed_size as usize),
                self.scratch.as_mut_ptr(),
                self.scratch.as_mut_ptr().add(self.scratch.len()),
            ),
            DecoderType::Mermaid => ffi::Mermaid_DecodeQuantum(
                output.as_mut_ptr().add(output_offset),
                output.as_mut_ptr().add(output_offset + output_len),
                output.as_mut_ptr(),
                input.as_ptr().add(input_offset),
                input
                    .as_ptr()
                    .add(input_offset + self.quantum_header.compressed_size as usize),
                self.scratch.as_mut_ptr(),
                self.scratch.as_mut_ptr().add(self.scratch.len()),
            ),
            DecoderType::Leviathan => ffi::Leviathan_DecodeQuantum(
                output.as_mut_ptr().add(output_offset),
                output.as_mut_ptr().add(output_offset + output_len),
                output.as_mut_ptr(),
                input.as_ptr().add(input_offset),
                input
                    .as_ptr()
                    .add(input_offset + self.quantum_header.compressed_size as usize),
                self.scratch.as_mut_ptr(),
                self.scratch.as_mut_ptr().add(self.scratch.len()),
            ),
            DecoderType::LZNA => {
                if self.header.restart_decoder {
                    self.header.restart_decoder = false;
                    ffi::LZNA_InitLookup(self.scratch.as_mut_ptr() as *mut ffi::LznaState);
                }
                ffi::LZNA_DecodeQuantum(
                    output.as_mut_ptr().add(output_offset),
                    output.as_mut_ptr().add(output_offset + output_len),
                    output.as_mut_ptr(),
                    input.as_ptr().add(input_offset),
                    input
                        .as_ptr()
                        .add(input_offset + self.quantum_header.compressed_size as usize),
                    self.scratch.as_mut_ptr() as *mut ffi::LznaState,
                )
            }
            DecoderType::Bitknit => {
                if self.header.restart_decoder {
                    self.header.restart_decoder = false;
                    ffi::BitknitState_Init(self.scratch.as_mut_ptr() as *mut ffi::BitknitState);
                }
                ffi::Bitknit_Decode(
                    input.as_ptr().add(input_offset),
                    input
                        .as_ptr()
                        .add(input_offset + self.quantum_header.compressed_size as usize),
                    output.as_mut_ptr().add(output_offset),
                    output.as_mut_ptr().add(output_offset + output_len),
                    output.as_mut_ptr(),
                    self.scratch.as_mut_ptr() as *mut ffi::BitknitState,
                ) as i32
            }
            _ => return Err(Error::msg("Unknown decoder type")),
        };

        // Check if the decompressed size is correct
        trace!("Checking if decompressed size is correct");
        if read_bytes != self.quantum_header.compressed_size as i32 {
            let message = format!(
                "Decompressed size mismatch: {} != {}",
                read_bytes, self.quantum_header.compressed_size
            );
            warn!("{}", message);
            return Err(Error::msg(message));
        }

        self.input_read = input_offset as u32 + read_bytes as u32;
        self.output_written = output_len as u32;

        Ok(())
    }
}

impl Default for Decoder {
    fn default() -> Self {
        Self {
            input_read: 0,
            output_written: 0,
            scratch: [0; 0x6C000],
            header: Header::default(),
            quantum_header: QuantumHeader::default(),
        }
    }
}
