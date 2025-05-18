#include "Storage/LSMStorage.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

#include "Core/Debug.h"
#include "Core/StringUtil.h"
#include "Core/Util.h"
#include "Storage/LogFactory.h"

namespace LogCabin {
namespace Storage {

namespace {

/**
 * 创建一个LSM存储的配置对象。
 */
Log::Config
makeConfig(uint64_t bufferSizeMB,
           bool compressionEnabled,
           const std::string& compressionAlgorithm,
           uint32_t maxLevels,
           double bloomFilterFPRate)
{
    Log::Config config;
    config["lsm_buffer_size_mb"] = Core::StringUtil::format("%lu", bufferSizeMB);
    config["lsm_compression_enabled"] = compressionEnabled ? "true" : "false";
    config["lsm_compression_algorithm"] = compressionAlgorithm;
    config["lsm_max_levels"] = Core::StringUtil::format("%u", maxLevels);
    config["lsm_bloom_filter_fp_rate"] = Core::StringUtil::format("%.5f", bloomFilterFPRate);
    return config;
}

} // anonymous namespace

LSMStorage::LSMStorage(const std::string& path, const Config& config)
    : path(path)
    , config(config)
    , memTable()
    , diskLevels()
    , lastLogIndex(0)
    , cachedMetadata()
    , mutex()
    , compactCond()
    , compactThread()
    , running(true)
    , initialized(false)
    // 新增成员初始化
    , flushIntervalSec(0)
    , disableCompactionFlag(false)
    , compressionAlgorithm("zstd")
    , bloomFilterFPRate(0.01)
    , globalBloom(nullptr)
{
    // 创建存储目录（如果不存在）
    mkdir(path.c_str(), 0755);
    
    // 初始化内存表
    memTable.min_index = std::numeric_limits<uint64_t>::max();
    memTable.max_index = 0;
    
    // 解析新配置
    if (config.count("lsmFlushIntervalSec"))
        flushIntervalSec = std::stoul(config.at("lsmFlushIntervalSec"));
    if (config.count("lsmDisableCompaction"))
        disableCompactionFlag = (config.at("lsmDisableCompaction") == "true");
    if (config.count("lsmCompressionAlgorithm"))
        compressionAlgorithm = config.at("lsmCompressionAlgorithm");
    if (config.count("lsmBloomFilterFPRate"))
        bloomFilterFPRate = std::stod(config.at("lsmBloomFilterFPRate"));

    // 加载磁盘数据
    loadFromDisk();
    
    // 启动后台合并线程
    if (!disableCompactionFlag)
        compactThread.reset(new std::thread(&LSMStorage::compact, this));
}

LSMStorage::~LSMStorage() 
{
    running.store(false);
    compactCond.notify_all();
    if (compactThread && compactThread->joinable()) {
        compactThread->join();
    }
    flush();
}

// 新增：外部可调用的强制刷盘方法
Log::Result
LSMStorage::flush()
{
    Log::Result res = flushMemTable();
    compactCond.notify_one();
    return res;
}

// 新增：关闭后台合并线程（仅测试或特殊场景使用）
void
LSMStorage::disableCompaction()
{
    running.store(false);
    compactCond.notify_all();
    if (compactThread && compactThread->joinable()) {
        compactThread->join();
    }
}

Log::Result
LSMStorage::append(const Log::Entry& entry)
{
    Core::Buffer buffer;
    buffer.append(&entry.type, sizeof(entry.type));
    buffer.append(&entry.data);
    
    Core::Mutex::Lock lock(mutex);
    
    if (memTable.entries.empty()) {
        memTable.min_index = entry.index;
    }
    
    memTable.entries[entry.index] = std::move(buffer);
    memTable.max_index = std::max(memTable.max_index, entry.index);

    // TODO: 更新memTable布隆过滤器
    // if (memTable.bloom) memTable.bloom->add(entry.index);

    // 更新最后日志索引
    lastLogIndex.store(memTable.max_index);
    
    // 检查内存表大小，如果超过阈值，刷新到磁盘
    uint64_t bufferSizeMB = std::stoull(config.at("lsm_buffer_size_mb"));
    size_t sizeBytes = 0;
    for (const auto& e : memTable.entries) {
        sizeBytes += e.second.getLength();
    }
    
    if (sizeBytes >= bufferSizeMB * 1024 * 1024) {
        lock.unlock();
        flushMemTable();
        compactCond.notify_one();
    }
    return Log::Result::SUCCESS;
}

Log::Result
LSMStorage::getEntry(uint64_t logIndex, Log::Entry& entry) const
{
    entry.index = logIndex;
    
    // 先查找内存表
    {
        Core::Mutex::Lock lock(mutex);
        auto it = memTable.entries.find(logIndex);
        if (it != memTable.entries.end()) {
            const Core::Buffer& buffer = it->second;
            memcpy(&entry.type, buffer.getData(), sizeof(entry.type));
            entry.data.append(
                static_cast<const char*>(buffer.getData()) + sizeof(entry.type),
                buffer.getLength() - sizeof(entry.type));
            return Log::Result::SUCCESS;
        }
    }
    
    // 按层级查找磁盘数据
    for (const auto& level : diskLevels) {
        if (logIndex >= level.min_index && logIndex <= level.max_index) {
            auto it = level.entries.find(logIndex);
            if (it != level.entries.end()) {
                const Core::Buffer& buffer = it->second;
                memcpy(&entry.type, buffer.getData(), sizeof(entry.type));
                entry.data.append(
                    static_cast<const char*>(buffer.getData()) + sizeof(entry.type),
                    buffer.getLength() - sizeof(entry.type));
                return Log::Result::SUCCESS;
            }
        }
    }
    
    return Log::Result::INVALID_LOG_INDEX;
}

uint64_t
LSMStorage::getLastLogIndex() const
{
    return lastLogIndex.load();
}

Log::Result
LSMStorage::getMetadata(Log::Metadata& metadata) const
{
    Core::Mutex::Lock lock(mutex);
    metadata = cachedMetadata;
    return Log::Result::SUCCESS;
}

Log::Result
LSMStorage::updateMetadata(const Log::Metadata& metadata)
{
    Core::Mutex::Lock lock(mutex);
    cachedMetadata = metadata;
    
    // 将元数据写入专用文件
    std::string metadataPath = path + "/metadata";
    std::ofstream metadataFile(metadataPath.c_str(), std::ios::binary);
    if (!metadataFile) {
        return Log::Result::IO_ERROR;
    }
    
    metadataFile.write(reinterpret_cast<const char*>(&metadata.versionNumber),
                      sizeof(metadata.versionNumber));
    metadataFile.write(reinterpret_cast<const char*>(&metadata.currentTerm),
                      sizeof(metadata.currentTerm));
    metadataFile.write(reinterpret_cast<const char*>(&metadata.votedFor),
                      sizeof(metadata.votedFor));
    
    if (metadataFile.fail()) {
        return Log::Result::IO_ERROR;
    }
    
    return Log::Result::SUCCESS;
}

Log::Result
LSMStorage::truncate(uint64_t logIndex)
{
    // 截断内存表
    {
        Core::Mutex::Lock lock(mutex);
        auto it = memTable.entries.lower_bound(logIndex);
        while (it != memTable.entries.end()) {
            it = memTable.entries.erase(it);
        }
        
        if (memTable.entries.empty()) {
            memTable.min_index = std::numeric_limits<uint64_t>::max();
            memTable.max_index = 0;
        } else {
            memTable.max_index = memTable.entries.rbegin()->first;
        }
    }
    
    // 更新最后日志索引
    lastLogIndex.store(logIndex - 1);
    
    // 重新初始化磁盘数据
    // 注意：这是一种简单但不高效的实现
    // 实际生产中应该重构数据而不是重新加载
    return loadFromDisk();
}

void
LSMStorage::compact()
{
    while (running.load()) {
        // 等待通知或定期检查是否需要合并
        {
            Core::Mutex::Lock lock(mutex);
            compactCond.wait(lock, [this] {
                return !running.load() || !memTable.entries.empty();
            });
            if (!running.load()) break;
        }
        
        // 尝试合并层级
        uint32_t maxLevels = std::stoul(config.at("lsm_max_levels"));
        for (size_t i = 0; i < diskLevels.size() && i < maxLevels - 1; ++i) {
            // 实现简单的合并策略：如果当前层的大小超过下一层的1/4，则合并
            if (i + 1 < diskLevels.size() && 
                diskLevels[i].entries.size() > diskLevels[i+1].entries.size() / 4) {
                mergeLevels(i);
            }
        }
        
        // 支持定时自动刷盘
        if (flushIntervalSec > 0) {
            flush();
        }
        // 休眠，避免过度合并
        std::this_thread::sleep_for(std::chrono::seconds(flushIntervalSec > 0 ? flushIntervalSec : 10));
    }
}

Log::Result
LSMStorage::flushMemTable()
{
    Level newLevel;
    {
        Core::Mutex::Lock lock(mutex);
        if (memTable.entries.empty()) {
            return Log::Result::SUCCESS;
        }
        // 交换内存表到新层
        newLevel = std::move(memTable);
        // 初始化布隆过滤器（预留，实际实现需引入库）
        // newLevel.bloom = std::make_shared<BloomFilter>(bloomFilterFPRate, newLevel.entries.size());
        // for (const auto& e : newLevel.entries) newLevel.bloom->add(e.first);

        // 重置内存表
        memTable.entries.clear();
        memTable.min_index = std::numeric_limits<uint64_t>::max();
        memTable.max_index = 0;
    }
    // 插入到磁盘层级的最顶层
    if (diskLevels.empty()) {
        diskLevels.push_back(std::move(newLevel));
    } else {
        diskLevels.insert(diskLevels.begin(), std::move(newLevel));
    }
    
    // 写入磁盘
    return writeToDisk(diskLevels[0], 0);
}

Log::Result
LSMStorage::mergeLevels(size_t level_index)
{
    if (level_index >= diskLevels.size() || level_index + 1 >= diskLevels.size()) {
        return Log::Result::INVALID_ARGUMENT;
    }
    Level& upperLevel = diskLevels[level_index];
    Level& lowerLevel = diskLevels[level_index + 1];

    // 优化：只合并不存在于下层的 key，减少无谓覆盖
    for (const auto& entry : upperLevel.entries) {
        if (lowerLevel.entries.find(entry.first) == lowerLevel.entries.end()) {
            lowerLevel.entries[entry.first] = entry.second;
            // if (lowerLevel.bloom) lowerLevel.bloom->add(entry.first);
        }
    }
    // 更新合并后的层级范围
    lowerLevel.min_index = std::min(lowerLevel.min_index, upperLevel.min_index);
    lowerLevel.max_index = std::max(lowerLevel.max_index, upperLevel.max_index);
    
    // 删除上层
    diskLevels.erase(diskLevels.begin() + level_index);
    
    // 重写合并后的层级到磁盘
    return writeToDisk(lowerLevel, level_index);
}

Log::Result
LSMStorage::loadFromDisk()
{
    // 加载元数据
    std::string metadataPath = path + "/metadata";
    std::ifstream metadataFile(metadataPath.c_str(), std::ios::binary);
    if (metadataFile) {
        metadataFile.read(reinterpret_cast<char*>(&cachedMetadata.versionNumber),
                          sizeof(cachedMetadata.versionNumber));
        metadataFile.read(reinterpret_cast<char*>(&cachedMetadata.currentTerm),
                         sizeof(cachedMetadata.currentTerm));
        metadataFile.read(reinterpret_cast<char*>(&cachedMetadata.votedFor),
                         sizeof(cachedMetadata.votedFor));
    } else if (!initialized) {
        // 初次运行，设置默认值
        cachedMetadata.versionNumber = 1;
        cachedMetadata.currentTerm = 0;
        cachedMetadata.votedFor = 0;
    }
    
    // 清空并重加载层级数据
    diskLevels.clear();
    
    uint64_t maxIndex = 0;
    
    // 尝试加载所有层级
    uint32_t maxLevels = std::stoul(config.at("lsm_max_levels"));
    for (uint32_t i = 0; i < maxLevels; ++i) {
        Level level;
        if (readFromDisk(level, i) == Log::Result::SUCCESS && !level.entries.empty()) {
            diskLevels.push_back(std::move(level));
            maxIndex = std::max(maxIndex, level.max_index);
        }
    }
    
    // 更新最后日志索引
    if (!diskLevels.empty()) {
        lastLogIndex.store(maxIndex);
    }
    
    initialized = true;
    return Log::Result::SUCCESS;
}

Log::Result
LSMStorage::writeToDisk(const Level& level, size_t level_index)
{
    std::string levelPath = getLevelPath(level_index);
    
    // 创建临时文件
    std::string tempPath = levelPath + ".tmp";
    std::ofstream file(tempPath.c_str(), std::ios::binary);
    if (!file) {
        return Log::Result::IO_ERROR;
    }
    
    // 先写入层级元数据
    file.write(reinterpret_cast<const char*>(&level.min_index), sizeof(level.min_index));
    file.write(reinterpret_cast<const char*>(&level.max_index), sizeof(level.max_index));
    
    // 写入项目数量
    size_t count = level.entries.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    // 逐项写入
    bool compressionEnabled = (config.at("lsm_compression_enabled") == "true");
    
    for (const auto& entry : level.entries) {
        uint64_t index = entry.first;
        
        // 写入项目索引
        file.write(reinterpret_cast<const char*>(&index), sizeof(index));
        
        // 获取项目数据
        const Core::Buffer& buffer = entry.second;
        uint32_t dataSize = buffer.getLength();
        
        // 写入数据大小
        file.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
        
        // 写入数据
        file.write(static_cast<const char*>(buffer.getData()), dataSize);
    }
    
    if (file.fail()) {
        unlink(tempPath.c_str());
        return Log::Result::IO_ERROR;
    }
    
    file.close();
    
    // 原子地替换文件
    if (rename(tempPath.c_str(), levelPath.c_str()) != 0) {
        unlink(tempPath.c_str());
        return Log::Result::IO_ERROR;
    }
    
    return Log::Result::SUCCESS;
}

Log::Result
LSMStorage::readFromDisk(Level& level, size_t level_index)
{
    std::string levelPath = getLevelPath(level_index);
    std::ifstream file(levelPath.c_str(), std::ios::binary);
    if (!file) {
        return Log::Result::IO_ERROR;
    }
    
    // 读取层级元数据
    file.read(reinterpret_cast<char*>(&level.min_index), sizeof(level.min_index));
    file.read(reinterpret_cast<char*>(&level.max_index), sizeof(level.max_index));
    
    // 读取项目数量
    size_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    // 逐项读取
    for (size_t i = 0; i < count; ++i) {
        uint64_t index;
        
        // 读取项目索引
        file.read(reinterpret_cast<char*>(&index), sizeof(index));
        
        // 读取数据大小
        uint32_t dataSize;
        file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
        
        // 读取数据
        Core::Buffer buffer;
        buffer.reserve(dataSize);
        char* data = static_cast<char*>(buffer.getData());
        file.read(data, dataSize);
        buffer.setLength(dataSize);
        
        // 存储到层级
        level.entries[index] = std::move(buffer);
    }
    
    if (file.fail() && !file.eof()) {
        return Log::Result::IO_ERROR;
    }
    
    return Log::Result::SUCCESS;
}

std::string
LSMStorage::getLevelPath(size_t level_index) const
{
    return path + "/level-" + Core::StringUtil::format("%03lu", level_index);
}

// 注册存储引擎
namespace {
/**
 * 创建LSMStorage对象的工厂。
 */
class LSMStorageFactory : public LogFactory::Module {
  public:
    explicit LSMStorageFactory(std::string& name)
        : LogFactory::Module(name)
    {
    }
    
