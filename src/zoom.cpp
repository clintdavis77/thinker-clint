
/*
 * Extra map zoom level when running with PRACX.
 *
 * Thinker does not own the map zoom-level table. When the GOG-bundled PRACX
 * patch is active (prax.dll), PRACX builds the list of zoom factors at runtime
 * in ZoomInit() and PRACXZoomKeyPress() snaps zoom changes to that list. The
 * number of levels comes from the [PRACX] ZoomLevels setting (six by default).
 *
 * To insert an additional level between levels 2 and 3 without redistributing
 * the existing six, Thinker extends PRACX's runtime table in place: the six
 * original factors are preserved and a seventh value, halfway between the
 * second and third, is inserted at index 2. PRACX's own zoom-key handler then
 * cycles through all seven levels, so the mouse wheel and the native zoom keys
 * both get the new level without any changes to the zoom input code.
 *
 * All offsets below are RVAs of prax.dll 1.11 (SMACX build). Every write is
 * guarded by a byte-pattern check so that a different PRACX version is left
 * untouched rather than corrupted.
 */

#include "main.h"

namespace {

/* PRACX runtime state (image base of prax.dll is 0x10000000). */
const uintptr_t PRAX_ZOOM_FACTORS   = 0x0048060; // int*  m_iZoomFactors
const uintptr_t PRAX_ZERO_INDEX     = 0x0048064; // int   m_iZoomZeroIndex
const uintptr_t PRAX_FACTOR_COUNT   = 0x0048068; // int   m_iZoomFactorCount
const uintptr_t PRAX_ZOOM_LEVELS    = 0x0048D94; // int   m_ST.m_iZoomLevels
const uintptr_t PRAX_LOOP_CMP       = 0x00094EC; // cmp ecx,[m_iZoomLevels] in PRACXZoomKeyPress
const uintptr_t PRAX_ZOOM_INIT      = 0x0000A230; // void ZoomInit(void)
const uintptr_t PRAX_NEW_ARRAY      = 0x0016BE9;  // void* operator new[](unsigned int)
const uintptr_t PRAX_DELETE_ARRAY   = 0x0016BF2;  // void  operator delete[](void*)

typedef void (__cdecl *FZoomInit)();
typedef void* (__cdecl *FNewArray)(unsigned int);
typedef void (__cdecl *FDeleteArray)(void*);

HMODULE prax_module = NULL;
bool prax_patched = false;
int* extended_factors = NULL; /* our seven-element array, if installed */

template<typename T>
T* prax_ptr(uintptr_t rva) {
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(prax_module) + rva);
}

/* Verify the PRACX build before touching any of its code or data. */
bool prax_signature_ok() {
    /* cmp ecx, [img_base + 0x8d94] : 3B 0D 94 8D 04 10 */
    const uint8_t* cmp = reinterpret_cast<const uint8_t*>(
        reinterpret_cast<uintptr_t>(prax_module) + PRAX_LOOP_CMP);
    uint32_t expected =
        reinterpret_cast<uintptr_t>(prax_module) + (PRAX_ZOOM_LEVELS - 0x10000000);
    return cmp[0] == 0x3B && cmp[1] == 0x0D
        && *reinterpret_cast<const uint32_t*>(cmp + 2) == expected;
}

/* Locate prax.dll and redirect PRACXZoomKeyPress's nearest-level loop to use
   m_iZoomFactorCount instead of the configured level count. */
void patch_prax_zoom_loop() {
    if (prax_patched || !prax_module) {
        return;
    }
    if (!prax_signature_ok()) {
        debug("Thinker: unsupported PRACX build, zoom extension disabled\n");
        prax_module = NULL;
        return;
    }

    uint8_t* imm = reinterpret_cast<uint8_t*>(
        reinterpret_cast<uintptr_t>(prax_module) + PRAX_LOOP_CMP + 2);
    uint32_t replacement =
        reinterpret_cast<uintptr_t>(prax_module) + (PRAX_FACTOR_COUNT - 0x10000000);

    DWORD old_protect;
    if (!VirtualProtect(imm, 4, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return;
    }
    *reinterpret_cast<uint32_t*>(imm) = replacement;
    DWORD ignored;
    VirtualProtect(imm, 4, old_protect, &ignored);
    prax_patched = true;
    debug("Thinker: PRACX zoom level loop patched\n");
}

/*
 * Insert a seventh zoom factor midway between PRACX levels 2 and 3 (array
 * indices 1 and 2). The original six factors are kept verbatim. Called after
 * PRACX (re)computes its table, including when a new map changes the range.
 */
void extend_pracx_zoom_table() {
    if (!prax_patched) {
        return;
    }
    int* factors = *prax_ptr<int*>(PRAX_ZOOM_FACTORS);
    int count = *prax_ptr<int>(PRAX_FACTOR_COUNT);
    if (!factors || count != 6) {
        /* Either not initialized yet, or already extended (count == 7). */
        return;
    }
    if (factors == extended_factors) {
        return;
    }

    /* Allocate with PRACX's own allocator; PRACX will delete[] this pointer
       the next time it recomputes the table for a different map size. */
    FNewArray op_new = reinterpret_cast<FNewArray>(
        reinterpret_cast<uintptr_t>(prax_module) + PRAX_NEW_ARRAY);
    int* extra = static_cast<int*>(op_new(sizeof(int) * 7));
    if (!extra) {
        return;
    }

    extra[0] = factors[0];
    extra[1] = factors[1];
    extra[2] = (factors[1] + factors[2]) / 2; /* between level 2 and level 3 */
    extra[3] = factors[2];
    extra[4] = factors[3];
    extra[5] = factors[4];
    extra[6] = factors[5];

    int zero_index = *prax_ptr<int>(PRAX_ZERO_INDEX);
    if (zero_index >= 2 && zero_index < 6) {
        ++zero_index; /* indices at/after the insertion point shift by one */
    }

    FDeleteArray op_delete = reinterpret_cast<FDeleteArray>(
        reinterpret_cast<uintptr_t>(prax_module) + PRAX_DELETE_ARRAY);
    op_delete(factors);

    *prax_ptr<int*>(PRAX_ZOOM_FACTORS) = extra;
    *prax_ptr<int>(PRAX_FACTOR_COUNT) = 7;
    *prax_ptr<int>(PRAX_ZERO_INDEX) = zero_index;
    extended_factors = extra;

    debug("Thinker: PRACX zoom factors: %d %d %d %d %d %d %d\n",
        extra[0], extra[1], extra[2], extra[3], extra[4], extra[5], extra[6]);
}

} // namespace

void extend_pracx_zoom() {
    if (!prax_module) {
        prax_module = GetModuleHandleA("prax.dll");
        if (!prax_module) {
            return;
        }
    }
    patch_prax_zoom_loop();
    if (!prax_patched) {
        return;
    }

    /* Force PRACX to build its table on the first message, if it has not
       already done so, so the very first zoom input sees all seven levels. */
    int configured_levels = *prax_ptr<int>(PRAX_ZOOM_LEVELS);
    if (!*prax_ptr<int*>(PRAX_ZOOM_FACTORS) && configured_levels >= 2 && configured_levels <= 20) {
        FZoomInit zoom_init = reinterpret_cast<FZoomInit>(
            reinterpret_cast<uintptr_t>(prax_module) + PRAX_ZOOM_INIT);
        zoom_init();
    }
    extend_pracx_zoom_table();
}
