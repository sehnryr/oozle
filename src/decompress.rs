use crate::ffi;

/// Decompresses a Kraken, Mermaid, Selkie, Leviathan, LZNA or Bitknit
/// compressed buffer.
/// 
/// # Arguments
/// 
/// * `src` - The compressed buffer.
/// * `dst` - The decompressed buffer.
/// 
/// # Returns
/// 
/// * `Ok(len)` - The length of the decompressed buffer.
/// * `Err(())` - The decompression failed.
pub unsafe fn decompress(src: &[u8], dst: &mut [u8]) -> Result<usize, ()> {
    let len = ffi::Kraken_Decompress(
        src.as_ptr(), 
        src.len(), 
        dst.as_mut_ptr(), 
        dst.len(),
    );

    if len < 0 {
        Err(())
    } else {
        Ok(len as usize)
    }
}
