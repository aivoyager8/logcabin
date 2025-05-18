#ifndef LOGCABIN_STORAGE_LSMSTORAGE_H
#define LOGCABIN_STORAGE_LSMSTORAGE_H

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <functional>

#include "Core/Buffer.h"
#include "Core/ConditionVariable.h"
#include "Core/Mutex.h"
#include "Storage/Log.h"

namespace LogCabin {
namespace Storage {

/**
 * 基于LSM树实现的日志存储系统。
 * 
 * LSM (Log-Structured Merge)树是一种针对写入优化的数据结构，类似于LevelDB或RocksDB的底层结构。
 * 它将写入先放入内存中的有序表，然后周期性地将这些表合并并写入磁盘。这种方式减少了随机写入，
 * 提供了更高的写入吞吐量，适合LogCabin这类以写入为主的系统。
 */
class LSMStorage : public Log {
  public:
    /**
     * 构造函数。
     * @param path 存储路径
     * @param config 存储配置对象
     */
    LSMStorage(const std::string& path, const Config& config);
    
    /**
     * 析构函数。
     * 确保后台合并线程安全退出，并释放所有资源。
     */
    virtual ~LSMStorage();
    
    /**
     * 附加一个条目到日志中。参见Log::append接口。
     */
    virtual Result append(const Entry& entry);
    
    /**
     * 读取指定位置的条目。参见Log::getEntry接口。
     */
    virtual Result getEntry(uint64_t logIndex, Entry& entry) const;
    
    /**
     * 获取最后一个条目的索引。参见Log::getLastLogIndex接口。
     */
    virtual uint64_t getLastLogIndex() const;
    
    /**
     * 获取元数据。参见Log::getMetadata接口。
     */
    virtual Result getMetadata(Metadata& metadata) const;
    
    /**
     * 更新元数据。参见Log::updateMetadata接口。
     */
    virtual Result updateMetadata(const Metadata& metadata);
    
    /**
     * 在指定位置截断日志。参见Log::truncate接口。
     */
    virtual Result truncate(uint64_t logIndex);
    
    /**
     * 强制刷盘，将内存表数据立即写入磁盘。
     */
    Result flush();
    
    /**
     * 关闭后台合并线程，仅用于测试或特殊场景。
     */
    void disableCompaction();
    
  private:
    /**
     * 后台线程入口，执行LSM树的合并操作。
     */
    void compact();
    
    /**
     * 布隆过滤器类型声明（具体实现可用第三方库，如libbloom/boost等）
     */
    struct BloomFilter;
    using BloomFilterPtr = std::shared_ptr<BloomFilter>;

    /**
     * 表示LSM树中一个层级的数据。
     */
    struct Level {
        std::map<uint64_t, Core::Buffer> entries;
        uint64_t min_index;
        uint64_t max_index;
        BloomFilterPtr bloom; // 新增
    };
    
    /**
     * 将内存中的数据刷新到磁盘。
     */
    Result flushMemTable();
    
    /**
     * 合并两个层级。
     */
    Result mergeLevels(size_t level_index);
    
    /**
     * 从磁盘加载现有数据。
     */
    Result loadFromDisk();
    
    /**
     * 写入磁盘操作。
     */
    Result writeToDisk(const Level& level, size_t level_index);
    
    /**
     * 从磁盘读取指定层级。
     */
    Result readFromDisk(Level& level, size_t level_index);
    
    /**
     * 获取层级文件路径。
     */
    std::string getLevelPath(size_t level_index) const;
    
    // 存储路径
    const std::string path;
    
    // 配置信息
    const Config config;
    
    // 内存表，尚未写入磁盘的数据
    Level memTable;
    
    // 磁盘上的各级数据
    std::vector<Level> diskLevels;
    
    // 最后一个日志索引
    std::atomic<uint64_t> lastLogIndex;
    
    // 元数据缓存
    mutable Metadata cachedMetadata;
    
    // 用于保护内存表的互斥锁
    mutable Core::Mutex mutex;
    
    // 条件变量，用于通知后台合并线程
    Core::ConditionVariable compactCond;
    
    // 后台合并线程
    std::unique_ptr<std::thread> compactThread;
    
    // 线程是否应继续运行
    std::atomic<bool> running;
    
    // 磁盘是否已经初始化
    bool initialized;

    // 刷盘间隔（秒），0为仅按需刷盘
    uint32_t flushIntervalSec = 0;
    // 是否禁用后台合并
    bool disableCompactionFlag = false;
    // 压缩算法
    std::string compressionAlgorithm;
    // 布隆过滤器误报率
    double bloomFilterFPRate = 0.01;

    // 预留：全局布隆过滤器（可选）
    BloomFilterPtr globalBloom;
};

} // namespace Storage
} // namespace LogCabin

#endif // LOGCABIN_STORAGE_LSMSTORAGE_H
