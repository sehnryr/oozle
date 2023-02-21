use crate::ffi;

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
    let len = ffi::Oozle_Decompress(input, output);

    if len < 0 {
        Err(())
    } else {
        Ok(len as usize)
    }
}
