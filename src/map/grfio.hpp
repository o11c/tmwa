#ifndef GRFIO_HPP
#define GRFIO_HPP

# include <cstddef>

/// Accessor to the .gat map virtual files
// Note .gat files are mapped to .wlk files by data/resnametable.txt
// Note that there currently is a 1-1 correlation between them,
// but it is possible for a single .wlk to have multiple .gats reference it

/// Load file into memory and possibly record length
// For some reason, this allocates an extra 1024 bytes at the end
void *grfio_reads(const char *resourcename, size_t *size);

/// Load file into memory
inline void *grfio_read(const char *resourcename)
{
    return grfio_reads(resourcename, NULL);
}

#endif // GRFIO_HPP
