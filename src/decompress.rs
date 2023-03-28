use anyhow::Result;

use crate::decoder::Decoder;

/// Decompresses a Kraken, Mermaid, Selkie, Leviathan, LZNA or Bitknit
/// compressed buffer.
///
/// # Arguments
///
/// * `input` - The compressed buffer.
/// * `output` - The decompressed buffer.
///
/// # Returns
///
/// * `Ok(len)` - The length of the decompressed buffer.
/// * `Err(anyhow::Error)` - The decompression failed.
pub unsafe fn decompress(input: &[u8], output: &mut [u8]) -> Result<usize> {
    let mut output_len: usize = output.len();

    let mut input_offset: usize = 0;
    let mut output_offset: usize = 0;

    let mut decoder: Decoder = Decoder::default();

    while output_len != 0 {
        decoder.decode_step(output, output_offset, &input[input_offset..])?;

        output_len -= decoder.output_written as usize;

        input_offset += decoder.input_read as usize;
        output_offset += decoder.output_written as usize;
    }
    Ok(output_offset)
}
