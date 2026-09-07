@name("malloc")
public extern fn c_malloc(size: i64) -> *mut void

@name("free")
public extern fn c_free(ptr: *mut void) -> void

@name("strlen")
public extern fn c_strlen(value: cstr) -> i64

@name("memcpy")
public extern fn c_memcpy(destination: *mut void, source: *mut void, size: i64) -> *mut void

@name("strcat")
public extern fn c_strcat(destination: cstr, source: cstr) -> cstr

@name("strcmp")
public extern fn c_strcmp(left: cstr, right: cstr) -> i32

@name("strncmp")
public extern fn c_strncmp(left: cstr, right: cstr, size: i64) -> i32

@name("strtoll")
public extern fn c_strtoll(value: cstr, end: *mut cstr, base: i32) -> i64

@name("strtoull")
public extern fn c_strtoull(value: cstr, end: *mut cstr, base: i32) -> u64

@name("strtod")
public extern fn c_strtod(value: cstr, end: *mut cstr) -> f64

@name("snprintf")
public extern fn c_snprintf(buffer: cstr, size: i64, format: cstr, ...) -> i32

@name("exit")
public extern fn c_exit(status: i32) -> never

@name("write")
public extern fn c_write(file_descriptor: i32, buffer: *void, length: i64) -> i64

@name("__errno_location")
@os("linux")
public extern fn zep_errno() -> *mut i32

@name("__error")
@os("macos")
public extern fn zep_errno() -> *mut i32

@name("socket")
public extern fn c_socket(domain: i32, kind: i32, protocol: i32) -> i32

@name("bind")
public extern fn c_bind(file_descriptor: i32, address: *void, length: u32) -> i32

@name("listen")
public extern fn c_listen(file_descriptor: i32, backlog: i32) -> i32

@name("accept")
public extern fn c_accept(file_descriptor: i32, address: *mut void, length: *mut u32) -> i32

@name("connect")
public extern fn c_connect(file_descriptor: i32, address: *void, length: u32) -> i32

@name("getsockname")
public extern fn c_getsockname(file_descriptor: i32, address: *mut void, length: *mut u32) -> i32

@name("setsockopt")
public extern fn c_setsockopt(file_descriptor: i32, level: i32, name: i32, value: *void, length: u32) -> i32

@name("recv")
public extern fn c_recv(file_descriptor: i32, buffer: *mut void, length: i64, flags: i32) -> i64

@name("send")
public extern fn c_send(file_descriptor: i32, buffer: *void, length: i64, flags: i32) -> i64

@name("close")
public extern fn c_close(file_descriptor: i32) -> i32

@name("htons")
public extern fn c_htons(value: u16) -> u16

@name("ntohs")
public extern fn c_ntohs(value: u16) -> u16

@name("htonl")
public extern fn c_htonl(value: u32) -> u32

public enum FfiSocketDomain : i32 {
    Ipv4 = 2
}

public enum FfiSocketKind : i32 {
    Stream = 1
}

public enum FfiSocketLevel : i32 {
    Socket = 1
}

public enum FfiSocketAddressLength : u32 {
    Ipv4 = 16
}

public enum FfiFileDescriptor : i32 {
    Invalid = -1
}

public enum FfiSocketOption : i32 {
    ReuseAddress = 2

    public:
        @os("linux")
        static fn reuse_address() -> i32 {
            return 2
        }

        @os("macos")
        static fn reuse_address() -> i32 {
            return 4
        }
}

@name("fopen")
public extern fn c_fopen(path: cstr, mode: cstr) -> *mut void

@name("fread")
public extern fn c_fread(buffer: *mut void, size: i64, count: i64, stream: *mut void) -> i64

@name("fwrite")
public extern fn c_fwrite(buffer: *void, size: i64, count: i64, stream: *mut void) -> i64

@name("fclose")
public extern fn c_fclose(stream: *mut void) -> i32

@name("fseek")
public extern fn c_fseek(stream: *mut void, offset: i64, whence: i32) -> i32

@name("ftell")
public extern fn c_ftell(stream: *mut void) -> i64

@name("remove")
public extern fn c_remove(path: cstr) -> i32

@name("access")
public extern fn c_access(path: cstr, mode: i32) -> i32

@name("system")
public extern fn c_system(command: cstr) -> i32

@name("popen")
public extern fn c_popen(command: cstr, mode: cstr) -> *mut void

@name("pclose")
public extern fn c_pclose(stream: *mut void) -> i32

@name("fgets")
public extern fn c_fgets(buffer: *mut void, size: i32, stream: *mut void) -> cstr

@name("getenv")
public extern fn c_getenv(name: cstr) -> cstr

@name("mkdir")
public extern fn c_mkdir(path: cstr, mode: u32) -> i32

@name("realpath")
public extern fn c_realpath(path: cstr, buffer: *mut char) -> cstr

@name("opendir")
public extern fn c_opendir(path: cstr) -> *mut void

@name("readdir")
public extern fn c_readdir(directory: *mut void) -> *mut void

@name("closedir")
public extern fn c_closedir(directory: *mut void) -> i32
