//
// Created by luoyesiqiu
//

#include <libgen.h>
#include <ctime>
#include <elf.h>
#include <pthread.h>
#include <errno.h>
#include <dlfcn.h>
#include "dpt_util.h"
#include "common/dpt_log.h"
#include "minizip-ng/mz_strm.h"

using namespace dpt;


int dpt_mprotect(void *start,void *end,int prot) {
    uintptr_t start_addr = PAGE_START((uintptr_t)start);
    uintptr_t end_addr = PAGE_START((uintptr_t)end - 1) + PAGE_SIZE;
    size_t size = end_addr - start_addr;

    DLOGD("%s begin: %p, size: %zu",__FUNCTION__,(void *)start_addr,size);


    if (0 != mprotect((void *)start_addr, size, prot)) return -1;
    return 0;
}

static int separate_dex_number(std::string *str) {
    int sum = 0;
    int mul = 1;
    for(auto it = str->cend();it >= str->cbegin();it--){
        if(isdigit(*it)){
            int number = *it - '0';
            sum += (number * mul);
            mul *= 10;
        }
        else{
            if(sum != 0 ) break;
        }
    }
    return sum;
}
/**
 * Get dex index from dex location
 * e.g. base.apk!classes2.dex will get 1
 */
int parse_dex_number(std::string *location) {
    int raw_dex_index = 1;
    if (location->rfind(".dex") != std::string::npos) {
        size_t sep = location->rfind('!');
        if(sep != std::string::npos){
            raw_dex_index = separate_dex_number(location);
        }
        else{
            sep = location->rfind(':');
            if(sep != std::string::npos){
                raw_dex_index = separate_dex_number(location);
            }
        }
    } else {
        raw_dex_index = 1;
    }

    return raw_dex_index - 1;
}

void parseClassName(const char *src, char *dest) {
    for (int i = 0; *(src + i) != '\0'; i++) {
        if (*(src + i) == '.') {
            dest[i] = '/';
        } else {
            *(dest + i) = *(src + i);
        }
    }
}

void getClassName(JNIEnv *env,jobject obj,char *destClassName,size_t max_len) {
    jclass objCls = env->GetObjectClass(obj);
    reflect::java_lang_Class cls(env,objCls);
    jstring classNameInner = cls.getName();

    const char *classNameInnerChs = env->GetStringUTFChars(classNameInner,nullptr);

    snprintf(destClassName,max_len,"%s",classNameInnerChs);

    env->ReleaseStringUTFChars(classNameInner,classNameInnerChs);
}

jclass getContextClass(JNIEnv *env) {
    if (g_ContextClass == nullptr) {
        jclass ContextClass = env->FindClass("android/content/Context");
        g_ContextClass = (jclass) env->NewGlobalRef(ContextClass);
    }
    return g_ContextClass;
}

AAssetManager *getAssetMgr(JNIEnv *env, jobject assetManager) {
    if (g_AssetMgrInstance == nullptr) {
        g_AssetMgrInstance = AAssetManager_fromJava(env, assetManager);
    }
    return g_AssetMgrInstance;
}

AAsset *getAsset(JNIEnv *env, jobject context, const char *filename) {
    if (context != nullptr) {
        jclass contextClass = getContextClass(env);
        jmethodID getAssetsId = env->GetMethodID(contextClass, "getAssets",
                                                 "()Landroid/content/res/AssetManager;");
        jobject assetManagerObj = env->CallObjectMethod(context, getAssetsId);
        AAssetManager *aAssetManager = getAssetMgr(env, assetManagerObj);
        if (aAssetManager != nullptr) {
            AAsset *aAsset = AAssetManager_open(g_AssetMgrInstance,
                                                filename,
                                                AASSET_MODE_BUFFER);
            return aAsset;
        }
    }
    return nullptr;
}

void getApkPath(JNIEnv *env,char *apkPathOut,size_t max_out_len){
    reflect::android_app_ActivityThread activityThread(env);
    jobject mBoundApplicationObj = activityThread.getBoundApplication();

    reflect::android_app_ActivityThread::AppBindData appBindData(env,mBoundApplicationObj);
    jobject appInfoObj = appBindData.getAppInfo();

    reflect::android_content_pm_ApplicationInfo applicationInfo(env,appInfoObj);
    auto sourceDir = applicationInfo.getSourceDir();

    const char *sourceDirChs = env->GetStringUTFChars(sourceDir,nullptr);
    snprintf(apkPathOut,max_out_len,"%s",sourceDirChs);

    DLOGD("getApkPath: %s",apkPathOut);
}

