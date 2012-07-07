/*
 * pict.h - convert an image to PICT.
 *
 * Currently creates a bitmap PICT resource; vector graphics are not preserved.
 *
 * By Charles Srstka.
 *
 * Public Domain. Do with it as you wish.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ConvertRGBAToPICT
 *
 * Converts image data, in 32-bit RGBA format, to PICT.
 * Calling it first with NULL for the buffer will cause it to return a suggested buffer size.
 * Returns the number of bytes actually written, or negative if something went wrong.
 * However, this usually just means the buffer wasn't large enough.
 */

ssize_t ConvertRGBAToPICT(uint8_t *buf, unsigned long bufSize, uint8_t *rgbaPixels, uint16_t width, uint16_t height);

#ifdef __cplusplus
}
#endif
