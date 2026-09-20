/*
 * Ratchet & Clank: Size Matters - Remastered Controls
 * Maintainer / Mantenedor: Rafitalocotron
 *
 * EN: The source implementation was generated with AI (ChatGPT / OpenAI)
 *     through an iterative reverse-engineering and real-hardware testing process.
 *     Rafitalocotron directed the project, performed the tests and supplied the
 *     reverse-engineering evidence used to validate each iteration.
 *
 * ES: La implementación del código fuente fue generada con IA (ChatGPT / OpenAI)
 *     mediante un proceso iterativo de ingeniería inversa y pruebas en hardware real.
 *     Rafitalocotron dirigió el proyecto, realizó las pruebas y aportó las evidencias
 *     de ingeniería inversa usadas para validar cada iteración.
 */

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <systemctrl.h>
#include <string.h>

PSP_MODULE_INFO("RatchetRemastered", 0x1007, 1, 0);
PSP_HEAP_SIZE_KB(0);

#define DEBUG_PATH "ms0:/seplugins/ratchet_debug.txt"

#define X_STORE_OFFSET 0xCCu
#define Y_STORE_OFFSET 0x114u
#define SIG_0 0x27BDFFD0u
#define SIG_1 0xE7B40010u
#define SIG_2 0xAFB00014u
#define SIG_3 0xAFB10018u
#define SIG_4 0xAFB2001Cu
#define SIG_5 0xAFBF0020u
#define X_STORE_SIG 0xE60C0274u
#define Y_STORE_SIG 0xE60C0278u

/* Same amplitudes proven during the debugger/direct-writer work. */
#define X_FULL_TARGET_RAD 0.055850543f /* 3.2 deg */
#define Y_FULL_TARGET_RAD 0.006981317f /* 0.4 deg */
#define STICK_DEADZONE    0.15f

/*
 * EN: Shared mailbox layout used to communicate from the Vita-side monitor
 *     to the pure user-space MIPS shim without calling plugin/kernel C code.
 * ES: Buzon compartido usado para comunicar el monitor del lado Vita con el
 *     shim MIPS puro en espacio de usuario sin llamar a código C/kernel del plugin.
 */
#define MAILBOX_FLAGS_OFF 0x00u
#define MAILBOX_X_OFF     0x04u
#define MAILBOX_Y_OFF     0x08u
#define MAILBOX_RAW_OFF      0x0Cu
#define MAILBOX_TARGET_X_OFF 0x10u
#define MAILBOX_TARGET_Y_OFF 0x14u
#define MAILBOX_CAMERA_FUNC_OFF 0x18u

static volatile u32 cameraFunc = 0;
static volatile u32 cameraStruct = 0;
static volatile u32 cameraXTarget = 0;
static volatile u32 cameraYTarget = 0;
static volatile u32 callsite = 0;
static volatile u32 shimCallsAddr = 0;
static volatile u32 returnSlotAddr = 0;
static volatile u32 sharedBaseAddr = 0;
static volatile u32 sharedFlagsAddr = 0;
static volatile u32 sharedXAddr = 0;
static volatile u32 sharedYAddr = 0;
static volatile u32 sharedRawAddr = 0;
static volatile u32 sharedTargetXAddr = 0;
static volatile u32 sharedTargetYAddr = 0;
static volatile u32 sharedCameraFuncAddr = 0;
static volatile u32 moduleTextAddr = 0;
static volatile u32 moduleTextSize = 0;
static volatile u32 shimAddr = 0;
static volatile u32 expectedOriginalCall = 0;
static volatile u32 expectedPatchedCall = 0;

static inline u32 read32(u32 a) { return *(volatile u32 *)a; }
static inline void write32(u32 a, u32 v) { *(volatile u32 *)a = v; }

static int slen(const char *s) {
    int n = 0;
    while (s && s[n]) ++n;
    return n;
}

static void log_raw(const char *s, int n) {
    SceUID fd = sceIoOpen(DEBUG_PATH,
        PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, s, n);
        sceIoClose(fd);
    }
}

static void log_s(const char *s) { log_raw(s, slen(s)); }

static void log_reset(void) {
    SceUID fd = sceIoOpen(DEBUG_PATH,
        PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) sceIoClose(fd);
}