void getDataDir(JNIEnv *env,char *dataDirOut,size_t max_out_len) {
    reflect::android_app_ActivityThread activityThread(env);
    jobject mBoundApplicationObj = activityThread.getBoundApplication();

    reflect::android_app_ActivityThread::AppBindData appBindData(env,mBoundApplicationObj);
    jobject appInfoObj = appBindData.getAppInfo();

    reflect::android_content_pm_ApplicationInfo applicationInfo(env,appInfoObj);
    auto sourceDir = applicationInfo.getDataDir();

    const char *dataDirChs = env->GetStringUTFChars(sourceDir,nullptr);
    snprintf(dataDirOut, max_out_len , "%s",dataDirChs);

    DLOGD("data dir: %s",dataDirOut);
}

jstring getApkPathExport(JNIEnv *env,jclass __unused) {
    char apkPathChs[256] = {0};
    getApkPath(env,apkPathChs, ARRAY_LENGTH(apkPathChs));

    return env->NewStringUTF(apkPathChs);
}

void getCompressedDexesPath(JNIEnv *env,char *outDexZipPath,size_t max_len) {
    char dataDir[256] = {0};
    getDataDir(env,dataDir, ARRAY_LENGTH(dataDir));
    snprintf(outDexZipPath,max_len,"%s/%s/%s",dataDir,CACHE_DIR,DEXES_ZIP_NAME);
}

void getCodeCachePath(JNIEnv *env,char *outCodeCachePath,size_t max_len) {
    char dataDir[256] = {0};
    getDataDir(env,dataDir, ARRAY_LENGTH(dataDir));
    snprintf(outCodeCachePath,max_len,"%s/%s/",dataDir,CACHE_DIR);
}

jstring getCompressedDexesPathExport(JNIEnv *env,jclass __unused) {
    char dexesPath[256] = {0};
    getCompressedDexesPath(env, dexesPath, ARRAY_LENGTH(dexesPath));
    return env->NewStringUTF(dexesPath);
}

static void writeDexAchieve(const char *dexAchievePath,void *apk_addr,size_t apk_size) {
    DLOGD("zipCode open = %s",dexAchievePath);
    FILE *fp = fopen(dexAchievePath, "wb");
    if(fp != nullptr){
        uint64_t dex_files_size = 0;
        void *dexFilesData = nullptr;
        bool needFree = read_zip_file_entry(apk_addr,apk_size,DEX_FILES_NAME_IN_ZIP,&dexFilesData,&dex_files_size);
        if(dexFilesData != nullptr) {
            fwrite(dexFilesData, 1, dex_files_size, fp);
        }
        fclose(fp);
        if(needFree) {
            DPT_FREE(dexFilesData);
        }
    }
    else {
        DLOGE("WTF! zipCode write fail: %s", strerror(errno));
    }
}

void extractDexesInNeeded(JNIEnv *env,void *apk_addr,size_t apk_size) {
    char compressedDexesPathChs[256] = {0};
    getCompressedDexesPath(env,compressedDexesPathChs, ARRAY_LENGTH(compressedDexesPathChs));

    char codeCachePathChs[256] = {0};
    getCodeCachePath(env,codeCachePathChs, ARRAY_LENGTH(codeCachePathChs));

    if(access(codeCachePathChs, F_OK) == 0){
        if(access(compressedDexesPathChs, F_OK) != 0) {
            writeDexAchieve(compressedDexesPathChs,apk_addr,apk_size);
            chmod(compressedDexesPathChs,0444);
            DLOGI("extractDexes %s write finish",compressedDexesPathChs);

        }
        else {
            DLOGI("extractDexes dex files is achieved!");
        }
    }
    else {
        if(mkdir(codeCachePathChs,0775) == 0){
            writeDexAchieve(compressedDexesPathChs,apk_addr,apk_size);
            chmod(compressedDexesPathChs,0444);
        }
        else {
            DLOGE("WTF! extractDexes cannot make code_cache directory!");
        }
    }
}


