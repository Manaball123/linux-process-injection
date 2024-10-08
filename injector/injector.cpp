#include "injector.h"
#include "../shared/utils.h"

using namespace Injector;




bool CInjector::ReadBufferFromProcess(Buffer& dst_buf, void* src_addr) {
    if(mem_fd == -1)
        return false;
    lseek64(mem_fd, (uint64)src_addr, SEEK_SET);
    uint64 r = read(mem_fd, dst_buf.p_buf, dst_buf.buf_size);
    std::cout << "read " << r << " bytes from " << src_addr << "\n";
    return true;
}
bool CInjector::InjectShellcodeMap(Buffer& buf, uint64 parameter) {
    std::string fname = std::to_string(pid) + "-temp_memory_file.bin";
    buf.DumpToFile(fname);
    bool res = InjectShellcodeMap(fname);
    //TODO: delete file here
    return res;
}
bool CInjector::InjectShellcodeMap(std::string filename, uint64 parameter) {
    return true;
}

bool CInjector::InjectShellcodeWriteAnonymous(Buffer& buf, uint64 parameter) {
    byte* dst_buf = (byte*)AllocateAnonymous(buf.buf_size);
    WriteBufferToProcess(buf, dst_buf);
    StartThreadAtAddress(dst_buf, 0);
    return true;
}
bool CInjector::InjectShellcodeWriteAnonymous(std::string filename, uint64 parameter) {
    Buffer buf(filename);
    return InjectShellcodeWriteAnonymous(buf);
}


bool CInjector::InjectSharedLibrary_manualmap(std::string filename) {
    uint64 entry_offset = dlsymFile(filename, "init");
    //uses anonymoos map cuz SWAG
    Buffer module_buf(filename);
    //rwx by default
    byte* module_dst_buf = (byte*)AllocateAnonymous(module_buf.buf_size, PROT_READ | PROT_EXEC);
    WriteBufferToProcess(module_buf, module_dst_buf);

    StartThreadAtAddress(module_dst_buf + entry_offset);

    return true;
}



void _entrypoint_prototype();
using dlopen_t = decltype(dlopen);
using entrypoint_t = decltype(_entrypoint_prototype);
struct DLOPEN_DATA {
    dlopen_t* p_dlopen = 0;
    char fname[512];
    DLOPEN_DATA(){
        memset(fname, 0, 512);
    }
};

void DlOpenShellcode(DLOPEN_DATA* p_dlopen_data) {

    void* result = (p_dlopen_data->p_dlopen)((const char*)(&(p_dlopen_data->fname)), RTLD_NOW);
    inline_exit(0);
    inline_int3();
}

void EntryPointWrapperShellcode(entrypoint_t* entrypoint) {
    entrypoint();
    inline_exit(0);
    inline_int3();
}
bool CInjector::RunEntryPoint(std::string module_fname, std::string proc_name) {
    module_fname = get_abs_path(module_fname);
    byte* proc = dlsymEx(module_fname, proc_name);
    std::cout << proc_name + " found @ " << (void*)proc<< "\n";
    if(proc == nullptr)
        throw std::runtime_error("Unable to find proc address, possibly because module is not injected.");

    Buffer shellcode_buf((byte*)&EntryPointWrapperShellcode, 0x1000);
    void* p_shellcode = AllocateAnonymousAndWrite(shellcode_buf);
    StartThreadAtAddress(p_shellcode, (uint64)proc);
    return true;
}
bool CInjector::InjectSharedLibrary_dlopen(std::string filename) {
    
    DLOPEN_DATA dlopen_dat;
    dlopen_dat.p_dlopen = (dlopen_t*)dlsymEx("/usr/lib/libc.so.6", "dlopen");
    std::cout << "dlopen found @ " << (void*)dlopen_dat.p_dlopen << "\n";

    filename = get_abs_path(filename);

    //filename = "/usr/lib/libc.so.6";
    assert(filename.size() <= 512);
    memcpy(&dlopen_dat.fname, filename.c_str(), filename.size());
    
    void* p_dlopen_dat = AllocateAnonymousAndWrite(Buffer((byte*)&dlopen_dat, sizeof(dlopen_dat)), PROT_READ | PROT_WRITE);
    Buffer shellcode_buf((byte*)&DlOpenShellcode, 0x1000);
    //shellcode_buf.DumpToFile("sc.bin");
    void* p_shellcode = AllocateAnonymousAndWrite(shellcode_buf);
    StartThreadAtAddress(p_shellcode, (uint64)p_dlopen_dat);
    
    // Buffer module_buf(filename);
    // //rwx by default
    // byte* module_dst_buf = (byte*)AllocateAnonymous(module_buf.buf_size, PROT_READ | PROT_EXEC);
    // WriteBufferToProcess(module_buf, module_dst_buf);

    // StartThreadAtAddress(module_dst_buf + entry_offset);

    return true;
}