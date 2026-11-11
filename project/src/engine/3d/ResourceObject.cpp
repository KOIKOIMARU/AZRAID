// ===== ResourceObject.cpp =====
// DirectXリソースへの参照を保持し、このオブジェクトの破棄時に解放する。

#include "engine/3d/ResourceObject.h"

ResourceObject::ResourceObject(ID3D12Resource* resource)
    : resource_(resource) {
}

// このオブジェクトが保持するDirectXリソースの参照を、破棄時に解放する。
ResourceObject::~ResourceObject() {
    if (resource_) {
        resource_->Release();
    }
}

ID3D12Resource* ResourceObject::Get() const {
    return resource_;
}