static void load_zip_by_mmap(const char* zip_file_path,void **zip_addr,size_t *zip_size) {
    int fd = open(zip_file_path,O_RDONLY);
    if(fd <= 0){
        DLOGE("load_zip cannot open file!");
        return;
    }
    struct stat fst;
    fstat(fd,&fst);
    const int page_size = sysconf(_SC_PAGESIZE);
    const size_t need_zip_size = (fst.st_size / page_size) * page_size + page_size;
    DLOGD("%s fst.st_size = " FMT_INT64_T ",need size = %zu" ,__FUNCTION__ ,fst.st_size,need_zip_size);
    *zip_addr = mmap64(nullptr,
                       need_zip_size,
                       PROT_READ ,
                       MAP_PRIVATE,
                       fd,
                       0);
    *zip_size = fst.st_size;

    DLOGD("%s addr:" FMT_POINTER " size: %zu" ,__FUNCTION__ ,(uintptr_t)*zip_addr,*zip_size);
}

static void load_zip(const char* zip_file_path,void **zip_addr,size_t *zip_size) {
    DLOGD("load_zip by mmap");
    load_zip_by_mmap(zip_file_path, zip_addr, zip_size);

    DLOGD("%s start: %p size: %zu" ,__FUNCTION__ , *zip_addr,*zip_size);
}

void load_apk(JNIEnv *env,void **apk_addr,size_t *apk_size) {
    char apkPathChs[512] = {0};
    getApkPath(env,apkPathChs,ARRAY_LENGTH(apkPathChs));
    load_zip(apkPathChs,apk_addr,apk_size);

}

void unload_apk(void *apk_addr,size_t apk_size) {
    if(apk_addr != nullptr) {
        munmap(apk_addr,apk_size);
        DLOGD("%s addr:" FMT_POINTER " size: %zu" ,__FUNCTION__ ,(uintptr_t)apk_addr,apk_size);
    }
}

bool read_zip_file_entry(void* zip_addr,off_t zip_size,const char* entry_name,void **entry_addr,uint64_t *entry_size) {
    DLOGD("read_zip_file_entry prepare read file: %s",entry_name);

    void *mem_stream = nullptr;
    void *zip_handle = nullptr;
    bool needFree = false;

    mz_stream_mem_create(&mem_stream);
    mz_stream_mem_set_buffer(mem_stream, zip_addr, zip_size);
    mz_stream_open(mem_stream, nullptr, MZ_OPEN_MODE_READ);

    mz_zip_create(&zip_handle);
    int32_t err = mz_zip_open(zip_handle, mem_stream, MZ_OPEN_MODE_READ);

    if(err == MZ_OK){
        err = mz_zip_goto_first_entry(zip_handle);
        while (err == MZ_OK) {
            mz_zip_file *file_info = nullptr;
            err = mz_zip_entry_get_info(zip_handle, &file_info);

            if (err == MZ_OK) {
                if (strncmp(file_info->filename, entry_name, 256) == 0) {
                    DLOGD("read_zip_file_entry found entry name = %s,file size = " FMT_INT64_T,
                          file_info->filename,
                          file_info->uncompressed_size);

                    err = mz_zip_entry_read_open(zip_handle, 0, nullptr);
                    if (err != MZ_OK) {
                        DLOGW("read_zip_file_entry not prepared: %d", err);
                        continue;
                    }
                    needFree = true;
                    DLOGD("read_zip_file_entry compress method is: %d",
                          file_info->compression_method);

                    *entry_addr = calloc(file_info->uncompressed_size + 1, 1);
                    DLOGD("read_zip_file_entry start read: %s", file_info->filename);

                    __unused size_t bytes_read = mz_zip_entry_read(zip_handle, *entry_addr,
                                                          file_info->uncompressed_size);

                    DLOGD("read_zip_file_entry reading entry: %s,read size: %zu", entry_name,
                          bytes_read);

                    *entry_size = (file_info->uncompressed_size);

                    goto tail;
                } // strncmp
            }
            else{
                DLOGW("read_zip_file_entry get entry info err: %d",err);
                break;
            }
            err = mz_zip_goto_next_entry(zip_handle);
        } // while
    }
    else{
        DLOGW("read_zip_file_entry zip open fail: %d",err);
    } // zip open

    tail: {
        return needFree;
    }
}

