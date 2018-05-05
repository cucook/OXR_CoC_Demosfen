#include "stdafx.h"

#include <Psapi.h>

xrMemory Memory;
// Also used in src\xrCore\xrDebug.cpp to prevent use of g_pStringContainer before it initialized
bool shared_str_initialized = false;

xrMemory::xrMemory()
{
}

void xrMemory::_initialize()
{
    stat_calls = 0;

    g_pStringContainer = new str_container();
    shared_str_initialized = true;
    g_pSharedMemoryContainer = new smem_container();
}

void xrMemory::_destroy()
{
    xr_delete(g_pSharedMemoryContainer);
    xr_delete(g_pStringContainer);
}

void xrMemory::GetProcessMemInfo(SProcessMemInfo& minfo)
{
    std::memset(&minfo, 0, sizeof(SProcessMemInfo));

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);

    minfo.TotalPhysicalMemory = mem.ullTotalPhys;
    minfo.FreePhysicalMemory = mem.ullAvailPhys;
    minfo.TotalVirtualMemory = mem.ullTotalVirtual;
    minfo.MemoryLoad = mem.dwMemoryLoad;

    //--------------------------------------------------------------------
    PROCESS_MEMORY_COUNTERS pc;
    std::memset(&pc, 0, sizeof(PROCESS_MEMORY_COUNTERS));
    pc.cb = sizeof(pc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pc, sizeof(pc)))
    {
        minfo.PeakWorkingSetSize = pc.PeakWorkingSetSize;
        minfo.WorkingSetSize = pc.WorkingSetSize;
        minfo.PagefileUsage = pc.PagefileUsage;
        minfo.PeakPagefileUsage = pc.PeakPagefileUsage;
    }
}

XRCORE_API void vminfo(size_t* _free, size_t* reserved, size_t* committed)
{
    MEMORY_BASIC_INFORMATION memory_info;
    memory_info.BaseAddress = 0;
    *_free = *reserved = *committed = 0;
    while (VirtualQuery(memory_info.BaseAddress, &memory_info, sizeof(memory_info)))
    {
        switch (memory_info.State)
        {
        case MEM_FREE: *_free += memory_info.RegionSize; break;
        case MEM_RESERVE: *reserved += memory_info.RegionSize; break;
        case MEM_COMMIT: *committed += memory_info.RegionSize; break;
        }
        memory_info.BaseAddress = (char*)memory_info.BaseAddress + memory_info.RegionSize;
    }
}

XRCORE_API void log_vminfo()
{
    size_t w_free, w_reserved, w_committed;
    vminfo(&w_free, &w_reserved, &w_committed);
#ifdef XR_X64
    Msg("* [win32]: free[%I64d MB], reserved[%u KB], committed[%u KB]", w_free / (1024 * 1024), w_reserved / 1024,
        w_committed / 1024);
#else
    Msg("* [win32]: free[%I64d K], reserved[%d K], committed[%d K]", w_free / 1024, w_reserved / 1024, w_committed / 1024);
#endif
}

size_t xrMemory::mem_usage()
{
    PROCESS_MEMORY_COUNTERS pmc = {};
    if (HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId()))
    {
        GetProcessMemoryInfo(h, &pmc, sizeof(pmc));
        CloseHandle(h);
    }
    return pmc.PagefileUsage;
}

void xrMemory::mem_compact()
{
    RegFlushKey(HKEY_CLASSES_ROOT);
    RegFlushKey(HKEY_CURRENT_USER);
    /*
    Следующие две команды в целом не нужны.
    Современные аллокаторы достаточно грамотно и когда нужно возвращают память операционной системе.
    Эта строчки нужны, скорее всего, в определённых ситуациях, вроде использования файлов отображаемых в память,
    которые требуют большие свободные области памяти.
    Но всё-же чистку tbb, возможно, стоит оставить. Но и это под большим вопросом.
    */
    scalable_allocation_command(TBBMALLOC_CLEAN_ALL_BUFFERS, NULL);
    //HeapCompact(GetProcessHeap(), 0);
    if (g_pStringContainer)
        g_pStringContainer->clean();
    if (g_pSharedMemoryContainer)
        g_pSharedMemoryContainer->clean();
    if (strstr(Core.Params, "-swap_on_compact"))
        SetProcessWorkingSetSize(GetCurrentProcess(), size_t(-1), size_t(-1));
}

// xr_strdup
pstr xr_strdup(pcstr string)
{
    VERIFY(string);
    size_t len = xr_strlen(string) + 1;
    char* memory = (char*)xr_malloc(len);
    CopyMemory(memory, string, len);
    return memory;
}