    std::unique_ptr<Log>
    makeLog(const std::string& logPath, const Log::Config& config) {
        uint64_t bufferSizeMB = 64;
        bool compressionEnabled = true;
        std::string compressionAlgorithm = "zstd";
        uint32_t maxLevels = 7;
        double bloomFilterFPRate = 0.01;
        
        if (config.find("lsm_buffer_size_mb") != config.end()) {
            bufferSizeMB = std::stoull(config.at("lsm_buffer_size_mb"));
        }
        
        if (config.find("lsm_compression_enabled") != config.end()) {
            compressionEnabled = (config.at("lsm_compression_enabled") == "true");
        }
        
        if (config.find("lsm_compression_algorithm") != config.end()) {
            compressionAlgorithm = config.at("lsm_compression_algorithm");
        }
        
        if (config.find("lsm_max_levels") != config.end()) {
            maxLevels = std::stoul(config.at("lsm_max_levels"));
        }
        
        if (config.find("lsm_bloom_filter_fp_rate") != config.end()) {
            bloomFilterFPRate = std::stod(config.at("lsm_bloom_filter_fp_rate"));
        }
        
        return std::make_unique<LSMStorage>(
            logPath,
            makeConfig(bufferSizeMB, compressionEnabled, compressionAlgorithm,
                      maxLevels, bloomFilterFPRate));
    }
};

// 静态全局变量，在程序启动时注册LSM存储模块
std::string lsmName = "LSM";
LSMStorageFactory lsmFactory(lsmName);

} // anonymous namespace

} // namespace Storage
} // namespace LogCabin
