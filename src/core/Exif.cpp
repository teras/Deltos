#include "Exif.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
#ifdef DELTOS_HAVE_HEIF
#include <libheif/heif.h>
#endif

namespace deltos::Exif {

namespace {

struct Reader {
    const std::vector<uint8_t>& d;
    size_t base;   // TIFF header offset
    bool le;

    uint16_t u16(size_t off) const {
        if (off + 2 > d.size()) return 0;
        return le ? uint16_t(d[off] | d[off + 1] << 8) : uint16_t(d[off] << 8 | d[off + 1]);
    }
    uint32_t u32(size_t off) const {
        if (off + 4 > d.size()) return 0;
        return le ? uint32_t(d[off] | d[off + 1] << 8 | d[off + 2] << 16 | uint32_t(d[off + 3]) << 24)
                  : uint32_t(uint32_t(d[off]) << 24 | d[off + 1] << 16 | d[off + 2] << 8 | d[off + 3]);
    }

    // Walk one IFD, calling fn(tag, type, count, valueOffset) for each entry.
    template <class F> void ifd(uint32_t rel, F fn) const {
        size_t off = base + rel;
        uint16_t n = u16(off);
        off += 2;
        for (uint16_t i = 0; i < n && off + 12 <= d.size(); ++i, off += 12) {
            uint16_t tag = u16(off), type = u16(off + 2);
            uint32_t count = u32(off + 4);
            fn(tag, type, count, off + 8);
        }
    }
};

} // namespace

static std::optional<Info> parseTiff(const std::vector<uint8_t>& d, size_t tiff) {
    if (tiff + 8 > d.size()) return std::nullopt;
    Reader r{d, tiff, d[tiff] == 'I'};
    if (r.u16(tiff + 2) != 42) return std::nullopt;

    Info info;
    uint32_t exifIfd = 0;
    r.ifd(r.u32(tiff + 4), [&](uint16_t tag, uint16_t type, uint32_t, size_t v) {
        if (tag == 0x0112 && type == 3) info.orientation = r.u16(v);
        if (tag == 0x8769 && type == 4) exifIfd = r.u32(v);
    });
    if (exifIfd) {
        r.ifd(exifIfd, [&](uint16_t tag, uint16_t type, uint32_t count, size_t v) {
            if (tag == 0x920A && type == 5) {
                size_t off = tiff + r.u32(v);
                uint32_t num = r.u32(off), den = r.u32(off + 4);
                if (den) info.focalLengthMm = double(num) / den;
            }
            if (tag == 0xA405 && type == 3) info.focalLength35mm = r.u16(v);
            if (tag == 0x9206 && type == 5) {
                size_t off = tiff + r.u32(v);
                uint32_t num = r.u32(off), den = r.u32(off + 4);
                if (den && num && num != 0xFFFFFFFF) info.exifSubjectDistanceMm = 1000.0 * num / den;
            }
            if (tag == 0x927C && type == 7 && count > 14) {
                // Apple MakerNote: "Apple iOS\0" + u16 version + "MM"/"II" + IFD,
                // with value offsets relative to the MakerNote start.
                const size_t mn = tiff + r.u32(v);
                if (mn + 14 <= d.size() && std::memcmp(&d[mn], "Apple iOS", 9) == 0) {
                    Reader ar{d, mn, d[mn + 12] == 'I'};
                    ar.ifd(14, [&](uint16_t atag, uint16_t atype, uint32_t, size_t av) {
                        if (atag == 0x0038 && atype == 9) {
                            const int32_t cm = int32_t(ar.u32(av));
                            if (cm > 0 && cm < 1000) info.subjectDistanceMm = cm * 10.0;
                        }
                    });
                }
            }
        });
    }
    // Prefer a real depth measurement; the generic EXIF tag is a fallback.
    if (info.subjectDistanceMm <= 0 && info.exifSubjectDistanceMm > 0)
        info.subjectDistanceMm = info.exifSubjectDistanceMm;
    return info;
}

static std::optional<Info> readJpeg(const std::vector<uint8_t>& d) {
    if (d.size() < 4 || d[0] != 0xFF || d[1] != 0xD8) return std::nullopt;
    // Find APP1 segment with "Exif\0\0"
    size_t p = 2;
    while (p + 4 <= d.size() && d[p] == 0xFF) {
        uint8_t marker = d[p + 1];
        if (marker == 0xDA || marker == 0xD9) break; // SOS / EOI
        uint16_t len = uint16_t(d[p + 2] << 8 | d[p + 3]);
        if (marker == 0xE1 && p + 10 <= d.size() && std::memcmp(&d[p + 4], "Exif\0\0", 6) == 0)
            return parseTiff(d, p + 10);
        p += 2 + len;
    }
    return std::nullopt;
}

#ifdef DELTOS_HAVE_HEIF
static std::optional<Info> readHeif(const std::string& path) {
    heif_context* ctx = heif_context_alloc();
    std::optional<Info> result;
    if (heif_context_read_from_file(ctx, path.c_str(), nullptr).code == heif_error_Ok) {
        heif_image_handle* h = nullptr;
        if (heif_context_get_primary_image_handle(ctx, &h).code == heif_error_Ok) {
            heif_item_id id;
            if (heif_image_handle_get_list_of_metadata_block_IDs(h, "Exif", &id, 1) == 1) {
                std::vector<uint8_t> d(heif_image_handle_get_metadata_size(h, id));
                if (!d.empty() && heif_image_handle_get_metadata(h, id, d.data()).code == heif_error_Ok && d.size() > 4) {
                    // First 4 bytes (big endian) = offset of the TIFF header from byte 4.
                    const size_t off = 4 + (size_t(d[0]) << 24 | d[1] << 16 | d[2] << 8 | d[3]);
                    result = parseTiff(d, off);
                }
            }
            heif_image_handle_release(h);
        }
    }
    heif_context_free(ctx);
    return result;
}
#endif

std::optional<Info> read(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
    if (auto j = readJpeg(d)) return j;
#ifdef DELTOS_HAVE_HEIF
    if (d.size() > 12 && std::memcmp(&d[4], "ftyp", 4) == 0) return readHeif(path);
#endif
    return std::nullopt;
}

double focalPx(const std::optional<Info>& info, int width, int height) {
    double f35 = 28.0; // typical phone main camera
    if (info && info->focalLength35mm > 0) f35 = info->focalLength35mm;
    // The 35 mm equivalent is defined through the frame diagonal (36 x 24 mm -> 43.27 mm),
    // not the long side: for 4:3 or 16:9 frames the difference is 4-5 % of f.
    return f35 / std::hypot(36.0, 24.0) * std::hypot(double(width), double(height));
}

} // namespace deltos::Exif