static void log_hex(const char *label, u32 v) {
    static const char h[] = "0123456789ABCDEF";
    char b[96];
    int p = 0;
    for (int i = 0; label[i] && p < 78; ++i) b[p++] = label[i];
    b[p++] = '0'; b[p++] = 'x';
    for (int sh = 28; sh >= 0; sh -= 4) b[p++] = h[(v >> sh) & 0xF];
    b[p++] = '\n';
    log_raw(b, p);
}

static u32 makeJal(u32 target) {
    return 0x0C000000u | (((target & 0x0FFFFFFCu) >> 2) & 0x03FFFFFFu);
}

static u32 floatBits(float v) {
    union { float f; u32 u; } x;
    x.f = v;
    return x.u;
}

/*
 * EN: Right stick input is converted to a signed analog value with a deadzone
 *     and a quadratic response curve. This keeps small accidental movements quiet.
 * ES: Stick derecho convertido a un valor analógico con signo, zona muerta y
 *     curva cuadrática para evitar movimientos accidentales cerca del centro.
 */
static float gameAxis(u8 raw) {
    float x = 1.0f - 2.0f * ((float)raw / 255.0f);
    float a = x < 0.0f ? -x : x;

    if (a < STICK_DEADZONE)
        return 0.0f;

    float t = (a - STICK_DEADZONE) / (1.0f - STICK_DEADZONE);
    float y = t * t;
    return x < 0.0f ? -y : y;
}

static u32 allocUserBlock(const char *name, u32 bytes) {
    SceUID block = sceKernelAllocPartitionMemory(
        PSP_MEMORY_PARTITION_USER, name, PSP_SMEM_High, bytes, NULL);
    if (block < 0) return 0;
    u32 p = (u32)sceKernelGetBlockHeadAddr(block);
    if (!p) return 0;
    for (u32 off = 0; off < bytes; off += 4)
        write32(p + off, 0);
    sceKernelDcacheWritebackAll();
    return p;
}

/*
 * EN: Pure MIPS shim. The game executes only user-memory MIPS code here;
 *     it never jumps into plugin/kernel C, because that boundary crashed Adrenaline
 *     during earlier experiments on real Vita hardware.
 * ES: Shim MIPS puro. El juego ejecuta aquí únicamente código MIPS en memoria de
 *     usuario; nunca salta a C/kernel del plugin, ya que ese límite provocaba
 *     cuelgues de Adrenaline en pruebas anteriores sobre una Vita real.
 *
 * v0.13 DYNAMIC-ROUTE SHARED-MEMORY / PURE-MIPS SHIM
 *
 * v0.11's crash-free boundary is still preserved: game code executes only
 * user-memory MIPS. It never calls plugin C and never crosses into a syscall
 * helper.
 *
 * v0.13 removes the last level-specific code address from the shim. The
 * current Camera_Update function pointer lives in the mailbox and is called
 * with jalr. X/Y target pointers are mailbox data too. The kernel monitor can
 * therefore switch Camera_Update + callsite + camera structure together when
 * a different level package is loaded.
 */
