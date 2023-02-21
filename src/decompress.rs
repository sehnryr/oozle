use crate::ffi::{OozleDecoder, Oozle_DecodeStep};

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
/// * `Err(())` - The decompression failed.
pub unsafe fn decompress(input: &[u8], output: &mut [u8]) -> Result<usize, ()> {
    let mut input_len: usize = input.len();
    let mut output_len: usize = output.len();

    let mut input_offset: usize = 0;
    let mut output_offset: usize = 0;

    let mut decoder: OozleDecoder = OozleDecoder::default();

    while output_len != 0 {
        if !Oozle_DecodeStep(
            &mut decoder,
            output.as_mut_ptr(),
            output_offset as i32,
            output_len,
            input.as_ptr().add(input_offset),
            input_len,
        ) {
            return Err(());
        }

        input_len -= decoder.input_read as usize;
        output_len -= decoder.output_written as usize;

        input_offset += decoder.input_read as usize;
        output_offset += decoder.output_written as usize;
    }
    Ok(output_offset)
}
