/*******************************************************************************
 * Copyright 上海赜睿信息科技有限公司(Zilliz) - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Proprietary and confidential.
 ******************************************************************************/
#include <src/server/ServerConfig.h>
#include <src/metrics/Metrics.h>
#include "Log.h"

#include "src/cache/CpuCacheMgr.h"
#include "ExecutionEngineImpl.h"
#include "wrapper/knowhere/vec_index.h"
#include "wrapper/knowhere/vec_impl.h"


namespace zilliz {
namespace milvus {
namespace engine {

ExecutionEngineImpl::ExecutionEngineImpl(uint16_t dimension,
                                         const std::string &location,
                                         EngineType type)
    : location_(location), dim(dimension), build_type(type) {
    index_ = CreatetVecIndex(EngineType::FAISS_IDMAP);
    current_type = EngineType::FAISS_IDMAP;
    std::static_pointer_cast<BFIndex>(index_)->Build(dimension);
}

ExecutionEngineImpl::ExecutionEngineImpl(VecIndexPtr index,
                                         const std::string &location,
                                         EngineType type)
    : index_(std::move(index)), location_(location), build_type(type) {
    current_type = type;
}

VecIndexPtr ExecutionEngineImpl::CreatetVecIndex(EngineType type) {
    std::shared_ptr<VecIndex> index;
    switch (type) {
        case EngineType::FAISS_IDMAP: {
            index = GetVecIndexFactory(IndexType::FAISS_IDMAP);
            break;
        }
        case EngineType::FAISS_IVFFLAT_GPU: {
            index = GetVecIndexFactory(IndexType::FAISS_IVFFLAT_MIX);
            break;
        }
        case EngineType::FAISS_IVFFLAT_CPU: {
            index = GetVecIndexFactory(IndexType::FAISS_IVFFLAT_CPU);
            break;
        }
        case EngineType::SPTAG_KDT_RNT_CPU: {
            index = GetVecIndexFactory(IndexType::SPTAG_KDT_RNT_CPU);
            break;
        }
        default: {
            ENGINE_LOG_ERROR << "Invalid engine type";
            return nullptr;
        }
    }
    return index;
}

Status ExecutionEngineImpl::AddWithIds(long n, const float *xdata, const long *xids) {
    index_->Add(n, xdata, xids, Config::object{{"dim", dim}});
    return Status::OK();
}

size_t ExecutionEngineImpl::Count() const {
    return index_->Count();
}

size_t ExecutionEngineImpl::Size() const {
    return (size_t) (Count() * Dimension()) * sizeof(float);
}

size_t ExecutionEngineImpl::Dimension() const {
    return index_->Dimension();
}

size_t ExecutionEngineImpl::PhysicalSize() const {
    return (size_t) (Count() * Dimension()) * sizeof(float);
}

Status ExecutionEngineImpl::Serialize() {
    write_index(index_, location_);
    return Status::OK();
}

Status ExecutionEngineImpl::Load() {
    index_ = zilliz::milvus::cache::CpuCacheMgr::GetInstance()->GetIndex(location_);
    bool to_cache = false;
    auto start_time = METRICS_NOW_TIME;
    if (!index_) {
        index_ = read_index(location_);
        to_cache = true;
        ENGINE_LOG_DEBUG << "Disk io from: " << location_;
    }

    if (to_cache) {
        Cache();
        auto end_time = METRICS_NOW_TIME;
        auto total_time = METRICS_MICROSECONDS(start_time, end_time);

        server::Metrics::GetInstance().FaissDiskLoadDurationSecondsHistogramObserve(total_time);
        double total_size = Size();

        server::Metrics::GetInstance().FaissDiskLoadSizeBytesHistogramObserve(total_size);
        server::Metrics::GetInstance().FaissDiskLoadIOSpeedGaugeSet(total_size / double(total_time));
    }
    return Status::OK();
}

Status ExecutionEngineImpl::Merge(const std::string &location) {
    if (location == location_) {
        return Status::Error("Cannot Merge Self");
    }
    ENGINE_LOG_DEBUG << "Merge index file: " << location << " to: " << location_;

    auto to_merge = zilliz::milvus::cache::CpuCacheMgr::GetInstance()->GetIndex(location);
    if (!to_merge) {
        to_merge = read_index(location);
    }

    if (auto file_index = std::dynamic_pointer_cast<BFIndex>(to_merge)) {
        index_->Add(file_index->Count(), file_index->GetRawVectors(), file_index->GetRawIds());
        return Status::OK();
    } else {
        return Status::Error("file index type is not idmap");
    }
}

ExecutionEnginePtr
ExecutionEngineImpl::BuildIndex(const std::string &location) {
    ENGINE_LOG_DEBUG << "Build index file: " << location << " from: " << location_;

    auto from_index = std::dynamic_pointer_cast<BFIndex>(index_);
    auto to_index = CreatetVecIndex(build_type);
    to_index->BuildAll(Count(),
                       from_index->GetRawVectors(),
                       from_index->GetRawIds(),
                       Config::object{{"dim", Dimension()}, {"gpu_id", gpu_num}});

    return std::make_shared<ExecutionEngineImpl>(to_index, location, build_type);
}

Status ExecutionEngineImpl::Search(long n,
                                   const float *data,
                                   long k,
                                   float *distances,
                                   long *labels) const {
    index_->Search(n, data, distances, labels, Config::object{{"k", k}, {"nprobe", nprobe_}});
    return Status::OK();
}

Status ExecutionEngineImpl::Cache() {
    zilliz::milvus::cache::CpuCacheMgr::GetInstance()->InsertItem(location_, index_);

    return Status::OK();
}

Status ExecutionEngineImpl::Init() {
    using namespace zilliz::milvus::server;
    ServerConfig &config = ServerConfig::GetInstance();
    ConfigNode server_config = config.GetConfig(CONFIG_SERVER);
    gpu_num = server_config.GetInt32Value("gpu_index", 0);

    switch (build_type) {
        case EngineType::FAISS_IVFFLAT_GPU: {
        }
        case EngineType::FAISS_IVFFLAT_CPU: {
            ConfigNode engine_config = config.GetConfig(CONFIG_ENGINE);
            nprobe_ = engine_config.GetInt32Value(CONFIG_NPROBE, 1000);
            break;
        }
    }

    return Status::OK();
}


} // namespace engine
} // namespace milvus
} // namespace zilliz