static u32 makeSharedMemoryShim(
    u32 counter,
    u32 returnSlot,
    u32 flagsAddr,
    u32 xAddr,
    u32 yAddr,
    u32 xTargetSlot,
    u32 yTargetSlot,
    u32 cameraFuncSlot) {

    const u32 words = 41;
    SceUID block = sceKernelAllocPartitionMemory(
        PSP_MEMORY_PARTITION_USER,
        "RatchetRCSharedShim13",
        PSP_SMEM_High,
        words * sizeof(u32),
        NULL);
    if (block < 0) return 0;

    u32 s = (u32)sceKernelGetBlockHeadAddr(block);
    if (!s) return 0;

    u32 rhi = (returnSlot >> 16) & 0xFFFFu;
    u32 rlo = returnSlot & 0xFFFFu;
    u32 chi = (counter >> 16) & 0xFFFFu;
    u32 clo = counter & 0xFFFFu;
    u32 fhi = (flagsAddr >> 16) & 0xFFFFu;
    u32 flo = flagsAddr & 0xFFFFu;
    u32 xhi = (xAddr >> 16) & 0xFFFFu;
    u32 xlo = xAddr & 0xFFFFu;
    u32 yhi = (yAddr >> 16) & 0xFFFFu;
    u32 ylo = yAddr & 0xFFFFu;
    u32 txhi = (xTargetSlot >> 16) & 0xFFFFu;
    u32 txlo = xTargetSlot & 0xFFFFu;
    u32 tyhi = (yTargetSlot >> 16) & 0xFFFFu;
    u32 tylo = yTargetSlot & 0xFFFFu;
    u32 cfhi = (cameraFuncSlot >> 16) & 0xFFFFu;
    u32 cflo = cameraFuncSlot & 0xFFFFu;

    /* Preserve the game's incoming SP exactly, as in v0.9.1/v0.11. */
    write32(s + 0x00, 0x3C010000u | rhi);      /* lui   at,hi(returnSlot) */
    write32(s + 0x04, 0x34210000u | rlo);      /* ori   at,at,lo(returnSlot) */
    write32(s + 0x08, 0xAC3F0000u);            /* sw    ra,0(at) */

    /*
     * EN: Dynamic Camera_Update. Its address can change with the loaded level,
     *     so the shim reads the current function pointer from the mailbox and
     *     calls it indirectly with jalr.
     * ES: Camera_Update dinamico. Su dirección puede cambiar según el nivel,
     *     por eso el shim lee del buzón el puntero actual y lo llama con jalr.
     */
    write32(s + 0x0C, 0x3C080000u | cfhi);     /* lui   t0,hi(cameraFunc slot) */
    write32(s + 0x10, 0x35080000u | cflo);     /* ori   t0,t0,lo(cameraFunc slot) */
    write32(s + 0x14, 0x8D190000u);            /* lw    t9,0(t0) */
    write32(s + 0x18, 0x0320F809u);            /* jalr  t9 */
    write32(s + 0x1C, 0x00000000u);            /* nop */

    write32(s + 0x20, 0x3C010000u | rhi);      /* lui   at,hi(returnSlot) */
    write32(s + 0x24, 0x34210000u | rlo);      /* ori   at,at,lo(returnSlot) */
    write32(s + 0x28, 0x8C3F0000u);            /* lw    ra,0(at) */

    /* Diagnostic counter, pure user-mode MIPS. */
    write32(s + 0x2C, 0x3C080000u | chi);      /* lui   t0,hi(counter) */
    write32(s + 0x30, 0x35080000u | clo);      /* ori   t0,t0,lo(counter) */
    write32(s + 0x34, 0x8D090000u);            /* lw    t1,0(t0) */
    write32(s + 0x38, 0x25290001u);            /* addiu t1,t1,1 */
    write32(s + 0x3C, 0xAD090000u);            /* sw    t1,0(t0) */

    /* Load mailbox flags once. */
    write32(s + 0x40, 0x3C080000u | fhi);      /* lui   t0,hi(flags) */
    write32(s + 0x44, 0x35080000u | flo);      /* ori   t0,t0,lo(flags) */
    write32(s + 0x48, 0x8D090000u);            /* lw    t1,0(t0) */

    /* X: target pointer is loaded from the dynamic mailbox slot. */
    write32(s + 0x4C, 0x312A0001u);            /* andi  t2,t1,1 */
    write32(s + 0x50, 0x11400008u);            /* beq   t2,zero,+8 -> Y */
    write32(s + 0x54, 0x00000000u);            /* nop */
    write32(s + 0x58, 0x3C080000u | xhi);      /* lui   t0,hi(sharedX) */
    write32(s + 0x5C, 0x35080000u | xlo);      /* ori   t0,t0,lo(sharedX) */
    write32(s + 0x60, 0xC5020000u);            /* lwc1  f2,0(t0) */
    write32(s + 0x64, 0x3C080000u | txhi);     /* lui   t0,hi(targetX slot) */
    write32(s + 0x68, 0x35080000u | txlo);     /* ori   t0,t0,lo(targetX slot) */
    write32(s + 0x6C, 0x8D080000u);            /* lw    t0,0(t0) */
    write32(s + 0x70, 0xE5020000u);            /* swc1  f2,0(t0) */

    /* Y: target pointer is loaded from the dynamic mailbox slot. */
    write32(s + 0x74, 0x312A0002u);            /* andi  t2,t1,2 */
    write32(s + 0x78, 0x11400008u);            /* beq   t2,zero,+8 -> return */
    write32(s + 0x7C, 0x00000000u);            /* nop */
    write32(s + 0x80, 0x3C080000u | yhi);      /* lui   t0,hi(sharedY) */
    write32(s + 0x84, 0x35080000u | ylo);      /* ori   t0,t0,lo(sharedY) */
    write32(s + 0x88, 0xC5040000u);            /* lwc1  f4,0(t0) */
    write32(s + 0x8C, 0x3C080000u | tyhi);     /* lui   t0,hi(targetY slot) */
    write32(s + 0x90, 0x35080000u | tylo);     /* ori   t0,t0,lo(targetY slot) */
    write32(s + 0x94, 0x8D080000u);            /* lw    t0,0(t0) */
    write32(s + 0x98, 0xE5040000u);            /* swc1  f4,0(t0) */

    write32(s + 0x9C, 0x03E00008u);            /* jr    ra */
    write32(s + 0xA0, 0x00000000u);            /* nop */

    sceKernelDcacheWritebackAll();
    sceKernelIcacheClearAll();
    return s;
}

