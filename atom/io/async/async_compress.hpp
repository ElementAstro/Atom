#ifndef ATOM_IO_ASYNC_ASYNC_COMPRESS_HPP
#define ATOM_IO_ASYNC_ASYNC_COMPRESS_HPP

/**
 * @file async_compress.hpp
 * @brief Aggregate header for all asynchronous compression components.
 *
 * This header provides backward compatibility by including all split
 * async compression components. For more targeted includes, use the
 * individual headers directly:
 *   - async_compressor.hpp   : BaseCompressor, SingleFileCompressor,
 *                              DirectoryCompressor
 *   - async_decompressor.hpp : BaseDecompressor, SingleFileDecompressor,
 *                              DirectoryDecompressor
 *   - async_zip.hpp          : ZipOperation, ListFilesInZip,
 *                              FileExistsInZip, RemoveFileFromZip,
 *                              GetZipFileSize
 */

#include "async_compressor.hpp"
#include "async_decompressor.hpp"
#include "async_zip.hpp"

#endif  // ATOM_IO_ASYNC_ASYNC_COMPRESS_HPP