void get_elf_section(Elf_Shdr *target,const char *elf_path,const char *sh_name) {
    if(elf_path == NULL) {
        return;
    }
    DLOGD("%s path: %s",__FUNCTION__,elf_path);

    FILE *elf_fp = fopen(elf_path, "r");
    if(!elf_fp) {
        return;
    }

    fseek(elf_fp, 0L, SEEK_END);
    size_t lib_size = ftell(elf_fp);
    fseek(elf_fp, 0L, SEEK_SET);

    uint8_t *data = (uint8_t *) calloc(lib_size, 1);
    fread(data, 1, lib_size, elf_fp);

    uint8_t *elf_bytes_data = data;

    Elf_Ehdr *ehdr = (Elf_Ehdr *) elf_bytes_data;
    Elf_Shdr *shdr = (Elf_Shdr *) (((uint8_t *) elf_bytes_data) + ehdr->e_shoff);
    DLOGD("%s section header table addr = %p, offset = %x", __FUNCTION__, shdr,
          (int) ehdr->e_shoff);
    Elf_Half shstrndx = ehdr->e_shstrndx;

    //seek to .shstr
    Elf_Shdr *shstr_shdr = (Elf_Shdr *)((uint8_t *) shdr + sizeof(Elf_Shdr) * shstrndx);

    uint8_t *shstr_addr = elf_bytes_data + shstr_shdr->sh_offset;

    DLOGD("%s shstr addr: %p", __FUNCTION__, shstr_addr);

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char *section_name = reinterpret_cast<const char *>((char *)shstr_addr + shdr->sh_name);
        if (strcmp(section_name, sh_name) == 0) {
            DLOGD("%s find section %s,offset: %x", __FUNCTION__,sh_name,(int)shdr->sh_offset);

            memcpy(target,shdr,sizeof(Elf_Shdr));
        }
        shdr++;
    }
    free(data);
    fclose(elf_fp);
}

const char* find_symbol_in_elf_file(const char *elf_file,int keyword_count,...) {
    FILE *elf_fp = fopen(elf_file, "r");
    if(elf_fp) {
        fseek(elf_fp, 0L, SEEK_END);
        size_t lib_size = ftell(elf_fp);
        fseek(elf_fp, 0L, SEEK_SET);

        char *data = (char *) calloc(lib_size, 1);
        fread(data, 1, lib_size, elf_fp);

        char *elf_bytes_data = data;
        Elf_Ehdr *ehdr = (Elf_Ehdr*)elf_bytes_data;

        Elf_Shdr *shdr = (Elf_Shdr *)(((uint8_t*) elf_bytes_data) + ehdr->e_shoff);

        va_list kw_list;

        for (int i = 0; i < ehdr->e_shnum;i++) {

            if(shdr->sh_type == SHT_STRTAB){
                const char* str_base = (char *)((uint8_t*)elf_bytes_data + shdr->sh_offset);
                char* ptr = (char *)str_base;

                for(int k = 0; ptr < (str_base + shdr->sh_size);k++){
                    const char* item_value = ptr;
                    size_t item_len = strnlen(item_value,128);
                    ptr += (item_len + 1);

                    if(item_len == 0){
                        continue;
                    }
                    int match_count = 0;
                    va_start(kw_list,keyword_count);
                    for(int n = 0;n < keyword_count;n++){
                        const char *keyword = va_arg(kw_list,const char*);
                        if(strstr(item_value,keyword)){
                            match_count++;
                        }
                    }
                    va_end(kw_list);
                    if(match_count == keyword_count){
                        return item_value;
                    }
                }
                break;
            }

            shdr++;
        }
        fclose(elf_fp);
        free(data);
    }
    return nullptr;
}

void hexdump(__unused const char* name,const void* data, size_t size){
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    char *buffer = (char*)calloc(size,1);
    const size_t MAX_LEN = size/2;
    char *item = (char*)calloc(MAX_LEN,1);
    for (i = 0; i < size; ++i) {
        memset(item,0,MAX_LEN);
        snprintf(item,MAX_LEN,"%02X ", ((unsigned char*)data)[i]);
        snprintf(buffer + strlen(buffer),MAX_LEN,"%s",item);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i+1) % 8 == 0 || i+1 == size) {
            memset(item,0,MAX_LEN);
            snprintf(item,MAX_LEN,"%s"," ");
            snprintf(buffer + strlen(buffer),MAX_LEN,"%s",item);

            if ((i+1) % 16 == 0) {
                memset(item,0,MAX_LEN);
                snprintf(item,MAX_LEN,"|  %s ", ascii);
                snprintf(buffer + strlen(buffer),MAX_LEN,"%s\n",item);

            } else if (i+1 == size) {
                ascii[(i+1) % 16] = '\0';
                if ((i+1) % 16 <= 8) {
                    memset(item,0,MAX_LEN);
                    snprintf(item,MAX_LEN,"%s"," ");
                    snprintf(buffer + strlen(buffer) + 1,MAX_LEN,"%s",item);
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    memset(item,0,MAX_LEN);
                    snprintf(item,MAX_LEN,"%s","   ");
                    snprintf(buffer + strlen(buffer),MAX_LEN,"%s",item);
                }
                memset(item,0,MAX_LEN);
                snprintf(item,MAX_LEN,"|  %s ", ascii);
                snprintf(buffer + strlen(buffer),MAX_LEN,"%s\n",item);
            }
        }
    }
    DLOGD("%s: \n%s",name,buffer);
    free(item);
    free(buffer);
}