/*
 * EN: Camera signature scanner. We identify Camera_Update by stable Allegrex/MIPS
 *     instruction patterns instead of hard-coding one address for every level.
 * ES: Escáner de firma de cámara. Identificamos Camera_Update mediante patrones
 *     estables de instrucciones Allegrex/MIPS en vez de fijar una dirección por nivel.
 */
static int signatureMatches(u32 a, u32 end) {
    if (a + Y_STORE_OFFSET + 4 > end) return 0;
    return
        read32(a + 0x00) == SIG_0 &&
        read32(a + 0x04) == SIG_1 &&
        read32(a + 0x08) == SIG_2 &&
        read32(a + 0x0C) == SIG_3 &&
        read32(a + 0x10) == SIG_4 &&
        read32(a + 0x14) == SIG_5 &&
        read32(a + X_STORE_OFFSET) == X_STORE_SIG &&
        read32(a + Y_STORE_OFFSET) == Y_STORE_SIG;
}

static u32 findCameraFunction(u32 start, u32 size) {
    if (!start || size < Y_STORE_OFFSET + 4) return 0;
    u32 end = start + size;
    if (end < start) return 0;
    for (u32 a = start; a + Y_STORE_OFFSET + 4 <= end; a += 4)
        if (read32(a) == SIG_0 && signatureMatches(a, end)) return a;
    return 0;
}

/*
 * EN: Derive the active camera structure from the instructions inside the
 *     currently discovered Camera_Update routine.
 * ES: Deriva la estructura de cámara activa a partir de las instrucciones de la
 *     rutina Camera_Update descubierta actualmente.
 */
static u32 deriveCameraStruct(u32 func) {
    u32 hi = read32(func + 0x20);
    u32 lo = read32(func + 0x24);
    if ((hi & 0xFFFF0000u) != 0x3C100000u) return 0;
    u32 upper = (hi & 0xFFFFu) << 16;
    if ((lo & 0xFFFF0000u) == 0x36100000u)
        return upper | (lo & 0xFFFFu);
    if ((lo & 0xFFFF0000u) == 0x26100000u) {
        s32 imm = (s16)(lo & 0xFFFFu);
        return (u32)((s32)upper + imm);
    }
    return 0;
}

/*
 * EN: Find the direct JAL that reaches the current Camera_Update routine.
 *     The callsite can move when a different level package is loaded.
 * ES: Busca el JAL directo que llama a Camera_Update. El callsite puede cambiar
 *     cuando el juego carga el paquete de otro nivel.
 */
static u32 findDirectJalRef(u32 start, u32 size, u32 target) {
    u32 wanted = makeJal(target);
    u32 end = start + size;
    if (end < start) return 0;
    for (u32 a = start; a + 4 <= end; a += 4)
        if (read32(a) == wanted) return a;
    return 0;
}

static int refreshRcp1Module(void) {
    SceUID mods[96];
    int count = 0;
    if (sceKernelGetModuleIdList(mods, sizeof(mods), &count) < 0)
        return 0;
    if (count > 96) count = 96;

    for (int i = 0; i < count; ++i) {
        SceKernelModuleInfo info;
        info.size = sizeof(info);
        if (sceKernelQueryModuleInfo(mods[i], &info) < 0) continue;
        if (strcmp(info.name, "rcp1") != 0) continue;

        if (moduleTextAddr != info.text_addr || moduleTextSize != info.text_size) {
            moduleTextAddr = info.text_addr;
            moduleTextSize = info.text_size;
            log_s("rcp1 text region updated\n");
            log_hex("text addr=", moduleTextAddr);
            log_hex("text size=", moduleTextSize);
        }
        return moduleTextAddr != 0 && moduleTextSize != 0;
    }
    return 0;
}

static int validUserAddress(u32 a) {
    return a >= 0x08800000u && a < 0x0C000000u;
}

/*
 * EN: Level/route rediscovery. If a level moves Camera_Update, its caller or the
 *     camera structure, this path discovers all three again at runtime.
 * ES: Redescubrimiento de ruta/nivel. Si un nivel mueve Camera_Update, su caller o
 *     la estructura de cámara, esta ruta vuelve a localizar los tres en ejecución.
 *
 * Full rediscovery used when a level package moves Camera_Update/callsite.
 * The strict signature already distinguished one camera function in all three
 * captured planets. The direct JAL to that function identifies its live caller.
 */
static int rescanCameraRoute(void) {
    if (!shimAddr || !sharedFlagsAddr || !sharedCameraFuncAddr ||
        !sharedTargetXAddr || !sharedTargetYAddr)
        return 0;

    write32(sharedFlagsAddr, 0);

    if (!refreshRcp1Module())
        return 0;

    u32 func = findCameraFunction(moduleTextAddr, moduleTextSize);
    if (!func)
        return 0;

    u32 st = deriveCameraStruct(func);
    if (!validUserAddress(st))
        return 0;

    u32 originalCall = makeJal(func);
    u32 patchedCall = makeJal(shimAddr);
    u32 cs = findDirectJalRef(moduleTextAddr, moduleTextSize, func);

    /* If this exact route is already patched, the original JAL is naturally
     * absent from text. Accept only our already-known callsite as the fallback;
     * never select an arbitrary occurrence of the shim JAL. */
    if (!cs && func == cameraFunc && callsite &&
        read32(callsite) == patchedCall) {
        cs = callsite;
    }
    if (!cs)
        return 0;

    u32 cur = read32(cs);
    /*
     * EN: Safe repatching: only touch the callsite when it contains either the
     *     exact original JAL or the JAL to our own shim. Unknown code is left alone.
     * ES: Reaplicacion segura: solo se modifica el callsite si contiene el JAL
     *     original exacto o el JAL hacia nuestro shim. Código desconocido no se toca.
     */
    if (cur != originalCall && cur != patchedCall)
        return 0;

    u32 oldFunc = cameraFunc;
    u32 oldCallsite = callsite;
    u32 oldStruct = cameraStruct;

    /* Publish all user-memory data before making the shim reachable from a new
     * callsite. Flags remain zero until the input loop samples a fresh stick. */
    write32(sharedCameraFuncAddr, func);
    write32(sharedTargetXAddr, st + 0x274u);
    write32(sharedTargetYAddr, st + 0x278u);
    sceKernelDcacheWritebackAll();

    cameraFunc = func;
    cameraStruct = st;
    cameraXTarget = st + 0x274u;
    cameraYTarget = st + 0x278u;
    callsite = cs;
    expectedOriginalCall = originalCall;
    expectedPatchedCall = patchedCall;

    if (cur == originalCall) {
        write32(cs, expectedPatchedCall);
        /*
         * EN: Cache maintenance is required after modifying executable MIPS code.
         * ES: Cache: tras modificar código MIPS ejecutable hay que sincronizar las
         *     cachés de datos e instrucciones.
         */
        sceKernelDcacheWritebackAll();
        sceKernelIcacheClearAll();
    }

    if (oldFunc != cameraFunc || oldCallsite != callsite) {
        log_s("camera route changed\n");
        log_hex("camera func=", cameraFunc);
        log_hex("callsite=", callsite);
        log_hex("original call=", expectedOriginalCall);
        log_hex("patched call=", read32(callsite));
    }
    if (oldStruct != cameraStruct) {
        log_s("camera struct changed\n");
        log_hex("camera struct=", cameraStruct);
        log_hex("camera X=", cameraXTarget);
        log_hex("camera Y=", cameraYTarget);
    }

    return read32(callsite) == expectedPatchedCall;
}