int find_in_maps(int count,...) {
    const int MAX_READ_LINE = 10 * 1024;
    char maps_path[128] = {0};
    snprintf(maps_path, ARRAY_LENGTH(maps_path), "/proc/%d/maps", getpid());
    FILE *fp = fopen(maps_path, "r");
    int found = 0;
    if (fp != nullptr) {
        char line[512] = {0};
        int read_line = 0;
        va_list ap;
        while (fgets(line, sizeof(line), fp) != nullptr) {
            if (read_line++ >= MAX_READ_LINE) {
                break;
            }
            char item_name[128] = {0};
#ifdef __LP64__
            int ret = sscanf(line, "%*llx-%*llx %*s %*llx %*s %*s %s", item_name);
#else
            int ret = sscanf(line, "%*x-%*x %*s %*x %*s %*s %s", item_name);
#endif

            if(ret != 1) {
                continue;
            }
            va_start(ap,count);

            for(int i = 0;i < count;i++) {
                const char *arg = va_arg(ap,const char *);
                if(strstr(item_name,arg) != 0) {
                    DLOGD("found %s in %s",arg,item_name);
                    found++;
                }
            }
            va_end(ap);

        }
    }

    return found;
}

DPT_ENCRYPT int find_in_threads_list(int count,...) {
    char task_path[128] = {0};
    pid_t pid = getpid();
    snprintf(task_path, ARRAY_LENGTH(task_path), "/proc/%d/task",pid);
    DIR *task_dir;
    if((task_dir = opendir(task_path)) == nullptr) {
        return 0;
    }

    int match_count = 0;

    struct dirent *de;
    va_list ap;
    while ((de = readdir(task_dir)) != nullptr) {
        if(isdigit(de->d_name[0])) {
            int tid = atoi(de->d_name);
            if(tid == pid) {
                continue;
            }
            char stat_path[256] = {0};
            snprintf(stat_path, ARRAY_LENGTH(stat_path),"%s/%d/%s",task_path,tid,"stat");
            FILE *fp = fopen(stat_path,"r");
            char buf[256] = {0};
            if(fp) {
                fgets(buf,256,fp);

                char *t_name = nullptr;
                for(size_t i = 0; i < strnlen(buf,256);i++) {
                    if(buf[i] == '(') {
                        t_name = &buf[i + 1];
                    }

                    if(buf[i] == ')') {
                        buf[i] = '\0';
                        break;
                    }

                }

                if(t_name != nullptr) {
                    va_start(ap,count);
                    for (int i = 0; i < count; i++) {
                        const char *arg = va_arg(ap, const char *);
                        if (strncmp(t_name, arg, 256) == 0) {
                            DLOGD("match thread name: %s", t_name);
                            match_count++;
                        }
                    }
                    va_end(ap);
                }
                fclose(fp);
            }
        }
    }

    if(task_dir) {
        closedir(task_dir);
    }
    return match_count;
}

void appendLog(const char* log){
    FILE *fp = fopen("nlog.log","aw");
    if(NULL != fp){
        fwrite(log,1,strlen(log),fp);
        fwrite("\n",1,1,fp);
        fclose(fp);
    }
}

void printTime(__unused const char* msg,__unused clock_t start){
    __unused clock_t end = clock();
    DLOGD("%s %lf",msg,(double)(end - start) / CLOCKS_PER_SEC);
}

const char* getThreadName(){
    static char threadName[256];
    memset(threadName, 0, 256);
    prctl(PR_GET_NAME, (unsigned long)threadName);
    return threadName;
}