/* Fast path for normal frames. Same-function level swaps (Pokitaru/Ryllus)
 * only change the camera structure. Code-relocating swaps (Kalidon) fall back
 * to rescanCameraRoute(), which discovers the new function and callsite. */
static int maintainCameraRoute(void) {
    if (!cameraFunc || !callsite || !moduleTextAddr || !moduleTextSize)
        return rescanCameraRoute();

    u32 end = moduleTextAddr + moduleTextSize;
    if (end < moduleTextAddr || cameraFunc < moduleTextAddr ||
        cameraFunc + Y_STORE_OFFSET + 4u > end ||
        !signatureMatches(cameraFunc, end)) {
        return rescanCameraRoute();
    }

    u32 st = deriveCameraStruct(cameraFunc);
    if (!validUserAddress(st))
        return rescanCameraRoute();

    if (st != cameraStruct) {
        write32(sharedFlagsAddr, 0);
        write32(sharedTargetXAddr, st + 0x274u);
        write32(sharedTargetYAddr, st + 0x278u);
        sceKernelDcacheWritebackAll();

        cameraStruct = st;
        cameraXTarget = st + 0x274u;
        cameraYTarget = st + 0x278u;
        log_s("camera struct changed\n");
        log_hex("camera struct=", cameraStruct);
        log_hex("camera X=", cameraXTarget);
        log_hex("camera Y=", cameraYTarget);
    }

    u32 cur = read32(callsite);
    if (cur == expectedPatchedCall)
        return 1;

    /* Same route was reloaded: only re-patch if the exact original JAL is back. */
    if (cur == expectedOriginalCall) {
        write32(callsite, expectedPatchedCall);
        sceKernelDcacheWritebackAll();
        sceKernelIcacheClearAll();
        log_s("current callsite restored after level reload\n");
        log_hex("patched call=", read32(callsite));
        return read32(callsite) == expectedPatchedCall;
    }

    /* Unknown instruction here means this route is no longer authoritative. */
    return rescanCameraRoute();
}

static int setupSharedRuntime(void) {
    if (shimAddr)
        return 1;

    u32 control = allocUserBlock("RatchetRCControl13", 8);
    sharedBaseAddr = allocUserBlock("RatchetRCMailbox13", 28);
    if (!control || !sharedBaseAddr) {
        log_s("ERROR: shared-memory allocation failed\n");
        return 0;
    }

    returnSlotAddr = control + 0;
    shimCallsAddr = control + 4;
    sharedFlagsAddr = sharedBaseAddr + MAILBOX_FLAGS_OFF;
    sharedXAddr = sharedBaseAddr + MAILBOX_X_OFF;
    sharedYAddr = sharedBaseAddr + MAILBOX_Y_OFF;
    sharedRawAddr = sharedBaseAddr + MAILBOX_RAW_OFF;
    sharedTargetXAddr = sharedBaseAddr + MAILBOX_TARGET_X_OFF;
    sharedTargetYAddr = sharedBaseAddr + MAILBOX_TARGET_Y_OFF;
    sharedCameraFuncAddr = sharedBaseAddr + MAILBOX_CAMERA_FUNC_OFF;

    u32 shim = makeSharedMemoryShim(
        shimCallsAddr,
        returnSlotAddr,
        sharedFlagsAddr,
        sharedXAddr,
        sharedYAddr,
        sharedTargetXAddr,
        sharedTargetYAddr,
        sharedCameraFuncAddr);
    if (!shim) {
        log_s("ERROR: shared MIPS shim allocation failed\n");
        return 0;
    }

    shimAddr = shim;
    expectedPatchedCall = makeJal(shimAddr);

    log_hex("shim counter=", shimCallsAddr);
    log_hex("return slot=", returnSlotAddr);
    log_hex("mailbox base=", sharedBaseAddr);
    log_hex("mailbox flags=", sharedFlagsAddr);
    log_hex("mailbox X=", sharedXAddr);
    log_hex("mailbox Y=", sharedYAddr);
    log_hex("mailbox raw=", sharedRawAddr);
    log_hex("mailbox target X slot=", sharedTargetXAddr);
    log_hex("mailbox target Y slot=", sharedTargetYAddr);
    log_hex("mailbox camera func slot=", sharedCameraFuncAddr);
    log_hex("shared user shim=", shimAddr);
    log_s("shim original call=DYNAMIC_JALR\n");
    log_s("shim helper JAL=NONE\n");
    return 1;
}

static int patchText(u32 textAddr, u32 textSize) {
    moduleTextAddr = textAddr;
    moduleTextSize = textSize;

    if (!setupSharedRuntime())
        return 0;

    if (!rescanCameraRoute())
        return 0;

    log_s("*** V0.13 DYNAMIC-ROUTE MIPS HOOK INSTALLED ***\n");
    return 1;
}

static int MonitorThread(SceSize args, void *argp) {
    (void)args; (void)argp;

    /* Single installer only. Initial discovery waits for rcp1, after which the
     * same monitor continuously tracks level-dependent camera routes. */
    while (!callsite) {
        if (refreshRcp1Module())
            patchText(moduleTextAddr, moduleTextSize);
        if (!callsite) sceKernelDelayThread(250000);
    }

    log_s("input mailbox active\n");
    log_s("dynamic Camera_Update/callsite/targets enabled\n");
    log_s("right stick is sampled in kernel thread; shim only reads user memory\n");

    u32 lastLoggedCalls = 0xFFFFFFFFu;
    u32 lastLoggedRaw = 0xFFFFFFFFu;
    u32 lastLoggedFlags = 0xFFFFFFFFu;
    u32 ticks = 0;

    while (1) {
        int cameraReady = maintainCameraRoute();

        SceCtrlData pad;
        int oldK1 = pspSdkSetK1(0);
        int got = sceCtrlPeekBufferPositive(&pad, 1);
        pspSdkSetK1(oldK1);

        if (got > 0 && cameraReady) {
            float rx = -gameAxis(pad.Rsrv[0]);
            float ry = gameAxis(pad.Rsrv[1]);
            u32 flags = 0;
            if (rx != 0.0f) flags |= 1u;
            if (ry != 0.0f) flags |= 2u;

            /* Values first, activation mask last. */
            write32(sharedXAddr, floatBits(rx * X_FULL_TARGET_RAD));
            write32(sharedYAddr, floatBits(ry * Y_FULL_TARGET_RAD));
            write32(sharedRawAddr, ((u32)pad.Rsrv[0] << 8) | (u32)pad.Rsrv[1]);
            write32(sharedFlagsAddr, flags);
        } else {
            write32(sharedFlagsAddr, 0);
        }

        /* Diagnostics for the first ~30 seconds only; no file IO in the shim. */
        /*
         * EN: Debug logging is intentionally limited to the startup window so the
         *     hot MIPS shim never performs file I/O and long sessions stay quiet.
         * ES: El log de depuración se limita al arranque para que el shim MIPS no
         *     haga E/S de archivos y las sesiones largas no generen ruido constante.
         */
        if ((ticks % 250u) == 0u && ticks < 30000u) {
            u32 calls = read32(shimCallsAddr);
            u32 raw = read32(sharedRawAddr);
            u32 flags = read32(sharedFlagsAddr);
            if (calls != lastLoggedCalls) {
                log_hex("shimCalls=", calls);
                lastLoggedCalls = calls;
            }
            if (raw != lastLoggedRaw) {
                log_hex("last RxRy=", raw);
                lastLoggedRaw = raw;
            }
            if (flags != lastLoggedFlags) {
                log_hex("activeFlags=", flags);
                lastLoggedFlags = flags;
            }
        }

        ticks++;
        sceKernelDelayThread(1000);
    }

    return sceKernelExitDeleteThread(0);
}

int module_start(SceSize args, void *argp) {
    (void)args; (void)argp;
    log_reset();
    log_s("=== RatchetRemastered v0.13 dynamic camera routes ===\n");
    log_s("single installer/monitor thread only\n");
    log_s("Camera_Update + callsite + camera targets are rediscovered per level\n");
    log_s("pure-MIPS user shim; dynamic jalr; NO shim-to-kernel-C call\n");

    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    SceUID thid = sceKernelCreateThread(
        "RatchetRCDynamic13", MonitorThread, 0x20, 0x3000, 0, NULL);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, NULL);
        log_s("monitor/input thread started\n");
    } else {
        log_hex("ERROR: monitor create=", (u32)thid);
    }
    return 0;
}

int module_stop(SceSize args, void *argp) {
    (void)args; (void)argp;
    return 0;
}
